#include "shm_consumer_service_impl.h"
#include "utils/log.h"
#include <sstream>
#include <cstring>
#include <chrono>

namespace cdmf::services {

ShmConsumerServiceImpl::ShmConsumerServiceImpl() {
    LOGI("ShmConsumerServiceImpl constructed");
}

ShmConsumerServiceImpl::~ShmConsumerServiceImpl() {
    if (running_) {
        stop();
    }
    LOGI("ShmConsumerServiceImpl destroyed");
}

void ShmConsumerServiceImpl::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        LOGW("ShmConsumerService already running");
        return;
    }

    // TODO: Initialize shared memory ring buffer here
    // shm_handle_ = open_shm(...);

    // Configure IPC proxy to connect to producer
    using namespace cdmf::ipc;
    ProxyConfig config;
    config.service_name = "ShmConsumerService";
    config.default_timeout_ms = 5000;  // 5 second timeout
    config.auto_reconnect = true;

    // Configure shared memory transport (must match producer!)
    config.transport_config.type = TransportType::SHARED_MEMORY;
    config.transport_config.endpoint = "/cdmf_shm_producer_service";
    config.transport_config.properties["create_shm"] = "false";  // Consumer opens existing shared memory
    config.transport_config.properties["bidirectional"] = "true";  // Enable bidirectional communication
    config.serialization_format = SerializationFormat::BINARY;

    // Create proxy (connection will be attempted later or via auto-reconnect)
    producer_proxy_ = std::make_shared<ServiceProxy>(config);

    // Try to connect, but don't fail if producer isn't ready yet
    if (!producer_proxy_->connect()) {
        LOGW("Failed to connect to ShmProducerService initially - will retry via auto-reconnect");
        // Don't return here - continue starting the service
    } else {
        LOGI("Successfully connected to ShmProducerService via IPC");
    }

    running_ = true;

    // Start consumer thread
    consumer_thread_ = std::thread(&ShmConsumerServiceImpl::consumerThreadFunc, this);

    LOGI("ShmConsumerService started (auto-reconnect enabled)");
}

void ShmConsumerServiceImpl::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }

    // Wait for consumer thread to finish
    if (consumer_thread_.joinable()) {
        consumer_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Disconnect from producer
        if (producer_proxy_) {
            producer_proxy_->disconnect();
        }

        // TODO: Cleanup shared memory resources
        // if (shm_handle_) { close_shm(shm_handle_); }
    }

    LOGI("ShmConsumerService stopped");
}

bool ShmConsumerServiceImpl::read(void* buffer, size_t buffer_size, size_t& bytes_read) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        LOGE("ShmConsumerService not running");
        read_failures_++;
        return false;
    }

    if (!buffer || buffer_size == 0) {
        LOGE("Invalid read parameters");
        read_failures_++;
        return false;
    }

    if (!producer_proxy_) {
        LOGE("Producer proxy not initialized");
        read_failures_++;
        return false;
    }

    // Call producer's getAvailableSpace via IPC to check if data is available
    std::vector<uint8_t> empty;
    auto space_result = producer_proxy_->call("getAvailableSpace", empty, 1000);

    if (!space_result.success) {
        LOGE_FMT("Failed to get available space: " << space_result.error_message);
        read_failures_++;
        return false;
    }

    // For now, simulate reading by getting producer status
    // In a real implementation, you would call a "getData" method
    auto result = producer_proxy_->call("getStatus", empty, 1000);

    if (!result.success) {
        LOGE_FMT("Failed to read data via IPC: " << result.error_message);
        read_failures_++;
        return false;
    }

    // Copy response data to buffer
    bytes_read = std::min(buffer_size, result.data.size());
    if (bytes_read > 0) {
        std::memcpy(buffer, result.data.data(), bytes_read);
        read_count_++;
        bytes_read_ += bytes_read;
    }

    return true;
}

void ShmConsumerServiceImpl::setDataCallback(DataCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_callback_ = std::move(callback);
    LOGI("Data callback registered");
}

size_t ShmConsumerServiceImpl::getAvailableData() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_ || !producer_proxy_) {
        return 0;
    }

    // Query producer's available space via IPC
    std::vector<uint8_t> empty;
    auto result = producer_proxy_->call("getAvailableSpace", empty, 1000);

    if (!result.success) {
        LOGE_FMT("Failed to get available data: " << result.error_message);
        return 0;
    }

    // Deserialize size_t from response
    if (result.data.size() >= sizeof(size_t)) {
        size_t available;
        std::memcpy(&available, result.data.data(), sizeof(size_t));
        return available;
    }

    return 0;
}

std::string ShmConsumerServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string local_status = running_ ? "Running" : "Stopped";

    // If running, also get producer status via IPC
    if (running_ && producer_proxy_) {
        std::vector<uint8_t> empty;
        auto result = producer_proxy_->call("getStatus", empty, 1000);

        if (result.success) {
            std::string producer_status(result.data.begin(), result.data.end());
            return local_status + " (Producer: " + producer_status + ")";
        }
    }

    return local_status;
}

std::string ShmConsumerServiceImpl::getStatistics() const {
    std::ostringstream oss;
    oss << "Shared Memory Consumer Statistics:\n"
        << "  Status: " << getStatus() << "\n"
        << "  Total Reads: " << read_count_.load() << "\n"
        << "  Bytes Read: " << bytes_read_.load() << "\n"
        << "  Read Failures: " << read_failures_.load() << "\n"
        << "  Available Data: " << getAvailableData() << " bytes";
    return oss.str();
}

void ShmConsumerServiceImpl::consumerThreadFunc() {
    LOGI("Consumer thread started");

    const size_t BUFFER_SIZE = 4096;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    int reconnect_attempts = 0;

    while (running_) {
        // Try to reconnect if not connected
        if (!producer_proxy_->isConnected() && reconnect_attempts < 10) {
            LOGI_FMT("Attempting to reconnect to producer (attempt " << (reconnect_attempts + 1) << "/10)");
            if (producer_proxy_->connect()) {
                LOGI("Successfully reconnected to ShmProducerService");
                reconnect_attempts = 0;  // Reset counter on success
            } else {
                reconnect_attempts++;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
        }

        size_t bytes_read = 0;

        // Try to read data
        if (read(buffer.data(), BUFFER_SIZE, bytes_read)) {
            if (bytes_read > 0) {
                // Invoke callback if registered
                std::lock_guard<std::mutex> lock(mutex_);
                if (data_callback_) {
                    data_callback_(buffer.data(), bytes_read);
                }
            } else {
                // No data available, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            // Read error, sleep before retry
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    LOGI("Consumer thread stopped");
}

}
