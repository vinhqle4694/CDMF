/**
 * @file transport.h
 * @brief Abstract transport interface for IPC communication
 *
 * Defines the base transport interface and configuration structures
 * for implementing various IPC transport mechanisms (Unix sockets,
 * shared memory, gRPC, etc.).
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Transport Agent
 */

#ifndef CDMF_IPC_TRANSPORT_H
#define CDMF_IPC_TRANSPORT_H

#include "message.h"
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <map>

namespace cdmf {
namespace ipc {

/**
 * @brief Transport type enumeration
 */
enum class TransportType {
    UNIX_SOCKET,
    SHARED_MEMORY,
    GRPC,
    TCP_SOCKET,
    UDP_SOCKET,
    UNKNOWN
};

/**
 * @brief Transport state enumeration
 */
enum class TransportState {
    UNINITIALIZED,
    INITIALIZED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED,
    ERROR
};

/**
 * @brief Transport operation mode
 */
enum class TransportMode {
    /** Synchronous operations (blocking) */
    SYNC,
    /** Asynchronous operations (non-blocking) */
    ASYNC,
    /** Hybrid mode (configurable per operation) */
    HYBRID
};

/**
 * @brief Transport configuration structure
 */
struct TransportConfig {
    /** Transport type */
    TransportType type = TransportType::UNKNOWN;

    /** Operation mode */
    TransportMode mode = TransportMode::SYNC;

    /** Endpoint address/path */
    std::string endpoint;

    /** Connection timeout (milliseconds) */
    uint32_t connect_timeout_ms = 5000;

    /** Send timeout (milliseconds) */
    uint32_t send_timeout_ms = 3000;

    /** Receive timeout (milliseconds) */
    uint32_t recv_timeout_ms = 3000;

    /** Enable automatic reconnection */
    bool auto_reconnect = false;

    /** Reconnect interval (milliseconds) */
    uint32_t reconnect_interval_ms = 1000;

    /** Maximum reconnect attempts (0 = infinite) */
    uint32_t max_reconnect_attempts = 3;

    /** Enable keepalive/heartbeat */
    bool enable_keepalive = true;

    /** Keepalive interval (milliseconds) */
    uint32_t keepalive_interval_ms = 30000;

    /** Maximum message size */
    uint32_t max_message_size = 16 * 1024 * 1024;

    /** Buffer size for send/receive */
    uint32_t buffer_size = 65536;

    /** Additional transport-specific properties */
    std::map<std::string, std::string> properties;

    TransportConfig() = default;
};

/**
 * @brief Transport statistics structure
 */
struct TransportStats {
    /** Total messages sent */
    uint64_t messages_sent = 0;

    /** Total messages received */
    uint64_t messages_received = 0;

    /** Total bytes sent */
    uint64_t bytes_sent = 0;

    /** Total bytes received */
    uint64_t bytes_received = 0;

    /** Send errors */
    uint64_t send_errors = 0;

    /** Receive errors */
    uint64_t recv_errors = 0;

    /** Connection errors */
    uint64_t connection_errors = 0;

    /** Current connection count */
    uint32_t active_connections = 0;

    /** Last error timestamp */
    std::chrono::system_clock::time_point last_error_time;

    /** Last error message */
    std::string last_error;

    TransportStats() = default;
};

/**
 * @brief Transport error codes
 */
enum class TransportError {
    SUCCESS = 0,
    NOT_INITIALIZED,
    ALREADY_INITIALIZED,
    NOT_CONNECTED,
    ALREADY_CONNECTED,
    CONNECTION_FAILED,
    CONNECTION_CLOSED,
    CONNECTION_TIMEOUT,
    SEND_FAILED,
    RECV_FAILED,
    TIMEOUT,
    INVALID_CONFIG,
    INVALID_MESSAGE,
    BUFFER_OVERFLOW,
    SERIALIZATION_ERROR,
    DESERIALIZATION_ERROR,
    RESOURCE_EXHAUSTED,
    PERMISSION_DENIED,
    ENDPOINT_NOT_FOUND,
    PROTOCOL_ERROR,
    UNKNOWN_ERROR
};

/**
 * @brief Result type for transport operations
 */
template<typename T>
struct TransportResult {
    TransportError error = TransportError::SUCCESS;
    T value = T{};
    std::string error_message;

    bool success() const { return error == TransportError::SUCCESS; }
    explicit operator bool() const { return success(); }
};

// Callback types
using MessageCallback = std::function<void(MessagePtr)>;
using ErrorCallback = std::function<void(TransportError, const std::string&)>;
using StateChangeCallback = std::function<void(TransportState, TransportState)>;

/**
 * @brief Abstract transport interface
 *
 * Base class for all transport implementations. Provides common
 * lifecycle management, message sending/receiving, and status reporting.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    // Lifecycle management

    /**
     * @brief Initialize the transport with configuration
     * @param config Transport configuration
     * @return Result with error code and message
     */
    virtual TransportResult<bool> init(const TransportConfig& config) = 0;

    /**
     * @brief Start the transport (begin listening/accepting connections)
     * @return Result with error code and message
     */
    virtual TransportResult<bool> start() = 0;

    /**
     * @brief Stop the transport (close all connections)
     * @return Result with error code and message
     */
    virtual TransportResult<bool> stop() = 0;

    /**
     * @brief Cleanup and release resources
     * @return Result with error code and message
     */
    virtual TransportResult<bool> cleanup() = 0;

    // Connection management

    /**
     * @brief Connect to remote endpoint (client-side)
     * @return Result with error code and message
     */
    virtual TransportResult<bool> connect() = 0;

    /**
     * @brief Disconnect from remote endpoint
     * @return Result with error code and message
     */
    virtual TransportResult<bool> disconnect() = 0;

    /**
     * @brief Check if connected to remote endpoint
     * @return true if connected, false otherwise
     */
    virtual bool isConnected() const = 0;

    // Message operations

    /**
     * @brief Send a message
     * @param message Message to send
     * @return Result with error code and message
     */
    virtual TransportResult<bool> send(const Message& message) = 0;

    /**
     * @brief Send a message (move semantics)
     * @param message Message to send
     * @return Result with error code and message
     */
    virtual TransportResult<bool> send(Message&& message) = 0;

    /**
     * @brief Receive a message (blocking or non-blocking based on mode)
     * @param timeout_ms Timeout in milliseconds (0 = use default, -1 = infinite)
     * @return Result with message or error
     */
    virtual TransportResult<MessagePtr> receive(int32_t timeout_ms = 0) = 0;

    /**
     * @brief Try to receive a message (non-blocking)
     * @return Result with message or error (TIMEOUT if no message available)
     */
    virtual TransportResult<MessagePtr> tryReceive() = 0;

    // Callbacks

    /**
     * @brief Set callback for received messages (async mode)
     * @param callback Message callback function
     */
    virtual void setMessageCallback(MessageCallback callback) = 0;

    /**
     * @brief Set callback for errors
     * @param callback Error callback function
     */
    virtual void setErrorCallback(ErrorCallback callback) = 0;

    /**
     * @brief Set callback for state changes
     * @param callback State change callback function
     */
    virtual void setStateChangeCallback(StateChangeCallback callback) = 0;

    // Status and information

    /**
     * @brief Get current transport state
     * @return Current state
     */
    virtual TransportState getState() const = 0;

    /**
     * @brief Get transport type
     * @return Transport type
     */
    virtual TransportType getType() const = 0;

    /**
     * @brief Get transport configuration
     * @return Current configuration
     */
    virtual const TransportConfig& getConfig() const = 0;

    /**
     * @brief Get transport statistics
     * @return Transport statistics
     */
    virtual TransportStats getStats() const = 0;

    /**
     * @brief Reset statistics counters
     */
    virtual void resetStats() = 0;

    /**
     * @brief Get last error information
     * @return Pair of error code and error message
     */
    virtual std::pair<TransportError, std::string> getLastError() const = 0;

    /**
     * @brief Get transport-specific information as string
     * @return Information string (for debugging/logging)
     */
    virtual std::string getInfo() const = 0;
};

/**
 * @brief Shared pointer type for transport
 */
using TransportPtr = std::shared_ptr<ITransport>;

/**
 * @brief Transport factory for creating transport instances
 */
class TransportFactory {
public:
    /**
     * @brief Create transport of specified type
     * @param type Transport type
     * @return Transport instance or nullptr on failure
     */
    static TransportPtr create(TransportType type);

    /**
     * @brief Create transport from configuration
     * @param config Transport configuration
     * @return Transport instance or nullptr on failure
     */
    static TransportPtr create(const TransportConfig& config);

    /**
     * @brief Register custom transport creator
     * @param type Transport type
     * @param creator Creator function
     */
    using TransportCreator = std::function<TransportPtr()>;
    static void registerTransport(TransportType type, TransportCreator creator);
};

/**
 * @brief Convert transport error to string
 */
const char* transportErrorToString(TransportError error);

/**
 * @brief Convert transport type to string
 */
const char* transportTypeToString(TransportType type);

/**
 * @brief Convert transport state to string
 */
const char* transportStateToString(TransportState state);

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_TRANSPORT_H
