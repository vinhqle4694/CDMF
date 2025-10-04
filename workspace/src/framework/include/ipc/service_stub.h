/**
 * @file service_stub.h
 * @brief Server-side stub for receiving and dispatching RPC requests
 *
 * Provides a stub interface for receiving remote procedure calls over IPC transports,
 * dispatching them to service implementations, and sending responses back to clients.
 * Supports method routing, error handling, and statistics tracking.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Stub Agent
 */

#ifndef CDMF_IPC_SERVICE_STUB_H
#define CDMF_IPC_SERVICE_STUB_H

#include "transport.h"
#include "message.h"
#include "serializer.h"
#include <memory>
#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <stdexcept>

namespace cdmf {
namespace ipc {

/**
 * @brief Method handler callback type
 *
 * Handler receives request data and returns response data.
 * If handler throws exception, it will be caught and error response sent.
 *
 * @param request_data Request payload data
 * @return Response payload data
 */
using MethodHandler = std::function<std::vector<uint8_t>(const std::vector<uint8_t>& request_data)>;

/**
 * @brief Request validation callback type
 *
 * Validates incoming request before dispatching to handler.
 *
 * @param message Request message
 * @return true if valid, false to reject
 */
using RequestValidator = std::function<bool(const Message& message)>;

/**
 * @brief Authentication callback type
 *
 * Authenticates incoming request.
 *
 * @param message Request message
 * @return true if authenticated, false to reject
 */
using AuthenticationHandler = std::function<bool(const Message& message)>;

/**
 * @brief Stub configuration
 */
struct StubConfig {
    /** Transport configuration */
    TransportConfig transport_config;

    /** Serialization format to use */
    SerializationFormat serialization_format = SerializationFormat::BINARY;

    /** Service endpoint name (for logging/debugging) */
    std::string service_name;

    /** Maximum concurrent request handlers */
    uint32_t max_concurrent_requests = 100;

    /** Request timeout (milliseconds) - for handler execution */
    uint32_t request_timeout_ms = 30000;

    /** Enable request validation */
    bool enable_validation = true;

    /** Enable authentication */
    bool enable_authentication = false;

    /** Graceful shutdown timeout (milliseconds) */
    uint32_t shutdown_timeout_ms = 5000;

    StubConfig() = default;
};

/**
 * @brief Stub statistics (non-copyable due to atomics)
 */
struct StubStats {
    /** Total requests received */
    std::atomic<uint64_t> total_requests{0};

    /** Successful responses */
    std::atomic<uint64_t> successful_responses{0};

    /** Error responses */
    std::atomic<uint64_t> error_responses{0};

    /** Rejected requests (validation/auth failed) */
    std::atomic<uint64_t> rejected_requests{0};

    /** Timeout requests (handler exceeded timeout) */
    std::atomic<uint64_t> timeout_requests{0};

    /** Average processing time (microseconds) */
    std::atomic<uint64_t> avg_processing_time_us{0};

    /** Active request handlers */
    std::atomic<uint32_t> active_handlers{0};

    /** Total bytes received */
    std::atomic<uint64_t> bytes_received{0};

    /** Total bytes sent */
    std::atomic<uint64_t> bytes_sent{0};

    // Delete copy operations (atomics are not copyable)
    StubStats() = default;
    StubStats(const StubStats&) = delete;
    StubStats& operator=(const StubStats&) = delete;
};

/**
 * @brief Copyable snapshot of stub statistics
 */
struct StubStatsSnapshot {
    uint64_t total_requests = 0;
    uint64_t successful_responses = 0;
    uint64_t error_responses = 0;
    uint64_t rejected_requests = 0;
    uint64_t timeout_requests = 0;
    uint64_t avg_processing_time_us = 0;
    uint32_t active_handlers = 0;
    uint64_t bytes_received = 0;
    uint64_t bytes_sent = 0;
};

/**
 * @brief Error codes for stub operations
 */
namespace stub_error_codes {
    constexpr uint32_t SUCCESS = 0;
    constexpr uint32_t METHOD_NOT_FOUND = 1001;
    constexpr uint32_t VALIDATION_FAILED = 1002;
    constexpr uint32_t AUTHENTICATION_FAILED = 1003;
    constexpr uint32_t HANDLER_EXCEPTION = 1004;
    constexpr uint32_t HANDLER_TIMEOUT = 1005;
    constexpr uint32_t MAX_REQUESTS_EXCEEDED = 1006;
    constexpr uint32_t INVALID_REQUEST = 1007;
    constexpr uint32_t SERIALIZATION_FAILED = 1008;
    constexpr uint32_t TRANSPORT_ERROR = 1009;
}

/**
 * @brief Service stub class for RPC server-side
 *
 * Receives RPC requests over IPC transports, dispatches them to registered
 * method handlers, and sends responses back to clients. Handles method routing,
 * error handling, validation, and statistics tracking.
 *
 * Thread Safety: This class is thread-safe for concurrent request handling.
 */
class ServiceStub {
public:
    /**
     * @brief Constructor
     * @param config Stub configuration
     */
    explicit ServiceStub(const StubConfig& config);

    /**
     * @brief Destructor
     */
    ~ServiceStub();

    // Disable copy
    ServiceStub(const ServiceStub&) = delete;
    ServiceStub& operator=(const ServiceStub&) = delete;

    // Lifecycle management

    /**
     * @brief Start the stub (begin listening for requests)
     * @return true if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stop the stub (stop listening, graceful shutdown)
     * @return true if stopped successfully, false otherwise
     */
    bool stop();

    /**
     * @brief Check if stub is running
     * @return true if running, false otherwise
     */
    bool isRunning() const;

    // Method registration

    /**
     * @brief Register a method handler
     * @param method_name Name of the method
     * @param handler Handler function
     * @return true if registered successfully, false if already exists
     */
    bool registerMethod(const std::string& method_name, MethodHandler handler);

    /**
     * @brief Unregister a method handler
     * @param method_name Name of the method
     * @return true if unregistered successfully, false if not found
     */
    bool unregisterMethod(const std::string& method_name);

    /**
     * @brief Check if method is registered
     * @param method_name Name of the method
     * @return true if registered, false otherwise
     */
    bool hasMethod(const std::string& method_name) const;

    /**
     * @brief Get list of registered methods
     * @return Vector of method names
     */
    std::vector<std::string> getRegisteredMethods() const;

    // Hooks and callbacks

    /**
     * @brief Set request validator
     * @param validator Validation callback
     */
    void setRequestValidator(RequestValidator validator);

    /**
     * @brief Set authentication handler
     * @param handler Authentication callback
     */
    void setAuthenticationHandler(AuthenticationHandler handler);

    /**
     * @brief Set error handler callback (called when handler throws)
     * @param handler Error handler callback
     */
    void setErrorHandler(std::function<void(const std::exception&, const std::string&)> handler);

    // Configuration

    /**
     * @brief Get stub configuration
     * @return Current configuration
     */
    const StubConfig& getConfig() const;

    /**
     * @brief Set maximum concurrent requests
     * @param max_requests Maximum concurrent requests
     */
    void setMaxConcurrentRequests(uint32_t max_requests);

    /**
     * @brief Set request timeout
     * @param timeout_ms Timeout in milliseconds
     */
    void setRequestTimeout(uint32_t timeout_ms);

    // Statistics

    /**
     * @brief Get stub statistics
     * @return Current statistics (copy of atomic values)
     */
    StubStatsSnapshot getStats() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStats();

    /**
     * @brief Get number of active request handlers
     * @return Number of active handlers
     */
    uint32_t getActiveHandlers() const;

private:
    /**
     * @brief Request processing thread
     */
    void requestThread();

    /**
     * @brief Handle incoming request message
     * @param message Request message
     */
    void handleRequest(MessagePtr message);

    /**
     * @brief Dispatch request to method handler
     * @param message Request message
     * @return Response message
     */
    Message dispatchRequest(const Message& message);

    /**
     * @brief Validate incoming request
     * @param message Request message
     * @return true if valid, false otherwise
     */
    bool validateRequest(const Message& message);

    /**
     * @brief Authenticate incoming request
     * @param message Request message
     * @return true if authenticated, false otherwise
     */
    bool authenticateRequest(const Message& message);

    /**
     * @brief Create error response message
     * @param request Request message
     * @param error_code Error code
     * @param error_message Error message
     * @return Error response message
     */
    Message createErrorResponse(
        const Message& request,
        uint32_t error_code,
        const std::string& error_message
    );

    /**
     * @brief Send response message
     * @param response Response message
     */
    void sendResponse(const Message& response);

    /**
     * @brief Update average processing time
     * @param processing_time_us Processing time in microseconds
     */
    void updateAvgProcessingTime(uint64_t processing_time_us);

    /**
     * @brief Wait for pending requests to complete (for graceful shutdown)
     * @param timeout_ms Timeout in milliseconds
     * @return true if all completed, false if timeout
     */
    bool waitForPendingRequests(uint32_t timeout_ms);

    /** Stub configuration */
    StubConfig config_;

    /** Transport instance */
    TransportPtr transport_;

    /** Serializer instance */
    SerializerPtr serializer_;

    /** Method handlers map */
    std::map<std::string, MethodHandler> method_handlers_;

    /** Mutex for method handlers */
    mutable std::mutex handlers_mutex_;

    /** Request validator */
    RequestValidator request_validator_;

    /** Authentication handler */
    AuthenticationHandler auth_handler_;

    /** Error handler */
    std::function<void(const std::exception&, const std::string&)> error_handler_;

    /** Request processing thread */
    std::thread request_thread_;

    /** Running flag */
    std::atomic<bool> running_{false};

    /** Statistics */
    StubStats stats_;

    /** Condition variable for graceful shutdown */
    std::condition_variable shutdown_cv_;

    /** Mutex for shutdown condition */
    std::mutex shutdown_mutex_;
};

/**
 * @brief Shared pointer type for ServiceStub
 */
using ServiceStubPtr = std::shared_ptr<ServiceStub>;

/**
 * @brief Stub factory for creating stub instances
 */
class StubFactory {
public:
    /**
     * @brief Create a service stub
     * @param config Stub configuration
     * @return Stub instance or nullptr on failure
     */
    static ServiceStubPtr createStub(const StubConfig& config);

    /**
     * @brief Create and start a service stub
     * @param config Stub configuration
     * @return Started stub instance or nullptr on failure
     */
    static ServiceStubPtr createAndStart(const StubConfig& config);
};

/**
 * @brief Helper class for building method handlers with type safety
 *
 * Example usage:
 * @code
 * auto handler = MethodHandlerBuilder<int, std::string>()
 *     .setHandler([](int input) { return "Result: " + std::to_string(input); })
 *     .build();
 * @endcode
 */
template<typename RequestType, typename ResponseType>
class MethodHandlerBuilder {
public:
    using TypedHandler = std::function<ResponseType(const RequestType&)>;

    /**
     * @brief Set typed handler function
     * @param handler Typed handler function
     * @return Reference to builder
     */
    MethodHandlerBuilder& setHandler(TypedHandler handler) {
        typed_handler_ = std::move(handler);
        return *this;
    }

    /**
     * @brief Set custom serializer for request
     * @param serializer Serializer function
     * @return Reference to builder
     */
    MethodHandlerBuilder& setRequestDeserializer(
        std::function<RequestType(const std::vector<uint8_t>&)> deserializer
    ) {
        request_deserializer_ = std::move(deserializer);
        return *this;
    }

    /**
     * @brief Set custom serializer for response
     * @param serializer Serializer function
     * @return Reference to builder
     */
    MethodHandlerBuilder& setResponseSerializer(
        std::function<std::vector<uint8_t>(const ResponseType&)> serializer
    ) {
        response_serializer_ = std::move(serializer);
        return *this;
    }

    /**
     * @brief Build method handler
     * @return Method handler function
     */
    MethodHandler build() {
        if (!typed_handler_) {
            throw std::runtime_error("Handler not set");
        }

        return [this](const std::vector<uint8_t>& request_data) -> std::vector<uint8_t> {
            // Deserialize request
            RequestType request;
            if (request_deserializer_) {
                request = request_deserializer_(request_data);
            } else {
                // Default: assume raw bytes can be cast
                // Note: In production, use proper serialization
                request = *reinterpret_cast<const RequestType*>(request_data.data());
            }

            // Invoke handler
            ResponseType response = typed_handler_(request);

            // Serialize response
            if (response_serializer_) {
                return response_serializer_(response);
            } else {
                // Default: copy bytes
                std::vector<uint8_t> result(sizeof(ResponseType));
                std::memcpy(result.data(), &response, sizeof(ResponseType));
                return result;
            }
        };
    }

private:
    TypedHandler typed_handler_;
    std::function<RequestType(const std::vector<uint8_t>&)> request_deserializer_;
    std::function<std::vector<uint8_t>(const ResponseType&)> response_serializer_;
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_SERVICE_STUB_H
