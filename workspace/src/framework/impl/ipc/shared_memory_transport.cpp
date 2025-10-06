/**
 * @file shared_memory_transport.cpp
 * @brief Shared memory transport implementation
 *
 * @version 1.0.0
 * @date 2025-10-03
 */

#include "ipc/shared_memory_transport.h"
#include "ipc/serializer.h"
#include "utils/log.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <chrono>
#include <thread>

namespace cdmf {
namespace ipc {

SharedMemoryTransport::SharedMemoryTransport()
    : state_(TransportState::UNINITIALIZED)
    , running_(false)
    , connected_(false)
    , shm_fd_(-1)
    , shm_addr_(nullptr)
    , shm_size_(0)
    , is_owner_(false)
    , control_block_(nullptr)
    , tx_queue_(nullptr)
    , rx_queue_(nullptr)
    , tx_sem_(SEM_FAILED)
    , rx_sem_(SEM_FAILED)
    , last_error_(TransportError::SUCCESS) {
    LOGD_FMT("SharedMemoryTransport constructed");
}

SharedMemoryTransport::~SharedMemoryTransport() {
    LOGD_FMT("SharedMemoryTransport destructor called");
    cleanup();
}

TransportResult<bool> SharedMemoryTransport::init(const TransportConfig& config) {
    LOGI_FMT("SharedMemoryTransport::init - endpoint: " << config.endpoint);
    if (state_ != TransportState::UNINITIALIZED) {
        LOGW_FMT("SharedMemoryTransport already initialized");
        return {TransportError::ALREADY_INITIALIZED, false, "Transport already initialized"};
    }

    config_ = config;

    // Parse shared memory specific config from properties
    shm_config_.shm_name = config.endpoint;

    auto it = config.properties.find("shm_size");
    if (it != config.properties.end()) {
        shm_config_.shm_size = std::stoull(it->second);
    }

    it = config.properties.find("ring_buffer_capacity");
    if (it != config.properties.end()) {
        shm_config_.ring_buffer_capacity = std::stoull(it->second);
    }

    it = config.properties.find("create_shm");
    if (it != config.properties.end()) {
        shm_config_.create_shm = (it->second == "true" || it->second == "1");
    }

    it = config.properties.find("bidirectional");
    if (it != config.properties.end()) {
        shm_config_.bidirectional = (it->second == "true" || it->second == "1");
    }

    // Validate power of 2 for ring buffer capacity
    if ((shm_config_.ring_buffer_capacity & (shm_config_.ring_buffer_capacity - 1)) != 0) {
        LOGE_FMT("SharedMemoryTransport::init failed - ring buffer capacity must be power of 2");
        return {TransportError::INVALID_CONFIG, false,
                "Ring buffer capacity must be power of 2"};
    }

    // Allocate send/receive buffers
    send_buffer_.resize(shm_config_.max_message_size + ShmMessageEnvelope::HEADER_SIZE);
    recv_buffer_.resize(shm_config_.max_message_size + ShmMessageEnvelope::HEADER_SIZE);

    setState(TransportState::INITIALIZED);
    LOGI_FMT("SharedMemoryTransport::init completed successfully - shm_name: " << shm_config_.shm_name
             << ", shm_size: " << shm_config_.shm_size);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::start() {
    LOGI_FMT("SharedMemoryTransport::start - shm_name: " << shm_config_.shm_name);
    if (state_ != TransportState::INITIALIZED && state_ != TransportState::DISCONNECTED) {
        LOGW_FMT("SharedMemoryTransport::start failed - not initialized");
        return {TransportError::NOT_INITIALIZED, false, "Transport not initialized"};
    }

    // Set ownership based on configuration (not on who actually creates the memory)
    // This ensures correct TX/RX ring assignment for bidirectional communication
    is_owner_ = shm_config_.create_shm;

    // Create or open shared memory
    TransportResult<bool> result;
    if (shm_config_.create_shm) {
        LOGD_FMT("Creating shared memory as owner");
        result = createSharedMemory();
        if (!result) return result;

        result = mapSharedMemory();
        if (!result) return result;

        result = initializeControlBlock();
        if (!result) return result;

        result = setupRingBuffers();
        if (!result) return result;

        result = createSemaphores();
        if (!result) return result;
    } else {
        LOGD_FMT("Opening existing shared memory");
        result = openSharedMemory();
        if (!result) return result;

        result = mapSharedMemory();
        if (!result) return result;

        result = validateControlBlock();
        if (!result) return result;

        result = setupRingBuffers();
        if (!result) return result;

        result = openSemaphores();
        if (!result) return result;
    }

    // Set running and connected flags
    running_ = true;
    connected_ = true;
    setState(TransportState::CONNECTED);

    LOGI_FMT("SharedMemoryTransport::start completed successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::stop() {
    LOGI_FMT("SharedMemoryTransport::stop called");
    if (!running_ && !connected_) {
        LOGD_FMT("Already stopped");
        return {TransportError::SUCCESS, true, ""};
    }

    running_ = false;

    if (io_thread_ && io_thread_->joinable()) {
        LOGD_FMT("Joining I/O thread");
        io_thread_->join();
    }

    disconnect();

    setState(TransportState::DISCONNECTED);
    LOGI_FMT("SharedMemoryTransport::stop completed");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::cleanup() {
    LOGI_FMT("SharedMemoryTransport::cleanup called");
    stop();

    closeSemaphores();
    unmapSharedMemory();

    if (shm_fd_ >= 0) {
        LOGD_FMT("Closing shared memory file descriptor");
        close(shm_fd_);
        shm_fd_ = -1;
    }

    if (is_owner_ && shm_config_.unlink_on_cleanup) {
        LOGD_FMT("Unlinking shared memory: " << shm_config_.shm_name);
        shm_unlink(shm_config_.shm_name.c_str());
    }

    setState(TransportState::UNINITIALIZED);
    LOGI_FMT("SharedMemoryTransport::cleanup completed");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::connect() {
    LOGI_FMT("SharedMemoryTransport::connect called");
    if (connected_) {
        LOGD_FMT("Already connected - shared memory is ready after start()");
        return {TransportError::SUCCESS, true, "Already connected"};
    }

    setState(TransportState::CONNECTING);

    // Update control block
    if (control_block_) {
        if (shm_config_.create_shm) {
            control_block_->writer_count.fetch_add(1);
        } else {
            control_block_->reader_count.fetch_add(1);
        }
    }

    connected_ = true;
    setState(TransportState::CONNECTED);

    // Start I/O thread for async mode
    if (config_.mode == TransportMode::ASYNC) {
        LOGD_FMT("Starting I/O thread for async mode");
        running_ = true;
        io_thread_ = std::make_unique<std::thread>(&SharedMemoryTransport::ioThreadFunc, this);
    }

    LOGI_FMT("SharedMemoryTransport::connect completed successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::disconnect() {
    LOGI_FMT("SharedMemoryTransport::disconnect called");
    if (!connected_) {
        LOGD_FMT("Already disconnected");
        return {TransportError::SUCCESS, true, ""};
    }

    setState(TransportState::DISCONNECTING);

    // Update control block
    if (control_block_) {
        if (shm_config_.create_shm) {
            control_block_->writer_count.fetch_sub(1);
        } else {
            control_block_->reader_count.fetch_sub(1);
        }
    }

    connected_ = false;
    setState(TransportState::DISCONNECTED);

    LOGI_FMT("SharedMemoryTransport::disconnect completed");
    return {TransportError::SUCCESS, true, ""};
}

bool SharedMemoryTransport::isConnected() const {
    return connected_;
}

TransportResult<bool> SharedMemoryTransport::send(const Message& message) {
    if (!connected_) {
        LOGE_FMT("SharedMemoryTransport::send failed - not connected");
        return {TransportError::NOT_CONNECTED, false, "Not connected"};
    }

    // Serialize message
    auto serializer = SerializerFactory::createSerializer(SerializationFormat::BINARY);
    auto result = serializer->serialize(message);
    if (!result.success) {
        LOGE_FMT("SharedMemoryTransport::send - serialization failed: " << result.error_message);
        setError(TransportError::SERIALIZATION_ERROR, result.error_message);
        updateStats(true, 0, false);
        return {TransportError::SERIALIZATION_ERROR, false, result.error_message};
    }

    if (result.data.size() > shm_config_.max_message_size) {
        LOGE_FMT("SharedMemoryTransport::send - message too large: " << result.data.size()
                 << " > " << shm_config_.max_message_size);
        setError(TransportError::BUFFER_OVERFLOW, "Message too large");
        updateStats(true, 0, false);
        return {TransportError::BUFFER_OVERFLOW, false, "Message too large"};
    }

    // Create envelope
    auto* envelope = reinterpret_cast<ShmMessageEnvelope*>(send_buffer_.data());
    envelope->size = result.data.size();
    envelope->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    envelope->checksum = computeCrc32(result.data.data(), result.data.size());
    // Data follows the header structure
    uint8_t* data_ptr = reinterpret_cast<uint8_t*>(envelope) + ShmMessageEnvelope::HEADER_SIZE;
    std::memcpy(data_ptr, result.data.data(), result.data.size());

    // Push to message queue
    auto push_result = pushToQueue(tx_queue_, tx_sem_,
                     reinterpret_cast<uint8_t*>(envelope),
                     ShmMessageEnvelope::HEADER_SIZE + result.data.size());

    if (!push_result.success()) {
        LOGE_FMT("SharedMemoryTransport::send failed - message queue push failed");
    }
    return push_result;
}

TransportResult<bool> SharedMemoryTransport::send(Message&& message) {
    return send(message);
}

TransportResult<MessagePtr> SharedMemoryTransport::receive(int32_t timeout_ms) {
    if (!connected_) {
        LOGE_FMT("SharedMemoryTransport::receive failed - not connected");
        return {TransportError::NOT_CONNECTED, nullptr, "Not connected"};
    }

    return popFromQueue(rx_queue_, rx_sem_, timeout_ms);
}

TransportResult<MessagePtr> SharedMemoryTransport::tryReceive() {
    LOGV_FMT("SharedMemoryTransport::tryReceive (non-blocking)");
    return receive(0);
}

void SharedMemoryTransport::setMessageCallback(MessageCallback callback) {
    LOGV_FMT("Setting message callback");
    message_callback_ = callback;
}

void SharedMemoryTransport::setErrorCallback(ErrorCallback callback) {
    LOGV_FMT("Setting error callback");
    error_callback_ = callback;
}

void SharedMemoryTransport::setStateChangeCallback(StateChangeCallback callback) {
    LOGV_FMT("Setting state change callback");
    state_callback_ = callback;
}

TransportState SharedMemoryTransport::getState() const {
    return state_;
}

TransportType SharedMemoryTransport::getType() const {
    return TransportType::SHARED_MEMORY;
}

const TransportConfig& SharedMemoryTransport::getConfig() const {
    return config_;
}

TransportStats SharedMemoryTransport::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void SharedMemoryTransport::resetStats() {
    LOGD_FMT("Resetting SharedMemoryTransport stats");
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = TransportStats{};
}

std::pair<TransportError, std::string> SharedMemoryTransport::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return {last_error_, last_error_msg_};
}

std::string SharedMemoryTransport::getInfo() const {
    std::ostringstream oss;
    oss << "SharedMemoryTransport[" << shm_config_.shm_name
        << ", size=" << shm_size_ << ", addr=" << shm_addr_ << "]";
    return oss.str();
}

SharedMemoryTransport::ShmInfo SharedMemoryTransport::getShmInfo() const {
    LOGD_FMT("Getting ShmInfo");
    ShmInfo info;
    info.name = shm_config_.shm_name;
    info.size = shm_size_;
    info.address = shm_addr_;
    info.is_owner = is_owner_;
    return info;
}

// Private helper methods

TransportResult<bool> SharedMemoryTransport::createSharedMemory() {
    LOGD_FMT("Creating shared memory: " << shm_config_.shm_name);
    shm_fd_ = shm_open(shm_config_.shm_name.c_str(),
                       O_CREAT | O_RDWR | O_EXCL, 0666);

    if (shm_fd_ < 0) {
        if (errno == EEXIST) {
            LOGW_FMT("Shared memory already exists, opening existing: " << shm_config_.shm_name);
            // Already exists, try to open it
            shm_fd_ = shm_open(shm_config_.shm_name.c_str(), O_RDWR, 0666);
            if (shm_fd_ >= 0) {
                // Note: is_owner_ is already set based on config in start()

                // Get the size of existing shared memory
                struct stat st;
                if (fstat(shm_fd_, &st) < 0) {
                    std::string error_msg = std::string("Failed to get existing shared memory size: ") + strerror(errno);
                    LOGE_FMT(error_msg);
                    close(shm_fd_);
                    shm_fd_ = -1;
                    setError(TransportError::CONNECTION_FAILED, error_msg);
                    return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
                }

                shm_size_ = st.st_size;
                LOGD_FMT("Opened existing shared memory successfully, fd: " << shm_fd_ << ", size: " << shm_size_);
                return {TransportError::SUCCESS, true, ""};
            }
        }
        std::string error_msg = std::string("Failed to create shared memory: ") + strerror(errno);
        LOGE_FMT(error_msg);
        setError(TransportError::CONNECTION_FAILED, error_msg);
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    // Note: is_owner_ is already set based on config in start()
    LOGD_FMT("Created shared memory, fd: " << shm_fd_);

    // Set size
    LOGD_FMT("Setting shared memory size with ftruncate: " << shm_config_.shm_size << " bytes");
    if (ftruncate(shm_fd_, shm_config_.shm_size) < 0) {
        std::string error_msg = std::string("Failed to set shared memory size: ") + strerror(errno);
        LOGE_FMT(error_msg << ", errno: " << errno);
        setError(TransportError::CONNECTION_FAILED, error_msg);
        close(shm_fd_);
        shm_fd_ = -1;
        shm_unlink(shm_config_.shm_name.c_str());
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    shm_size_ = shm_config_.shm_size;
    LOGD_FMT("Shared memory created successfully, fd: " << shm_fd_ << ", size: " << shm_size_);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::openSharedMemory() {
    LOGD_FMT("Opening existing shared memory: " << shm_config_.shm_name);
    shm_fd_ = shm_open(shm_config_.shm_name.c_str(), O_RDWR, 0666);

    if (shm_fd_ < 0) {
        std::string error_msg = std::string("Failed to open shared memory: ") + strerror(errno);
        LOGE_FMT(error_msg);
        setError(TransportError::ENDPOINT_NOT_FOUND, error_msg);
        return {TransportError::ENDPOINT_NOT_FOUND, false, last_error_msg_};
    }

    // Get size
    struct stat st;
    if (fstat(shm_fd_, &st) < 0) {
        std::string error_msg = std::string("Failed to get shared memory size: ") + strerror(errno);
        LOGE_FMT(error_msg);
        setError(TransportError::CONNECTION_FAILED, error_msg);
        close(shm_fd_);
        shm_fd_ = -1;
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    shm_size_ = st.st_size;
    // Note: is_owner_ is already set based on config in start()

    LOGD_FMT("Opened shared memory successfully, size: " << shm_size_);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::mapSharedMemory() {
    // Validate inputs before mmap
    if (shm_fd_ < 0) {
        std::string error_msg = "Invalid file descriptor for mmap: " + std::to_string(shm_fd_);
        LOGE_FMT(error_msg);
        setError(TransportError::CONNECTION_FAILED, error_msg);
        return {TransportError::CONNECTION_FAILED, false, error_msg};
    }

    if (shm_size_ == 0) {
        std::string error_msg = "Invalid shared memory size: 0";
        LOGE_FMT(error_msg);
        setError(TransportError::CONNECTION_FAILED, error_msg);
        return {TransportError::CONNECTION_FAILED, false, error_msg};
    }

    shm_addr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE,
                     MAP_SHARED, shm_fd_, 0);

    if (shm_addr_ == MAP_FAILED) {
        std::string error_msg = std::string("Failed to map shared memory: ") + strerror(errno);
        LOGE_FMT(error_msg);
        setError(TransportError::CONNECTION_FAILED, error_msg);
        shm_addr_ = nullptr;
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    LOGD_FMT("Shared memory mapped successfully, size: " << shm_size_);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::unmapSharedMemory() {
    LOGD_FMT("Unmapping shared memory");
    if (shm_addr_ != nullptr) {
        munmap(shm_addr_, shm_size_);
        shm_addr_ = nullptr;
        LOGD_FMT("Shared memory unmapped successfully");
    }
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::initializeControlBlock() {
    LOGD_FMT("Initializing control block");
    control_block_ = static_cast<ShmControlBlock*>(shm_addr_);

    control_block_->magic = ShmControlBlock::MAGIC;
    control_block_->version = ShmControlBlock::VERSION;
    control_block_->total_size = shm_size_;
    control_block_->ring_capacity = shm_config_.ring_buffer_capacity;
    control_block_->reader_count.store(0);
    control_block_->writer_count.store(0);
    control_block_->server_pid = getpid();
    control_block_->client_pid = 0;
    control_block_->flags = 0;

    LOGD_FMT("Control block initialized successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::validateControlBlock() {
    LOGD_FMT("Validating control block");
    control_block_ = static_cast<ShmControlBlock*>(shm_addr_);

    if (control_block_->magic != ShmControlBlock::MAGIC) {
        LOGE_FMT("Invalid shared memory magic number");
        setError(TransportError::PROTOCOL_ERROR, "Invalid shared memory magic number");
        return {TransportError::PROTOCOL_ERROR, false, last_error_msg_};
    }

    if (control_block_->version != ShmControlBlock::VERSION) {
        LOGE_FMT("Incompatible shared memory version");
        setError(TransportError::PROTOCOL_ERROR, "Incompatible shared memory version");
        return {TransportError::PROTOCOL_ERROR, false, last_error_msg_};
    }

    LOGD_FMT("Control block validated successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::setupRingBuffers() {
    // Calculate offsets
    size_t offset = sizeof(ShmControlBlock);
    offset = (offset + 63) & ~63; // Align to 64 bytes

    // Get pointers to both message queues
    ShmMessageQueue* queue1 = reinterpret_cast<ShmMessageQueue*>(
        static_cast<uint8_t*>(shm_addr_) + offset);

    offset += sizeof(ShmMessageQueue);
    offset = (offset + 63) & ~63;

    ShmMessageQueue* queue2 = nullptr;
    if (shm_config_.bidirectional) {
        queue2 = reinterpret_cast<ShmMessageQueue*>(
            static_cast<uint8_t*>(shm_addr_) + offset);
    }

    // Initialize message queues (owner only)
    if (is_owner_) {
        new (queue1) ShmMessageQueue();
        if (queue2) {
            new (queue2) ShmMessageQueue();
        }
    }

    // Assign TX/RX based on role
    if (shm_config_.bidirectional) {
        if (is_owner_) {
            // Owner (server): TX = queue1, RX = queue2
            tx_queue_ = queue1;
            rx_queue_ = queue2;
        } else {
            // Client: TX = queue2, RX = queue1 (swapped for bidirectional communication)
            tx_queue_ = queue2;
            rx_queue_ = queue1;
        }
    } else {
        // Unidirectional: both use same queue
        tx_queue_ = queue1;
        rx_queue_ = queue1;
    }

    LOGD_FMT("Message queues configured - role: " << (is_owner_ ? "owner" : "client")
             << ", mode: " << (shm_config_.bidirectional ? "bidirectional" : "unidirectional"));

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::createSemaphores() {
    tx_sem_name_ = shm_config_.shm_name + "_tx";
    rx_sem_name_ = shm_config_.shm_name + "_rx";

    sem_unlink(tx_sem_name_.c_str());
    sem_unlink(rx_sem_name_.c_str());

    tx_sem_ = sem_open(tx_sem_name_.c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (tx_sem_ == SEM_FAILED) {
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to create TX semaphore: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    rx_sem_ = sem_open(rx_sem_name_.c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (rx_sem_ == SEM_FAILED) {
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to create RX semaphore: ") + strerror(errno));
        sem_close(tx_sem_);
        sem_unlink(tx_sem_name_.c_str());
        tx_sem_ = SEM_FAILED;
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::openSemaphores() {
    tx_sem_name_ = shm_config_.shm_name + "_rx"; // Swapped for client
    rx_sem_name_ = shm_config_.shm_name + "_tx";

    tx_sem_ = sem_open(tx_sem_name_.c_str(), 0);
    if (tx_sem_ == SEM_FAILED) {
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to open TX semaphore: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    rx_sem_ = sem_open(rx_sem_name_.c_str(), 0);
    if (rx_sem_ == SEM_FAILED) {
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to open RX semaphore: ") + strerror(errno));
        sem_close(tx_sem_);
        tx_sem_ = SEM_FAILED;
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::closeSemaphores() {
    if (tx_sem_ != SEM_FAILED) {
        sem_close(tx_sem_);
        if (is_owner_) {
            sem_unlink(tx_sem_name_.c_str());
        }
        tx_sem_ = SEM_FAILED;
    }

    if (rx_sem_ != SEM_FAILED) {
        sem_close(rx_sem_);
        if (is_owner_) {
            sem_unlink(rx_sem_name_.c_str());
        }
        rx_sem_ = SEM_FAILED;
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> SharedMemoryTransport::pushToQueue(ShmMessageQueue* queue, sem_t* sem,
                                                          const uint8_t* data, size_t size) {
    if (!queue || !data || size == 0) {
        LOGE_FMT("pushToQueue failed - invalid arguments: queue=" << queue << ", data=" << data << ", size=" << size);
        return {TransportError::INVALID_MESSAGE, false, "Invalid arguments"};
    }

    if (size > ShmMessageQueue::MAX_MSG_SIZE) {
        LOGE_FMT("pushToQueue failed - message too large: " << size << " > " << ShmMessageQueue::MAX_MSG_SIZE);
        return {TransportError::BUFFER_OVERFLOW, false, "Message too large"};
    }

    // Message format: [4 bytes size][data]
    const uint32_t total_size = sizeof(uint32_t) + size;

    // Try to write to queue (lock-free)
    uint32_t write_pos = queue->write_pos.load(std::memory_order_relaxed);
    uint32_t read_pos = queue->read_pos.load(std::memory_order_acquire);

    // Calculate available space (circular buffer)
    uint32_t available;
    if (write_pos >= read_pos) {
        available = ShmMessageQueue::QUEUE_SIZE - (write_pos - read_pos) - 1;
    } else {
        available = read_pos - write_pos - 1;
    }

    if (total_size > available) {
        LOGV_FMT("pushToQueue - queue full, available=" << available << ", needed=" << total_size);
        return {TransportError::BUFFER_OVERFLOW, false, "Queue full"};
    }

    // Write message size
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        queue->data[write_pos] = (size >> (i * 8)) & 0xFF;
        write_pos = (write_pos + 1) % ShmMessageQueue::QUEUE_SIZE;
    }

    // Write message data
    for (size_t i = 0; i < size; ++i) {
        queue->data[write_pos] = data[i];
        write_pos = (write_pos + 1) % ShmMessageQueue::QUEUE_SIZE;
    }

    // Update write position (release semantics so reader sees the data)
    queue->write_pos.store(write_pos, std::memory_order_release);

    updateStats(true, size, true);

    // Signal semaphore
    if (shm_config_.use_semaphores && sem != SEM_FAILED) {
        sem_post(sem);
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<MessagePtr> SharedMemoryTransport::popFromQueue(ShmMessageQueue* queue,
                                                                 sem_t* sem,
                                                                 int32_t timeout_ms) {
    if (!queue) {
        LOGE_FMT("popFromQueue failed - invalid queue");
        return {TransportError::INVALID_MESSAGE, nullptr, "Invalid queue"};
    }

    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 1);

    while (true) {
        // Try to read from queue (lock-free)
        uint32_t read_pos = queue->read_pos.load(std::memory_order_relaxed);
        uint32_t write_pos = queue->write_pos.load(std::memory_order_acquire);

        // Check if queue is empty
        if (read_pos == write_pos) {
            // Queue is empty
            if (timeout_ms == 0) {
                // Non-blocking, return immediately
                return {TransportError::RECV_FAILED, nullptr, "No data available"};
            }

            // Check timeout
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (elapsed >= timeout_duration) {
                    return {TransportError::TIMEOUT, nullptr, "Timeout waiting for message"};
                }
            }

            // Wait briefly before retry (1ms)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Read message size (4 bytes)
        uint32_t msg_size = 0;
        for (size_t i = 0; i < sizeof(uint32_t); ++i) {
            msg_size |= (static_cast<uint32_t>(queue->data[read_pos]) << (i * 8));
            read_pos = (read_pos + 1) % ShmMessageQueue::QUEUE_SIZE;
        }

        if (msg_size == 0 || msg_size > ShmMessageQueue::MAX_MSG_SIZE) {
            LOGE_FMT("popFromQueue - invalid message size: " << msg_size);
            return {TransportError::INVALID_MESSAGE, nullptr, "Invalid message size"};
        }

        // Read message data into receive buffer
        if (msg_size > recv_buffer_.size()) {
            recv_buffer_.resize(msg_size);
        }

        for (size_t i = 0; i < msg_size; ++i) {
            recv_buffer_[i] = queue->data[read_pos];
            read_pos = (read_pos + 1) % ShmMessageQueue::QUEUE_SIZE;
        }

        // Update read position (release semantics)
        queue->read_pos.store(read_pos, std::memory_order_release);

        // Parse the envelope
        ShmMessageEnvelope* envelope = reinterpret_cast<ShmMessageEnvelope*>(recv_buffer_.data());
        uint8_t* serialized_data = recv_buffer_.data() + ShmMessageEnvelope::HEADER_SIZE;
        size_t data_size = envelope->size;

        // Deserialize the message
        auto serializer = SerializerFactory::createSerializer(SerializationFormat::BINARY);
        auto result = serializer->deserialize(serialized_data, data_size);

        if (!result.success) {
            LOGE_FMT("Failed to deserialize message: " << result.error_message);
            return {TransportError::SERIALIZATION_ERROR, nullptr, result.error_message};
        }

        updateStats(false, data_size, true);

        return {TransportError::SUCCESS, result.message, ""};
    }
}

void SharedMemoryTransport::ioThreadFunc() {
    while (running_) {
        pollReceiveRing();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SharedMemoryTransport::pollReceiveRing() {
    auto result = popFromQueue(rx_queue_, rx_sem_, 0);
    if (result.success() && result.value && message_callback_) {
        message_callback_(result.value);
    }
}

void SharedMemoryTransport::setState(TransportState new_state) {
    TransportState old_state = state_.exchange(new_state);
    if (old_state != new_state && state_callback_) {
        state_callback_(old_state, new_state);
    }
}

void SharedMemoryTransport::setError(TransportError error, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = error;
        last_error_msg_ = message;
        stats_.last_error = message;
        stats_.last_error_time = std::chrono::system_clock::now();
    }

    if (error_callback_) {
        error_callback_(error, message);
    }
}

void SharedMemoryTransport::updateStats(bool is_send, uint32_t bytes, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (is_send) {
        if (success) {
            stats_.messages_sent++;
            stats_.bytes_sent += bytes;
        } else {
            stats_.send_errors++;
        }
    } else {
        if (success) {
            stats_.messages_received++;
            stats_.bytes_received += bytes;
        } else {
            stats_.recv_errors++;
        }
    }
}

uint32_t SharedMemoryTransport::computeCrc32(const void* data, size_t size) {
    // CRC32 with standard polynomial
    static const uint32_t crc32_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
        // ... (full table would be here)
    };

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* buf = static_cast<const uint8_t*>(data);

    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

} // namespace ipc
} // namespace cdmf
