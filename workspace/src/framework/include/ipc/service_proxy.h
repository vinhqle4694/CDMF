/**
 * @file service_proxy.h
 * @brief Client-side proxy for transparent remote method invocation
 *
 * Provides a proxy interface for making remote procedure calls over IPC transports.
 * Supports synchronous, asynchronous, and one-way call semantics with automatic
 * request/response correlation, retry logic, and timeout handling.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Stub Agent
 */

#ifndef CDMF_IPC_SERVICE_PROXY_H
#define CDMF_IPC_SERVICE_PROXY_H

#include "transport.h"
#include "message.h"
#include "serializer.h"
#include <memory>
#include <string>
#include <future>
#include <functional>
#include <map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

namespace cdmf {
namespace ipc {

/**
 * @brief RPC call type enumeration
 */
enum class CallType {
    /** Synchronous call - blocks until response received */
    SYNC,

    /** Asynchronous call - returns future, non-blocking */
    ASYNC,

    /** One-way call - fire-and-forget, no response expected */
    ONEWAY
};

/**
 * @brief Retry policy configuration
 */
struct RetryPolicy {
    /** Enable automatic retry on failure */
    bool enabled = false;

    /** Maximum number of retry attempts */
    uint32_t max_attempts = 3;

    /** Initial retry delay (milliseconds) */
    uint32_t initial_delay_ms = 100;

    /** Maximum retry delay (milliseconds) */
    uint32_t max_delay_ms = 5000;

    /** Use exponential backoff */
    bool exponential_backoff = true;

    /** Backoff multiplier (e.g., 2.0 for doubling) */
    double backoff_multiplier = 2.0;

    RetryPolicy() = default;
};

/**
 * @brief Proxy configuration
 */
struct ProxyConfig {
    /** Transport configuration */
    TransportConfig transport_config;

    /** Default timeout for synchronous calls (milliseconds) */
    uint32_t default_timeout_ms = 5000;

    /** Retry policy */
    RetryPolicy retry_policy;

    /** Enable automatic reconnection on disconnect */
    bool auto_reconnect = true;

    /** Serialization format to use */
    SerializationFormat serialization_format = SerializationFormat::BINARY;

    /** Service endpoint name (for logging/debugging) */
    std::string service_name;

    ProxyConfig() = default;
};

/**
 * @brief RPC call result structure
 */
template<typename T = std::vector<uint8_t>>
struct CallResult {
    /** Success flag */
    bool success = false;

    /** Result data (for successful calls) */
    T data;

    /** Error code (0 if success) */
    uint32_t error_code = 0;

    /** Error message (if failed) */
    std::string error_message;

    /** Number of retry attempts made */
    uint32_t retry_count = 0;

    /** Total call duration (including retries) */
    std::chrono::milliseconds duration{0};

    /** Check if call succeeded */
    explicit operator bool() const { return success; }
};

/**
 * @brief Callback type for async calls
 */
template<typename T = std::vector<uint8_t>>
using AsyncCallback = std::function<void(const CallResult<T>&)>;

/**
 * @brief Pending call information
 */
struct PendingCall {
    /** Call ID (correlation ID) */
    uint8_t call_id[16];

    /** Method name */
    std::string method_name;

    /** Promise for async result */
    std::promise<MessagePtr> promise;

    /** Call start time */
    std::chrono::steady_clock::time_point start_time;

    /** Timeout duration */
    std::chrono::milliseconds timeout;

    /** Retry count */
    uint32_t retry_count = 0;

    PendingCall() = default;
};

/**
 * @brief Proxy statistics snapshot (copyable)
 */
struct ProxyStatsSnapshot {
    /** Total calls made */
    uint64_t total_calls = 0;

    /** Successful calls */
    uint64_t successful_calls = 0;

    /** Failed calls */
    uint64_t failed_calls = 0;

    /** Timeout calls */
    uint64_t timeout_calls = 0;

    /** Total retries */
    uint64_t total_retries = 0;

    /** Average response time (microseconds) */
    uint64_t avg_response_time_us = 0;

    /** Active pending calls */
    uint32_t active_calls = 0;
};

/**
 * @brief Proxy statistics (internal, uses atomics)
 */
struct ProxyStats {
    /** Total calls made */
    std::atomic<uint64_t> total_calls{0};

    /** Successful calls */
    std::atomic<uint64_t> successful_calls{0};

    /** Failed calls */
    std::atomic<uint64_t> failed_calls{0};

    /** Timeout calls */
    std::atomic<uint64_t> timeout_calls{0};

    /** Total retries */
    std::atomic<uint64_t> total_retries{0};

    /** Average response time (microseconds) */
    std::atomic<uint64_t> avg_response_time_us{0};

    /** Active pending calls */
    std::atomic<uint32_t> active_calls{0};

    // Delete copy operations (atomics are not copyable)
    ProxyStats() = default;
    ProxyStats(const ProxyStats&) = delete;
    ProxyStats& operator=(const ProxyStats&) = delete;
};

/**
 * @brief Service proxy class for RPC client-side
 *
 * Provides transparent remote method invocation over IPC transports.
 * Handles request/response correlation, timeouts, retries, and connection management.
 *
 * Thread Safety: This class is thread-safe for concurrent method calls.
 */
class ServiceProxy {
public:
    /**
     * @brief Constructor
     * @param config Proxy configuration
     */
    explicit ServiceProxy(const ProxyConfig& config);

    /**
     * @brief Destructor
     */
    ~ServiceProxy();

    // Disable copy
    ServiceProxy(const ServiceProxy&) = delete;
    ServiceProxy& operator=(const ServiceProxy&) = delete;

    // Connection management

    /**
     * @brief Connect to remote service
     * @return true if connected successfully, false otherwise
     */
    bool connect();

    /**
     * @brief Disconnect from remote service
     * @return true if disconnected successfully, false otherwise
     */
    bool disconnect();

    /**
     * @brief Check if connected to remote service
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    // Synchronous calls

    /**
     * @brief Make a synchronous RPC call
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @param request_size Request payload size
     * @param timeout_ms Timeout in milliseconds (0 = use default)
     * @return Call result with response data or error
     */
    CallResult<std::vector<uint8_t>> call(
        const std::string& method_name,
        const void* request_data,
        uint32_t request_size,
        uint32_t timeout_ms = 0
    );

    /**
     * @brief Make a synchronous RPC call (vector variant)
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @param timeout_ms Timeout in milliseconds (0 = use default)
     * @return Call result with response data or error
     */
    CallResult<std::vector<uint8_t>> call(
        const std::string& method_name,
        const std::vector<uint8_t>& request_data,
        uint32_t timeout_ms = 0
    );

    // Asynchronous calls

    /**
     * @brief Make an asynchronous RPC call
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @param request_size Request payload size
     * @param timeout_ms Timeout in milliseconds (0 = use default)
     * @return Future for call result
     */
    std::future<CallResult<std::vector<uint8_t>>> callAsync(
        const std::string& method_name,
        const void* request_data,
        uint32_t request_size,
        uint32_t timeout_ms = 0
    );

    /**
     * @brief Make an asynchronous RPC call (vector variant)
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @param timeout_ms Timeout in milliseconds (0 = use default)
     * @return Future for call result
     */
    std::future<CallResult<std::vector<uint8_t>>> callAsync(
        const std::string& method_name,
        const std::vector<uint8_t>& request_data,
        uint32_t timeout_ms = 0
    );

    /**
     * @brief Make an asynchronous RPC call with callback
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @param callback Callback function to invoke with result
     * @param timeout_ms Timeout in milliseconds (0 = use default)
     */
    void callAsync(
        const std::string& method_name,
        const std::vector<uint8_t>& request_data,
        AsyncCallback<std::vector<uint8_t>> callback,
        uint32_t timeout_ms = 0
    );

    // One-way calls

    /**
     * @brief Make a one-way RPC call (fire-and-forget)
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @param request_size Request payload size
     * @return true if sent successfully, false otherwise
     */
    bool callOneWay(
        const std::string& method_name,
        const void* request_data,
        uint32_t request_size
    );

    /**
     * @brief Make a one-way RPC call (vector variant)
     * @param method_name Name of the remote method
     * @param request_data Request payload data
     * @return true if sent successfully, false otherwise
     */
    bool callOneWay(
        const std::string& method_name,
        const std::vector<uint8_t>& request_data
    );

    // Configuration

    /**
     * @brief Get proxy configuration
     * @return Current configuration
     */
    const ProxyConfig& getConfig() const;

    /**
     * @brief Update retry policy
     * @param policy New retry policy
     */
    void setRetryPolicy(const RetryPolicy& policy);

    /**
     * @brief Set default timeout
     * @param timeout_ms Timeout in milliseconds
     */
    void setDefaultTimeout(uint32_t timeout_ms);

    // Statistics

    /**
     * @brief Get proxy statistics
     * @return Current statistics (copy of atomic values)
     */
    ProxyStatsSnapshot getStats() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStats();

    /**
     * @brief Get number of active pending calls
     * @return Number of active calls
     */
    uint32_t getActiveCalls() const;

private:
    /**
     * @brief Send request and wait for response
     * @param method_name Method name
     * @param request_data Request data
     * @param request_size Request size
     * @param timeout Timeout duration
     * @param retry_count Current retry count
     * @return Call result
     */
    CallResult<std::vector<uint8_t>> sendAndReceive(
        const std::string& method_name,
        const void* request_data,
        uint32_t request_size,
        std::chrono::milliseconds timeout,
        uint32_t retry_count = 0
    );

    /**
     * @brief Create request message
     * @param method_name Method name
     * @param request_data Request data
     * @param request_size Request size
     * @param call_type Call type
     * @return Request message
     */
    Message createRequest(
        const std::string& method_name,
        const void* request_data,
        uint32_t request_size,
        CallType call_type
    );

    /**
     * @brief Generate unique call ID
     * @param id Output buffer (16 bytes)
     */
    void generateCallId(uint8_t* id);

    /**
     * @brief Register pending call
     * @param call_id Call ID
     * @param method_name Method name
     * @param timeout Timeout duration
     * @return Shared pointer to pending call
     */
    std::shared_ptr<PendingCall> registerPendingCall(
        const uint8_t* call_id,
        const std::string& method_name,
        std::chrono::milliseconds timeout
    );

    /**
     * @brief Unregister pending call
     * @param call_id Call ID
     */
    void unregisterPendingCall(const uint8_t* call_id);

    /**
     * @brief Handle received response message
     * @param message Response message
     */
    void handleResponse(MessagePtr message);

    /**
     * @brief Background thread for receiving responses
     */
    void receiveThread();

    /**
     * @brief Timeout checker thread
     */
    void timeoutThread();

    /**
     * @brief Calculate retry delay
     * @param attempt Retry attempt number (0-based)
     * @return Delay in milliseconds
     */
    uint32_t calculateRetryDelay(uint32_t attempt) const;

    /**
     * @brief Update average response time
     * @param response_time_us Response time in microseconds
     */
    void updateAvgResponseTime(uint64_t response_time_us);

    /** Proxy configuration */
    ProxyConfig config_;

    /** Transport instance */
    TransportPtr transport_;

    /** Serializer instance */
    SerializerPtr serializer_;

    /** Pending calls map (correlation_id -> PendingCall) */
    std::map<std::string, std::shared_ptr<PendingCall>> pending_calls_;

    /** Mutex for pending calls */
    mutable std::mutex pending_calls_mutex_;

    /** Receive thread */
    std::thread receive_thread_;

    /** Timeout checker thread */
    std::thread timeout_thread_;

    /** Running flag */
    std::atomic<bool> running_{false};

    /** Statistics */
    ProxyStats stats_;

    /** Call ID counter for uniqueness */
    std::atomic<uint64_t> call_id_counter_{0};
};

/**
 * @brief Shared pointer type for ServiceProxy
 */
using ServiceProxyPtr = std::shared_ptr<ServiceProxy>;

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_SERVICE_PROXY_H
