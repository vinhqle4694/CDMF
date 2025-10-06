/**
 * @file sandbox_ipc.h
 * @brief Sandbox IPC communication interface
 *
 * Provides high-level IPC abstraction for parent-child sandbox communication
 * using pluggable transport strategies (shared memory, Unix sockets, TCP).
 *
 * @version 1.0.0
 * @date 2025-10-06
 */

#ifndef CDMF_SANDBOX_IPC_H
#define CDMF_SANDBOX_IPC_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include "ipc/transport.h"
#include "ipc/message.h"

namespace cdmf {
namespace security {

/**
 * @brief Message types for sandbox IPC
 */
enum class SandboxMessageType : uint32_t {
    // Lifecycle commands
    LOAD_MODULE = 1,        ///< Parent → Child: Load module shared library
    MODULE_LOADED = 2,      ///< Child → Parent: Module loaded successfully
    START_MODULE = 3,       ///< Parent → Child: Start module
    MODULE_STARTED = 4,     ///< Child → Parent: Module started
    STOP_MODULE = 5,        ///< Parent → Child: Stop module
    MODULE_STOPPED = 6,     ///< Child → Parent: Module stopped

    // Service invocation
    CALL_SERVICE = 10,      ///< Parent → Child: Call service method
    SERVICE_RESPONSE = 11,  ///< Child → Parent: Service method response

    // Monitoring
    HEARTBEAT = 20,         ///< Bidirectional: Keep-alive
    STATUS_QUERY = 21,      ///< Parent → Child: Request status
    STATUS_REPORT = 22,     ///< Child → Parent: Status report

    // Control
    SHUTDOWN = 30,          ///< Parent → Child: Shutdown process
    ERROR = 31,             ///< Child → Parent: Error occurred
};

/**
 * @brief Sandbox IPC message payload
 */
struct SandboxMessage {
    SandboxMessageType type;
    std::string moduleId;
    std::string payload;      ///< JSON-encoded data (module path, service call args, etc.)
    uint64_t requestId;       ///< For request-response correlation
    int32_t errorCode;        ///< 0 = success, non-zero = error

    /**
     * @brief Serialize to IPC Message
     * @return IPC message pointer
     */
    ipc::MessagePtr toIPCMessage() const;

    /**
     * @brief Deserialize from IPC Message
     * @param msg IPC message
     * @return Sandbox message
     */
    static SandboxMessage fromIPCMessage(const ipc::Message& msg);
};

/**
 * @brief High-level wrapper around ITransport for sandbox communication
 *
 * Uses Strategy pattern to support multiple transport types:
 * - SharedMemoryTransport: High bandwidth for same-device IPC
 * - UnixSocketTransport: Low overhead for development/debugging
 * - TCPSocketTransport: For distributed sandboxing
 *
 * Provides synchronous send/receive semantics on top of pluggable transport.
 * Optimized for parent-child process communication.
 */
class SandboxIPC {
public:
    /**
     * @brief Role in IPC communication
     */
    enum class Role {
        PARENT,  ///< Framework (creates/binds transport)
        CHILD    ///< Sandboxed module (connects to transport)
    };

    /**
     * @brief Constructor with transport injection (Strategy pattern)
     *
     * @param role PARENT or CHILD
     * @param sandboxId Unique sandbox identifier
     * @param transport Transport strategy to use (ownership transferred)
     */
    SandboxIPC(Role role, const std::string& sandboxId,
               std::unique_ptr<ipc::ITransport> transport);

    ~SandboxIPC();

    /**
     * @brief Initialize IPC channel with transport-specific config
     *
     * Parent creates/binds transport, child connects to existing.
     *
     * @param config Transport configuration (type-specific)
     * @return true if initialization successful
     */
    bool initialize(const ipc::TransportConfig& config);

    /**
     * @brief Send message (blocking with timeout)
     * @param msg Sandbox message to send
     * @param timeoutMs Timeout in milliseconds
     * @return true if sent successfully
     */
    bool sendMessage(const SandboxMessage& msg, int32_t timeoutMs = 5000);

    /**
     * @brief Receive message (blocking with timeout)
     * @param msg Output message
     * @param timeoutMs Timeout in milliseconds
     * @return true if received successfully
     */
    bool receiveMessage(SandboxMessage& msg, int32_t timeoutMs = 5000);

    /**
     * @brief Try receive message (non-blocking)
     * @param msg Output message
     * @return true if message received
     */
    bool tryReceiveMessage(SandboxMessage& msg);

    /**
     * @brief Send request and wait for response
     * @param request Request message
     * @param response Output response message
     * @param timeoutMs Timeout in milliseconds
     * @return true if request-response completed successfully
     */
    bool sendRequest(const SandboxMessage& request, SandboxMessage& response,
                    int32_t timeoutMs = 5000);

    /**
     * @brief Close IPC channel
     */
    void close();

    /**
     * @brief Check if IPC is connected
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get transport endpoint information
     *
     * Returns transport-specific endpoint:
     * - Shared memory: "/dev/shm/cdmf_sandbox_<id>"
     * - Unix socket: "/tmp/cdmf_sandbox_<id>.sock"
     * - TCP: "localhost:9000"
     *
     * @return Endpoint string
     */
    std::string getEndpoint() const;

    /**
     * @brief Get statistics
     */
    struct Stats {
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t send_failures;
        uint64_t receive_failures;
        uint64_t bytes_sent;
        uint64_t bytes_received;
    };

    /**
     * @brief Get IPC statistics
     * @return Statistics structure
     */
    Stats getStats() const;

private:
    Role role_;
    std::string sandbox_id_;

    // Transport strategy (pluggable)
    std::unique_ptr<ipc::ITransport> transport_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;

    // Request-response tracking
    std::atomic<uint64_t> next_request_id_;

    /**
     * @brief Update statistics
     * @param is_send true for send, false for receive
     * @param bytes Number of bytes
     * @param success true if operation succeeded
     */
    void updateStats(bool is_send, size_t bytes, bool success);
};

/**
 * @brief Factory function to create transport for sandbox IPC
 *
 * @param type Transport type to create
 * @param sandboxId Sandbox identifier (used for endpoint naming)
 * @return Unique pointer to transport instance
 */
std::unique_ptr<ipc::ITransport> createSandboxTransport(
    ipc::TransportType type,
    const std::string& sandboxId);

/**
 * @brief Helper to create transport configuration for sandbox
 *
 * Creates transport configuration based on type and properties from manifest.
 * Reads properties like ipc_buffer_size, ipc_shm_size, ipc_endpoint, etc.
 *
 * @param type Transport type
 * @param sandboxId Sandbox identifier
 * @param role PARENT or CHILD
 * @param properties Properties map from SandboxConfig (from manifest.json)
 * @return Transport configuration
 */
ipc::TransportConfig createSandboxTransportConfig(
    ipc::TransportType type,
    const std::string& sandboxId,
    SandboxIPC::Role role,
    const std::map<std::string, std::string>& properties);

/**
 * @brief Convert transport type to string
 * @param type Transport type
 * @return String representation
 */
const char* transportTypeToString(ipc::TransportType type);

} // namespace security
} // namespace cdmf

#endif // CDMF_SANDBOX_IPC_H
