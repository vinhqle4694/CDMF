/**
 * @file service_proxy.cpp
 * @brief Implementation of client-side proxy for RPC
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Stub Agent
 */

#include "ipc/service_proxy.h"
#include "utils/log.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>

namespace cdmf {
namespace ipc {

namespace {
    /**
     * @brief Convert call ID to string for map key
     */
    std::string callIdToString(const uint8_t* id) {
        std::ostringstream oss;
        for (int i = 0; i < 16; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(id[i]);
        }
        return oss.str();
    }

    /**
     * @brief Compare call IDs
     */
    [[maybe_unused]] bool callIdEquals(const uint8_t* id1, const uint8_t* id2) {
        return std::memcmp(id1, id2, 16) == 0;
    }
}

ServiceProxy::ServiceProxy(const ProxyConfig& config)
    : config_(config)
{
    LOGI_FMT("Creating ServiceProxy for service: " << config_.service_name
             << ", endpoint: " << config_.transport_config.endpoint);

    // Create transport
    transport_ = TransportFactory::create(config_.transport_config);
    if (!transport_) {
        LOGE_FMT("Failed to create transport for proxy");
        throw std::runtime_error("Failed to create transport for proxy");
    }

    // Create serializer
    serializer_ = SerializerFactory::createSerializer(config_.serialization_format);
    if (!serializer_) {
        LOGE_FMT("Failed to create serializer for proxy");
        throw std::runtime_error("Failed to create serializer for proxy");
    }

    LOGI_FMT("ServiceProxy created successfully");
    // Note: Transport will be initialized in connect()
}

ServiceProxy::~ServiceProxy() {
    LOGI_FMT("Destroying ServiceProxy");
    disconnect();
}

bool ServiceProxy::connect() {
    if (isConnected()) {
        LOGI_FMT("ServiceProxy already connected");
        return true;
    }

    LOGI_FMT("ServiceProxy connecting to " << config_.transport_config.endpoint);

    // Initialize transport if not already done
    if (transport_->getState() == TransportState::UNINITIALIZED) {
        LOGD_FMT("Initializing transport");
        auto init_result = transport_->init(config_.transport_config);
        if (!init_result.success()) {
            LOGE_FMT("Transport initialization failed: " << init_result.error_message);
            return false;
        }
    }

    // Start transport
    LOGD_FMT("Starting transport");
    auto start_result = transport_->start();
    if (!start_result.success()) {
        LOGE_FMT("Transport start failed: " << start_result.error_message);
        return false;
    }

    // Connect to remote endpoint
    LOGD_FMT("Connecting to remote endpoint");
    auto connect_result = transport_->connect();
    if (!connect_result.success()) {
        LOGE_FMT("Transport connect failed: " << connect_result.error_message);
        return false;
    }

    // Start background threads
    running_ = true;
    receive_thread_ = std::thread(&ServiceProxy::receiveThread, this);
    timeout_thread_ = std::thread(&ServiceProxy::timeoutThread, this);

    LOGI_FMT("ServiceProxy connected successfully");
    return true;
}

bool ServiceProxy::disconnect() {
    if (!running_) {
        LOGD_FMT("ServiceProxy already disconnected");
        return true;
    }

    LOGI_FMT("ServiceProxy disconnecting");

    // Stop background threads
    running_ = false;

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    if (timeout_thread_.joinable()) {
        timeout_thread_.join();
    }

    // Disconnect transport
    LOGD_FMT("Disconnecting transport");
    auto disconnect_result = transport_->disconnect();

    // Stop and cleanup transport
    transport_->stop();
    transport_->cleanup();

    // Reject all pending calls
    {
        std::lock_guard<std::mutex> lock(pending_calls_mutex_);
        for (auto& pair : pending_calls_) {
            try {
                pair.second->promise.set_exception(
                    std::make_exception_ptr(std::runtime_error("Proxy disconnected"))
                );
            } catch (...) {
                // Promise already satisfied
            }
        }
        pending_calls_.clear();
    }

    return disconnect_result.success();
}

bool ServiceProxy::isConnected() const {
    return transport_ && transport_->isConnected();
}

CallResult<std::vector<uint8_t>> ServiceProxy::call(
    const std::string& method_name,
    const void* request_data,
    uint32_t request_size,
    uint32_t timeout_ms
) {
    auto timeout = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : config_.default_timeout_ms);
    return sendAndReceive(method_name, request_data, request_size, timeout);
}

CallResult<std::vector<uint8_t>> ServiceProxy::call(
    const std::string& method_name,
    const std::vector<uint8_t>& request_data,
    uint32_t timeout_ms
) {
    return call(method_name, request_data.data(), static_cast<uint32_t>(request_data.size()), timeout_ms);
}

std::future<CallResult<std::vector<uint8_t>>> ServiceProxy::callAsync(
    const std::string& method_name,
    const void* request_data,
    uint32_t request_size,
    uint32_t timeout_ms
) {
    return std::async(std::launch::async, [this, method_name, request_data, request_size, timeout_ms]() {
        return call(method_name, request_data, request_size, timeout_ms);
    });
}

std::future<CallResult<std::vector<uint8_t>>> ServiceProxy::callAsync(
    const std::string& method_name,
    const std::vector<uint8_t>& request_data,
    uint32_t timeout_ms
) {
    return callAsync(method_name, request_data.data(), static_cast<uint32_t>(request_data.size()), timeout_ms);
}

void ServiceProxy::callAsync(
    const std::string& method_name,
    const std::vector<uint8_t>& request_data,
    AsyncCallback<std::vector<uint8_t>> callback,
    uint32_t timeout_ms
) {
    auto future = std::async(std::launch::async, [this, method_name, request_data, callback, timeout_ms]() {
        auto result = call(method_name, request_data, timeout_ms);
        callback(result);
    });
    // Store future to avoid it being destroyed immediately
    (void)future;
}

bool ServiceProxy::callOneWay(
    const std::string& method_name,
    const void* request_data,
    uint32_t request_size
) {
    LOGD_FMT("One-way call to method: " << method_name << ", size: " << request_size);

    if (!isConnected()) {
        LOGE_FMT("One-way call failed: Not connected");
        return false;
    }

    // Create one-way request (no response expected)
    auto request = createRequest(method_name, request_data, request_size, CallType::ONEWAY);

    // Send request
    auto send_result = transport_->send(std::move(request));

    stats_.total_calls++;
    if (send_result.success()) {
        stats_.successful_calls++;
        LOGI_FMT("One-way call sent successfully for method: " << method_name);
        return true;
    } else {
        stats_.failed_calls++;
        LOGE_FMT("One-way call failed: " << send_result.error_message);
        return false;
    }
}

bool ServiceProxy::callOneWay(
    const std::string& method_name,
    const std::vector<uint8_t>& request_data
) {
    return callOneWay(method_name, request_data.data(), static_cast<uint32_t>(request_data.size()));
}

const ProxyConfig& ServiceProxy::getConfig() const {
    return config_;
}

void ServiceProxy::setRetryPolicy(const RetryPolicy& policy) {
    config_.retry_policy = policy;
}

void ServiceProxy::setDefaultTimeout(uint32_t timeout_ms) {
    config_.default_timeout_ms = timeout_ms;
}

ProxyStatsSnapshot ServiceProxy::getStats() const {
    ProxyStatsSnapshot snapshot;
    snapshot.total_calls = stats_.total_calls.load();
    snapshot.successful_calls = stats_.successful_calls.load();
    snapshot.failed_calls = stats_.failed_calls.load();
    snapshot.timeout_calls = stats_.timeout_calls.load();
    snapshot.total_retries = stats_.total_retries.load();
    snapshot.avg_response_time_us = stats_.avg_response_time_us.load();
    snapshot.active_calls = stats_.active_calls.load();
    return snapshot;
}

void ServiceProxy::resetStats() {
    stats_.total_calls = 0;
    stats_.successful_calls = 0;
    stats_.failed_calls = 0;
    stats_.timeout_calls = 0;
    stats_.total_retries = 0;
    stats_.avg_response_time_us = 0;
}

uint32_t ServiceProxy::getActiveCalls() const {
    return stats_.active_calls;
}

CallResult<std::vector<uint8_t>> ServiceProxy::sendAndReceive(
    const std::string& method_name,
    const void* request_data,
    uint32_t request_size,
    std::chrono::milliseconds timeout,
    uint32_t retry_count
) {
    CallResult<std::vector<uint8_t>> result;
    auto start_time = std::chrono::steady_clock::now();

    LOGV_FMT("Calling method: " << method_name << ", size: " << request_size
             << ", timeout: " << timeout.count() << "ms, retry: " << retry_count);

    if (!isConnected()) {
        LOGE_FMT("Call failed: Not connected to service");
        result.error_code = 1;
        result.error_message = "Not connected to service";
        result.retry_count = retry_count;
        return result;
    }

    stats_.total_calls++;
    stats_.active_calls++;

    try {
        // Create request message
        auto request = createRequest(method_name, request_data, request_size, CallType::SYNC);
        uint8_t call_id[16];
        request.getMessageId(call_id);

        std::string call_id_str = callIdToString(call_id);
        LOGV_FMT("Created request with call_id: " << call_id_str);

        // Register pending call
        auto pending_call = registerPendingCall(call_id, method_name, timeout);

        // Send request
        LOGV_FMT("Sending request for method: " << method_name);
        auto send_result = transport_->send(std::move(request));
        if (!send_result.success()) {
            LOGE_FMT("Send failed: " << send_result.error_message);
            unregisterPendingCall(call_id);

            // Retry if enabled
            if (config_.retry_policy.enabled && retry_count < config_.retry_policy.max_attempts) {
                stats_.total_retries++;
                auto delay = calculateRetryDelay(retry_count);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                stats_.active_calls--;
                return sendAndReceive(method_name, request_data, request_size, timeout, retry_count + 1);
            }

            result.error_code = 2;
            result.error_message = "Failed to send request: " + send_result.error_message;
            result.retry_count = retry_count;
            stats_.failed_calls++;
            stats_.active_calls--;
            return result;
        }

        // Wait for response
        LOGV_FMT("Waiting for response, timeout: " << timeout.count() << "ms");
        auto future = pending_call->promise.get_future();
        auto status = future.wait_for(timeout);

        if (status == std::future_status::timeout) {
            LOGW_FMT("Request timeout for method: " << method_name << ", call_id: " << call_id_str);
            unregisterPendingCall(call_id);

            // Retry if enabled
            if (config_.retry_policy.enabled && retry_count < config_.retry_policy.max_attempts) {
                stats_.total_retries++;
                auto delay = calculateRetryDelay(retry_count);
                LOGI_FMT("Retrying after " << delay << "ms delay");
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                stats_.active_calls--;
                return sendAndReceive(method_name, request_data, request_size, timeout, retry_count + 1);
            }

            result.error_code = 3;
            result.error_message = "Request timeout";
            result.retry_count = retry_count;
            stats_.timeout_calls++;
            stats_.failed_calls++;
            stats_.active_calls--;
            return result;
        }

        // Get response message
        LOGV_FMT("Response received for method: " << method_name);
        MessagePtr response;
        try {
            response = future.get();
        } catch (const std::exception& e) {
            LOGE_FMT("Error getting response: " << e.what());
            result.error_code = 4;
            result.error_message = std::string("Error receiving response: ") + e.what();
            result.retry_count = retry_count;
            stats_.failed_calls++;
            stats_.active_calls--;
            return result;
        }

        // Check if error response
        if (response->isError()) {
            const auto& error_info = response->getErrorInfo();
            LOGE_FMT("Error response received, code: " << error_info.error_code
                     << ", message: " << error_info.error_message);
            result.error_code = error_info.error_code;
            result.error_message = error_info.error_message;
            result.retry_count = retry_count;
            stats_.failed_calls++;
            stats_.active_calls--;
            return result;
        }

        // Extract response data
        auto response_size = response->getPayloadSize();
        LOGV_FMT("Response payload size: " << response_size);
        if (response_size > 0) {
            result.data.resize(response_size);
            std::memcpy(result.data.data(), response->getPayload(), response_size);
        }

        result.success = true;
        stats_.successful_calls++;
        LOGV_FMT("Call successful for method: " << method_name << ", response size: " << response_size);

    } catch (const std::exception& e) {
        LOGE_FMT("Exception in sendAndReceive: " << e.what());
        result.error_code = 999;
        result.error_message = std::string("Exception: ") + e.what();
        result.retry_count = retry_count;
        stats_.failed_calls++;
    }

    stats_.active_calls--;

    // Update duration and response time
    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto response_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    updateAvgResponseTime(response_time_us);

    LOGV_FMT("Call completed in " << result.duration.count() << "ms");
    return result;
}

Message ServiceProxy::createRequest(
    const std::string& method_name,
    const void* request_data,
    uint32_t request_size,
    CallType call_type
) {
    Message request(MessageType::REQUEST);

    // Generate unique message ID
    generateCallId(request.getHeader().message_id);
    std::string msg_id = callIdToString(request.getHeader().message_id);

    // Set metadata
    request.setSubject(method_name);
    request.setSourceEndpoint(config_.service_name);
    request.updateTimestamp();
    request.setFormat(config_.serialization_format);

    // Set payload
    if (request_data && request_size > 0) {
        request.setPayload(request_data, request_size);
    }

    // Update checksum after setting payload
    request.updateChecksum();

    LOGV_FMT("Created request message - ID: " << msg_id
             << ", method: " << method_name
             << ", payload_size: " << request_size
             << ", checksum: " << request.getHeader().checksum);

    // For one-way calls, set flag
    if (call_type == CallType::ONEWAY) {
        LOGV_FMT("One-way call (no response expected)");
    }

    return request;
}

void ServiceProxy::generateCallId(uint8_t* id) {
    // Generate pseudo-unique ID using counter and random data
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;

    uint64_t counter = call_id_counter_++;
    uint64_t random = dist(gen);

    std::memcpy(id, &counter, 8);
    std::memcpy(id + 8, &random, 8);
}

std::shared_ptr<PendingCall> ServiceProxy::registerPendingCall(
    const uint8_t* call_id,
    const std::string& method_name,
    std::chrono::milliseconds timeout
) {
    auto pending_call = std::make_shared<PendingCall>();
    std::memcpy(pending_call->call_id, call_id, 16);
    pending_call->method_name = method_name;
    pending_call->start_time = std::chrono::steady_clock::now();
    pending_call->timeout = timeout;

    std::string call_id_str = callIdToString(call_id);
    std::lock_guard<std::mutex> lock(pending_calls_mutex_);
    pending_calls_[call_id_str] = pending_call;

    LOGV_FMT("Registered pending call - ID: " << call_id_str
             << ", method: " << method_name
             << ", timeout: " << timeout.count() << "ms"
             << ", total_pending: " << pending_calls_.size());

    return pending_call;
}

void ServiceProxy::unregisterPendingCall(const uint8_t* call_id) {
    std::string call_id_str = callIdToString(call_id);
    std::lock_guard<std::mutex> lock(pending_calls_mutex_);
    pending_calls_.erase(call_id_str);
    LOGV_FMT("Unregistered pending call - ID: " << call_id_str
             << ", remaining: " << pending_calls_.size());
}

void ServiceProxy::handleResponse(MessagePtr message) {
    uint8_t correlation_id[16];
    message->getCorrelationId(correlation_id);
    std::string corr_id_str = callIdToString(correlation_id);

    LOGV_FMT("Handling response with correlation_id: " << corr_id_str);

    std::shared_ptr<PendingCall> pending_call;
    {
        std::lock_guard<std::mutex> lock(pending_calls_mutex_);
        auto it = pending_calls_.find(corr_id_str);
        if (it != pending_calls_.end()) {
            pending_call = it->second;
            pending_calls_.erase(it);
            LOGV_FMT("Found pending call for correlation_id: " << corr_id_str);
        } else {
            LOGW_FMT("No pending call found for correlation_id: " << corr_id_str);
        }
    }

    if (pending_call) {
        try {
            pending_call->promise.set_value(message);
            LOGV_FMT("Response delivered to pending call");
        } catch (...) {
            LOGW_FMT("Failed to set promise value (already satisfied or destroyed)");
        }
    }
}

void ServiceProxy::receiveThread() {
    LOGI_FMT("Receive thread started");
    while (running_) {
        try {
            // Try to receive response
            auto result = transport_->receive(100);  // 100ms timeout

            if (result.success() && result.value) {
                LOGV_FMT("Message received in receive thread");
                // Check if it's a response message
                if (result.value->getType() == MessageType::RESPONSE ||
                    result.value->getType() == MessageType::ERROR) {
                    handleResponse(result.value);
                }
            }
        } catch (const std::exception& e) {
            LOGE_FMT("Exception in receive thread: " << e.what());
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    LOGI_FMT("Receive thread stopped");
}

void ServiceProxy::timeoutThread() {
    LOGI_FMT("Timeout thread started");
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> timed_out_calls;

        {
            std::lock_guard<std::mutex> lock(pending_calls_mutex_);
            for (const auto& pair : pending_calls_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - pair.second->start_time
                );
                if (elapsed >= pair.second->timeout) {
                    timed_out_calls.push_back(pair.first);
                }
            }

            for (const auto& call_id : timed_out_calls) {
                auto it = pending_calls_.find(call_id);
                if (it != pending_calls_.end()) {
                    LOGW_FMT("Timeout detected for call_id: " << call_id
                             << ", method: " << it->second->method_name);
                    try {
                        it->second->promise.set_exception(
                            std::make_exception_ptr(std::runtime_error("Request timeout"))
                        );
                    } catch (...) {
                        LOGV_FMT("Promise already satisfied for call_id: " << call_id);
                    }
                    pending_calls_.erase(it);
                }
            }
        }
    }
    LOGI_FMT("Timeout thread stopped");
}

uint32_t ServiceProxy::calculateRetryDelay(uint32_t attempt) const {
    if (!config_.retry_policy.exponential_backoff) {
        LOGV_FMT("Retry delay (fixed): " << config_.retry_policy.initial_delay_ms << "ms");
        return config_.retry_policy.initial_delay_ms;
    }

    uint32_t delay = config_.retry_policy.initial_delay_ms;
    for (uint32_t i = 0; i < attempt; ++i) {
        delay = static_cast<uint32_t>(delay * config_.retry_policy.backoff_multiplier);
        if (delay > config_.retry_policy.max_delay_ms) {
            delay = config_.retry_policy.max_delay_ms;
            break;
        }
    }
    LOGV_FMT("Retry delay (exponential) for attempt " << attempt << ": " << delay << "ms");
    return delay;
}

void ServiceProxy::updateAvgResponseTime(uint64_t response_time_us) {
    // Simple moving average
    uint64_t current_avg = stats_.avg_response_time_us;
    uint64_t total = stats_.total_calls;
    if (total > 0) {
        stats_.avg_response_time_us = (current_avg * (total - 1) + response_time_us) / total;
    } else {
        stats_.avg_response_time_us = response_time_us;
    }
    LOGV_FMT("Updated avg response time: " << stats_.avg_response_time_us.load()
             << "us (current: " << response_time_us << "us)");
}

} // namespace ipc
} // namespace cdmf
