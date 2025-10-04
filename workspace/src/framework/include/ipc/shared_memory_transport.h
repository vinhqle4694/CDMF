/**
 * @file shared_memory_transport.h
 * @brief Shared memory transport implementation
 *
 * Provides zero-copy IPC transport using POSIX shared memory (shm_open/mmap)
 * and ring buffers for message queuing. Uses semaphores for synchronization.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Transport Agent
 */

#ifndef CDMF_IPC_SHARED_MEMORY_TRANSPORT_H
#define CDMF_IPC_SHARED_MEMORY_TRANSPORT_H

#include "transport.h"
#include "ring_buffer.h"
#include <semaphore.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstring>

namespace cdmf {
namespace ipc {

/**
 * @brief Shared memory specific configuration
 */
struct SharedMemoryConfig {
    /** Shared memory segment name (must start with '/') */
    std::string shm_name;

    /** Shared memory size in bytes */
    size_t shm_size = 4 * 1024 * 1024; // 4 MB default

    /** Ring buffer capacity (must be power of 2) */
    size_t ring_buffer_capacity = 4096;

    /** Create shared memory (server/producer) or open existing (client/consumer) */
    bool create_shm = false;

    /** Use bidirectional communication (two ring buffers) */
    bool bidirectional = true;

    /** Enable semaphore-based blocking operations */
    bool use_semaphores = true;

    /** Semaphore timeout (milliseconds) */
    uint32_t semaphore_timeout_ms = 5000;

    /** Unlink shared memory on cleanup */
    bool unlink_on_cleanup = true;

    /** Maximum message size for ring buffer */
    size_t max_message_size = 65536; // 64 KB

    SharedMemoryConfig() = default;
};

/**
 * @brief Message envelope for shared memory transport
 *
 * Wrapper around serialized message data stored in ring buffer
 */
struct ShmMessageEnvelope {
    uint32_t size;           // Message size in bytes
    uint64_t timestamp;      // Timestamp
    uint32_t checksum;       // CRC32 of data
    // Variable-length data follows this header
    // Note: Flexible array member must be accessed via pointer arithmetic

    static constexpr size_t HEADER_SIZE = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);
} __attribute__((packed));

/**
 * @brief Shared memory control block
 *
 * Stored at the beginning of shared memory segment
 */
struct ShmControlBlock {
    /** Magic number for validation */
    uint32_t magic;

    /** Protocol version */
    uint32_t version;

    /** Total shared memory size */
    size_t total_size;

    /** Ring buffer capacity */
    size_t ring_capacity;

    /** Number of active readers */
    std::atomic<uint32_t> reader_count;

    /** Number of active writers */
    std::atomic<uint32_t> writer_count;

    /** Server PID */
    pid_t server_pid;

    /** Client PID */
    pid_t client_pid;

    /** Flags */
    uint32_t flags;

    static constexpr uint32_t MAGIC = 0xCDAF5000;  // Magic number for shared memory (CDAF = CDMF-like)
    static constexpr uint32_t VERSION = 1;
} __attribute__((aligned(64)));

/**
 * @brief Shared memory transport implementation
 */
class SharedMemoryTransport : public ITransport {
public:
    /**
     * @brief Constructor
     */
    SharedMemoryTransport();

    /**
     * @brief Destructor
     */
    ~SharedMemoryTransport() override;

    // ITransport interface implementation

    TransportResult<bool> init(const TransportConfig& config) override;
    TransportResult<bool> start() override;
    TransportResult<bool> stop() override;
    TransportResult<bool> cleanup() override;

    TransportResult<bool> connect() override;
    TransportResult<bool> disconnect() override;
    bool isConnected() const override;

    TransportResult<bool> send(const Message& message) override;
    TransportResult<bool> send(Message&& message) override;
    TransportResult<MessagePtr> receive(int32_t timeout_ms = 0) override;
    TransportResult<MessagePtr> tryReceive() override;

    void setMessageCallback(MessageCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;
    void setStateChangeCallback(StateChangeCallback callback) override;

    TransportState getState() const override;
    TransportType getType() const override;
    const TransportConfig& getConfig() const override;
    TransportStats getStats() const override;
    void resetStats() override;
    std::pair<TransportError, std::string> getLastError() const override;
    std::string getInfo() const override;

    // Shared memory specific methods

    /**
     * @brief Get shared memory configuration
     */
    const SharedMemoryConfig& getShmConfig() const { return shm_config_; }

    /**
     * @brief Get shared memory segment info
     */
    struct ShmInfo {
        std::string name;
        size_t size;
        void* address;
        bool is_owner;
    };
    ShmInfo getShmInfo() const;

private:
    // Configuration
    TransportConfig config_;
    SharedMemoryConfig shm_config_;

    // State
    std::atomic<TransportState> state_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;

    // Shared memory
    int shm_fd_;
    void* shm_addr_;
    size_t shm_size_;
    bool is_owner_; // Created the shared memory

    // Control block and ring buffers in shared memory
    ShmControlBlock* control_block_;

    // Ring buffer for messages (stored as byte arrays)
    using ByteRingBuffer = SPSCRingBuffer<uint8_t*, 4096>;
    ByteRingBuffer* tx_ring_;  // Transmit ring buffer
    ByteRingBuffer* rx_ring_;  // Receive ring buffer

    // Semaphores for blocking operations
    sem_t* tx_sem_;  // Signal when data available in tx
    sem_t* rx_sem_;  // Signal when data available in rx
    std::string tx_sem_name_;
    std::string rx_sem_name_;

    // I/O thread (async mode)
    std::unique_ptr<std::thread> io_thread_;

    // Callbacks
    MessageCallback message_callback_;
    ErrorCallback error_callback_;
    StateChangeCallback state_callback_;

    // Statistics
    mutable std::mutex stats_mutex_;
    TransportStats stats_;

    // Last error
    mutable std::mutex error_mutex_;
    TransportError last_error_;
    std::string last_error_msg_;

    // Memory pool for message envelopes
    std::vector<uint8_t> send_buffer_;
    std::vector<uint8_t> recv_buffer_;

    // Helper methods
    TransportResult<bool> createSharedMemory();
    TransportResult<bool> openSharedMemory();
    TransportResult<bool> mapSharedMemory();
    TransportResult<bool> unmapSharedMemory();
    TransportResult<bool> initializeControlBlock();
    TransportResult<bool> validateControlBlock();
    TransportResult<bool> setupRingBuffers();
    TransportResult<bool> createSemaphores();
    TransportResult<bool> openSemaphores();
    TransportResult<bool> closeSemaphores();

    TransportResult<bool> pushToRing(ByteRingBuffer* ring, sem_t* sem,
                                      const uint8_t* data, size_t size);
    TransportResult<MessagePtr> popFromRing(ByteRingBuffer* ring, sem_t* sem,
                                             int32_t timeout_ms);

    void ioThreadFunc();
    void pollReceiveRing();

    void setState(TransportState new_state);
    void setError(TransportError error, const std::string& message);
    void updateStats(bool is_send, uint32_t bytes, bool success);

    static uint32_t computeCrc32(const void* data, size_t size);
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_SHARED_MEMORY_TRANSPORT_H
