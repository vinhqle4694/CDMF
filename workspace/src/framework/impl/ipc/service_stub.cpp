/**
 * @file service_stub.cpp
 * @brief Implementation of server-side stub for RPC
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Stub Agent
 */

#include "ipc/service_stub.h"
#include "utils/log.h"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace cdmf {
namespace ipc {

ServiceStub::ServiceStub(const StubConfig& config)
    : config_(config)
{
    LOGI_FMT("Creating ServiceStub for service: " << config_.service_name
             << ", endpoint: " << config_.transport_config.endpoint);

    // Create transport
    transport_ = TransportFactory::create(config_.transport_config);
    if (!transport_) {
        LOGE_FMT("Failed to create transport for stub");
        throw std::runtime_error("Failed to create transport for stub");
    }

    // Create serializer
    serializer_ = SerializerFactory::createSerializer(config_.serialization_format);
    if (!serializer_) {
        LOGE_FMT("Failed to create serializer for stub");
        throw std::runtime_error("Failed to create serializer for stub");
    }

    LOGI_FMT("ServiceStub created successfully");
    // Note: Transport will be initialized in start()
}

ServiceStub::~ServiceStub() {
    LOGI_FMT("Destroying ServiceStub");
    stop();
}

bool ServiceStub::start() {
    if (running_) {
        LOGI_FMT("ServiceStub already running");
        return true;
    }

    LOGI_FMT("Starting ServiceStub on " << config_.transport_config.endpoint);

    // Initialize transport if not already done
    if (transport_->getState() == TransportState::UNINITIALIZED) {
        LOGD_FMT("Initializing transport");
        auto init_result = transport_->init(config_.transport_config);
        if (!init_result.success()) {
            LOGE_FMT("Transport initialization failed: " << init_result.error_message);
            return false;
        }
    }

    // Start transport (server mode)
    LOGD_FMT("Starting transport");
    auto start_result = transport_->start();
    if (!start_result.success()) {
        LOGE_FMT("Transport start failed: " << start_result.error_message);
        return false;
    }

    // Connect transport to enable message exchange
    LOGD_FMT("Connecting transport");
    auto connect_result = transport_->connect();
    if (!connect_result.success()) {
        LOGE_FMT("Transport connect failed: " << connect_result.error_message);
        return false;
    }

    // Set up message callback for async message reception
    transport_->setMessageCallback([this](MessagePtr message) {
        if (message && message->getType() == MessageType::REQUEST) {
            LOGD_FMT("Received REQUEST message via callback");
            handleRequest(message);
        }
    });

    // Start request processing thread
    running_ = true;
    request_thread_ = std::thread(&ServiceStub::requestThread, this);

    LOGI_FMT("ServiceStub started successfully");
    return true;
}

bool ServiceStub::stop() {
    if (!running_) {
        LOGD_FMT("ServiceStub already stopped");
        return true;
    }

    LOGI_FMT("Stopping ServiceStub");

    running_ = false;

    // Wait for pending requests to complete
    bool completed = waitForPendingRequests(config_.shutdown_timeout_ms);
    if (!completed) {
        LOGW_FMT("Some pending requests did not complete within timeout");
    }

    // Stop request thread
    if (request_thread_.joinable()) {
        request_thread_.join();
    }

    // Stop and cleanup transport
    transport_->stop();
    transport_->cleanup();

    LOGI_FMT("ServiceStub stopped");
    return completed;
}

bool ServiceStub::isRunning() const {
    return running_;
}

bool ServiceStub::registerMethod(const std::string& method_name, MethodHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    if (method_handlers_.find(method_name) != method_handlers_.end()) {
        return false;  // Method already registered
    }

    method_handlers_[method_name] = std::move(handler);
    return true;
}

bool ServiceStub::unregisterMethod(const std::string& method_name) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    auto it = method_handlers_.find(method_name);
    if (it == method_handlers_.end()) {
        return false;  // Method not found
    }

    method_handlers_.erase(it);
    return true;
}

bool ServiceStub::hasMethod(const std::string& method_name) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    return method_handlers_.find(method_name) != method_handlers_.end();
}

std::vector<std::string> ServiceStub::getRegisteredMethods() const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    std::vector<std::string> methods;
    methods.reserve(method_handlers_.size());

    for (const auto& pair : method_handlers_) {
        methods.push_back(pair.first);
    }

    return methods;
}

void ServiceStub::setRequestValidator(RequestValidator validator) {
    request_validator_ = std::move(validator);
}

void ServiceStub::setAuthenticationHandler(AuthenticationHandler handler) {
    auth_handler_ = std::move(handler);
}

void ServiceStub::setErrorHandler(std::function<void(const std::exception&, const std::string&)> handler) {
    error_handler_ = std::move(handler);
}

const StubConfig& ServiceStub::getConfig() const {
    return config_;
}

void ServiceStub::setMaxConcurrentRequests(uint32_t max_requests) {
    config_.max_concurrent_requests = max_requests;
}

void ServiceStub::setRequestTimeout(uint32_t timeout_ms) {
    config_.request_timeout_ms = timeout_ms;
}

StubStatsSnapshot ServiceStub::getStats() const {
    StubStatsSnapshot snapshot;
    snapshot.total_requests = stats_.total_requests.load();
    snapshot.successful_responses = stats_.successful_responses.load();
    snapshot.error_responses = stats_.error_responses.load();
    snapshot.rejected_requests = stats_.rejected_requests.load();
    snapshot.timeout_requests = stats_.timeout_requests.load();
    snapshot.avg_processing_time_us = stats_.avg_processing_time_us.load();
    snapshot.active_handlers = stats_.active_handlers.load();
    snapshot.bytes_received = stats_.bytes_received.load();
    snapshot.bytes_sent = stats_.bytes_sent.load();
    return snapshot;
}

void ServiceStub::resetStats() {
    stats_.total_requests = 0;
    stats_.successful_responses = 0;
    stats_.error_responses = 0;
    stats_.rejected_requests = 0;
    stats_.timeout_requests = 0;
    stats_.avg_processing_time_us = 0;
    stats_.bytes_received = 0;
    stats_.bytes_sent = 0;
}

uint32_t ServiceStub::getActiveHandlers() const {
    return stats_.active_handlers;
}

void ServiceStub::requestThread() {
    while (running_) {
        try {
            // Poll for requests (when not using async callback)
            auto result = transport_->tryReceive();

            if (result.success() && result.value) {
                if (result.value->getType() == MessageType::REQUEST) {
                    handleRequest(result.value);
                }
            } else {
                // No message available, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (const std::exception& e) {
            // Log error but continue
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
}

void ServiceStub::handleRequest(MessagePtr message) {
    if (!message || !running_) {
        return;
    }

    stats_.total_requests++;
    stats_.bytes_received += message->getTotalSize();

    // Check max concurrent requests
    if (stats_.active_handlers >= config_.max_concurrent_requests) {
        auto error_response = createErrorResponse(
            *message,
            stub_error_codes::MAX_REQUESTS_EXCEEDED,
            "Maximum concurrent requests exceeded"
        );
        sendResponse(error_response);
        stats_.rejected_requests++;
        return;
    }

    // Process request in separate thread for concurrent handling
    std::thread([this, message]() {
        auto start_time = std::chrono::steady_clock::now();
        stats_.active_handlers++;

        try {
            // Validate request
            if (config_.enable_validation && !validateRequest(*message)) {
                auto error_response = createErrorResponse(
                    *message,
                    stub_error_codes::VALIDATION_FAILED,
                    "Request validation failed"
                );
                sendResponse(error_response);
                stats_.rejected_requests++;
                stats_.active_handlers--;
                return;
            }

            // Authenticate request
            if (config_.enable_authentication && !authenticateRequest(*message)) {
                auto error_response = createErrorResponse(
                    *message,
                    stub_error_codes::AUTHENTICATION_FAILED,
                    "Authentication failed"
                );
                sendResponse(error_response);
                stats_.rejected_requests++;
                stats_.active_handlers--;
                return;
            }

            // Dispatch request to handler
            auto response = dispatchRequest(*message);

            // Send response
            sendResponse(response);

            if (response.isError()) {
                stats_.error_responses++;
            } else {
                stats_.successful_responses++;
            }

        } catch (const std::exception& e) {
            // Handle exception
            if (error_handler_) {
                error_handler_(e, message->getSubject());
            }

            auto error_response = createErrorResponse(
                *message,
                stub_error_codes::HANDLER_EXCEPTION,
                std::string("Handler exception: ") + e.what()
            );
            sendResponse(error_response);
            stats_.error_responses++;
        }

        stats_.active_handlers--;

        // Update processing time
        auto end_time = std::chrono::steady_clock::now();
        auto processing_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time
        ).count();
        updateAvgProcessingTime(processing_time_us);

    }).detach();  // Detach to allow concurrent handling
}

Message ServiceStub::dispatchRequest(const Message& message) {
    auto method_name = message.getSubject();

    LOGV_FMT("Dispatching request to method: " << method_name);

    // Find method handler
    MethodHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = method_handlers_.find(method_name);
        if (it == method_handlers_.end()) {
            LOGE_FMT("Method not found: " << method_name);
            return createErrorResponse(
                message,
                stub_error_codes::METHOD_NOT_FOUND,
                "Method not found: " + method_name
            );
        }
        handler = it->second;
    }

    // Extract request data
    std::vector<uint8_t> request_data;
    auto payload_size = message.getPayloadSize();
    LOGV_FMT("Request payload size: " << payload_size);
    if (payload_size > 0) {
        request_data.resize(payload_size);
        ::memcpy(request_data.data(), message.getPayload(), payload_size);
    }

    // Invoke handler with timeout
    std::vector<uint8_t> response_data;
    bool handler_success = false;
    std::string error_message;

    try {
        // TODO: Implement timeout mechanism for handler execution
        // For now, execute directly
        response_data = handler(request_data);
        handler_success = true;
    } catch (const std::exception& e) {
        error_message = std::string("Handler exception: ") + e.what();
    }

    // Create response message
    Message response;
    if (handler_success) {
        response = message.createResponse();
        response.setType(MessageType::RESPONSE);
        if (!response_data.empty()) {
            response.setPayload(std::move(response_data));
        }
        // Update checksum after setting payload
        response.updateChecksum();
    } else {
        response = createErrorResponse(
            message,
            stub_error_codes::HANDLER_EXCEPTION,
            error_message
        );
    }

    return response;
}

bool ServiceStub::validateRequest(const Message& message) {
    // Default validation
    if (!message.validate()) {
        return false;
    }

    if (message.getType() != MessageType::REQUEST) {
        return false;
    }

    if (message.getSubject().empty()) {
        return false;
    }

    // Custom validation
    if (request_validator_) {
        return request_validator_(message);
    }

    return true;
}

bool ServiceStub::authenticateRequest(const Message& message) {
    if (auth_handler_) {
        return auth_handler_(message);
    }
    return true;  // No authentication required
}

Message ServiceStub::createErrorResponse(
    const Message& request,
    uint32_t error_code,
    const std::string& error_message
) {
    auto response = request.createErrorResponse(error_code, error_message);
    response.setSourceEndpoint(config_.service_name);
    response.setFormat(config_.serialization_format);
    // Update checksum for error response
    response.updateChecksum();
    return response;
}

void ServiceStub::sendResponse(const Message& response) {
    uint8_t corr_id[16];
    response.getCorrelationId(corr_id);
    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(corr_id[i]);
    }

    LOGV_FMT("Sending response - correlation_id: " << oss.str()
             << ", size: " << response.getTotalSize()
             << ", checksum: " << response.getHeader().checksum
             << ", is_error: " << response.isError());

    auto send_result = transport_->send(response);

    if (send_result.success()) {
        stats_.bytes_sent += response.getTotalSize();
        LOGV_FMT("Response sent successfully");
    } else {
        LOGE_FMT("Failed to send response: " << send_result.error_message);
    }
}

void ServiceStub::updateAvgProcessingTime(uint64_t processing_time_us) {
    // Simple moving average
    uint64_t current_avg = stats_.avg_processing_time_us;
    uint64_t total = stats_.total_requests;
    if (total > 0) {
        stats_.avg_processing_time_us = (current_avg * (total - 1) + processing_time_us) / total;
    } else {
        stats_.avg_processing_time_us = processing_time_us;
    }
}

bool ServiceStub::waitForPendingRequests(uint32_t timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeout_ms);

    while (stats_.active_handlers > 0) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout) {
            return false;  // Timeout
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;  // All completed
}

// StubFactory implementation

ServiceStubPtr StubFactory::createStub(const StubConfig& config) {
    try {
        return std::make_shared<ServiceStub>(config);
    } catch (const std::exception&) {
        return nullptr;
    }
}

ServiceStubPtr StubFactory::createAndStart(const StubConfig& config) {
    auto stub = createStub(config);
    if (stub && stub->start()) {
        return stub;
    }
    return nullptr;
}

} // namespace ipc
} // namespace cdmf
