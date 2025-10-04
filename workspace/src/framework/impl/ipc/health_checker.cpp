/**
 * @file health_checker.cpp
 * @brief Implementation of health checking infrastructure
 */

#include "ipc/health_checker.h"
#include "ipc/message.h"
#include "utils/log.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <thread>

namespace cdmf {
namespace ipc {

/**
 * @brief Endpoint health state
 */
struct EndpointHealthState {
    std::string endpoint;
    TransportPtr transport;
    HealthStatus status = HealthStatus::UNKNOWN;
    HealthCheckStats stats;

    // Passive monitoring data
    std::deque<bool> request_results;  // true = success, false = failure

    std::chrono::steady_clock::time_point last_check_time;
    std::chrono::steady_clock::time_point last_status_change_time;

    mutable std::mutex mutex;
};

/**
 * @brief HealthChecker implementation
 */
class HealthChecker::Impl {
public:
    explicit Impl(const HealthCheckConfig& config)
        : config_(config)
        , running_(false)
        , should_stop_(false) {
    }

    ~Impl() {
        stop();
    }

    bool start() {
        LOGI_FMT("HealthChecker::start called");
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            LOGW_FMT("HealthChecker::start: already running");
            return false;
        }

        should_stop_ = false;
        running_ = true;

        if (config_.enable_active_checks) {
            check_thread_ = std::thread(&Impl::healthCheckLoop, this);
            LOGD_FMT("HealthChecker::start: health check thread started");
        }

        LOGI_FMT("HealthChecker::start: started successfully");
        return true;
    }

    void stop() {
        LOGI_FMT("HealthChecker::stop called");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                LOGD_FMT("HealthChecker::stop: already stopped");
                return;
            }

            should_stop_ = true;
            running_ = false;
        }

        cv_.notify_all();

        // Join the thread with a timeout
        if (check_thread_.joinable()) {
            LOGD_FMT("HealthChecker::stop: waiting for health check thread to finish");

            // Try to join for a reasonable time
            auto future = std::async(std::launch::async, [this]() {
                if (check_thread_.joinable()) {
                    check_thread_.join();
                }
            });

            if (future.wait_for(std::chrono::milliseconds(2000)) == std::future_status::timeout) {
                // Thread didn't finish, detach it as last resort
                LOGW_FMT("HealthChecker::stop: health check thread did not stop in time, detaching");
                if (check_thread_.joinable()) {
                    check_thread_.detach();
                }
            } else {
                LOGD_FMT("HealthChecker::stop: health check thread stopped successfully");
            }
        }

        LOGI_FMT("HealthChecker::stop: stopped successfully");
    }

    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    bool addEndpoint(const std::string& endpoint, TransportPtr transport) {
        LOGD_FMT("HealthChecker::addEndpoint called for endpoint=" << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        if (endpoints_.find(endpoint) != endpoints_.end()) {
            LOGW_FMT("HealthChecker::addEndpoint: endpoint already exists");
            return false;  // Already exists
        }

        auto state = std::make_unique<EndpointHealthState>();
        state->endpoint = endpoint;
        state->transport = transport;
        state->status = HealthStatus::UNKNOWN;
        state->last_check_time = std::chrono::steady_clock::now();
        state->last_status_change_time = state->last_check_time;

        endpoints_[endpoint] = std::move(state);
        LOGI_FMT("HealthChecker::addEndpoint: endpoint added, total=" << endpoints_.size());
        return true;
    }

    bool removeEndpoint(const std::string& endpoint) {
        LOGD_FMT("HealthChecker::removeEndpoint called for endpoint=" << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);
        bool result = endpoints_.erase(endpoint) > 0;
        if (result) {
            LOGD_FMT("HealthChecker::removeEndpoint: endpoint removed, total=" << endpoints_.size());
        } else {
            LOGW_FMT("HealthChecker::removeEndpoint: endpoint not found");
        }
        return result;
    }

    bool checkNow(const std::string& endpoint) {
        LOGD_FMT("HealthChecker::checkNow called for endpoint=" << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("HealthChecker::checkNow: endpoint not found");
            return false;
        }

        bool result = performHealthCheck(*it->second);
        LOGD_FMT("HealthChecker::checkNow: health check result=" << (result ? "healthy" : "unhealthy"));
        return result;
    }

    HealthStatus getStatus(const std::string& endpoint) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            return HealthStatus::UNKNOWN;
        }

        std::lock_guard<std::mutex> state_lock(it->second->mutex);
        return it->second->status;
    }

    bool isHealthy(const std::string& endpoint) const {
        return getStatus(endpoint) == HealthStatus::HEALTHY;
    }

    void recordSuccess(const std::string& endpoint) {
        LOGD_FMT("HealthChecker::recordSuccess called for endpoint=" << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("HealthChecker::recordSuccess: endpoint not found");
            return;
        }

        recordPassiveResult(*it->second, true);
    }

    void recordFailure(const std::string& endpoint) {
        LOGD_FMT("HealthChecker::recordFailure called for endpoint=" << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("HealthChecker::recordFailure: endpoint not found");
            return;
        }

        recordPassiveResult(*it->second, false);
    }

    HealthCheckStats getStats(const std::string& endpoint) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            return HealthCheckStats{};
        }

        std::lock_guard<std::mutex> state_lock(it->second->mutex);
        return it->second->stats;
    }

    void resetStats(const std::string& endpoint) {
        LOGD_FMT("HealthChecker::resetStats called for endpoint=" << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("HealthChecker::resetStats: endpoint not found");
            return;
        }

        std::lock_guard<std::mutex> state_lock(it->second->mutex);
        it->second->stats = HealthCheckStats{};
        it->second->stats.current_status = it->second->status;
        LOGD_FMT("HealthChecker::resetStats: statistics reset for endpoint");
    }

    void setStatusChangeCallback(StatusChangeCallback callback) {
        LOGD_FMT("HealthChecker::setStatusChangeCallback called");
        std::lock_guard<std::mutex> lock(callback_mutex_);
        status_change_callback_ = std::move(callback);
    }

    void setCustomCheckCallback(CustomCheckCallback callback) {
        LOGD_FMT("HealthChecker::setCustomCheckCallback called");
        std::lock_guard<std::mutex> lock(callback_mutex_);
        custom_check_callback_ = std::move(callback);
    }

    void updateConfig(const HealthCheckConfig& config) {
        LOGI_FMT("HealthChecker::updateConfig called");
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        LOGD_FMT("HealthChecker::updateConfig: configuration updated");
    }

    HealthCheckConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

private:
    void healthCheckLoop() {
        while (!should_stop_) {
            // auto check_start = std::chrono::steady_clock::now();

            // Perform health checks on all endpoints
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [endpoint, state] : endpoints_) {
                    performHealthCheck(*state);
                }
            }

            // Wait for next check interval
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, config_.check_interval,
                        [this] { return should_stop_.load(); });
        }
    }

    bool performHealthCheck(EndpointHealthState& state) {
        auto check_start = std::chrono::steady_clock::now();
        bool result = false;

        switch (config_.strategy) {
            case HealthCheckStrategy::TCP_CONNECT:
                result = checkTcpConnect(state);
                break;

            case HealthCheckStrategy::APPLICATION_PING:
                result = checkApplicationPing(state);
                break;

            case HealthCheckStrategy::PASSIVE_MONITORING:
                result = checkPassiveMonitoring(state);
                break;

            case HealthCheckStrategy::CUSTOM:
                result = checkCustom(state);
                break;
        }

        auto check_end = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
            check_end - check_start);

        // Update statistics
        std::lock_guard<std::mutex> lock(state.mutex);
        state.stats.total_checks++;
        state.stats.last_check_time = check_end;
        state.stats.last_check_latency = latency;

        if (result) {
            state.stats.successful_checks++;
            state.stats.consecutive_successes++;
            state.stats.consecutive_failures = 0;
        } else {
            state.stats.failed_checks++;
            state.stats.consecutive_failures++;
            state.stats.consecutive_successes = 0;
        }

        // Update average latency
        state.stats.avg_check_latency = std::chrono::microseconds(
            (state.stats.avg_check_latency.count() * (state.stats.total_checks - 1) +
             latency.count()) / state.stats.total_checks);

        // Update health status based on thresholds
        updateHealthStatus(state, result);

        return result;
    }

    bool checkTcpConnect(EndpointHealthState& state) {
        if (!state.transport) {
            return false;
        }

        // Check if transport is connected
        if (state.transport->isConnected()) {
            return true;
        }

        // Try to connect
        auto result = state.transport->connect();
        return result.success();
    }

    bool checkApplicationPing(EndpointHealthState& state) {
        if (!state.transport || !state.transport->isConnected()) {
            return false;
        }

        // Create ping message
        Message ping(MessageType::HEARTBEAT);
        ping.setSubject("health_check_ping");
        ping.updateTimestamp();

        // Send ping
        auto send_result = state.transport->send(ping);
        if (!send_result.success()) {
            return false;
        }

        // Wait for pong response
        auto recv_result = state.transport->receive(
            config_.check_timeout.count());

        if (!recv_result.success()) {
            return false;
        }

        // Verify it's a pong
        auto response = recv_result.value;
        return response &&
               response->getType() == MessageType::HEARTBEAT &&
               response->getSubject() == "health_check_pong";
    }

    bool checkPassiveMonitoring(EndpointHealthState& state) {
        std::lock_guard<std::mutex> lock(state.mutex);

        if (state.request_results.size() < config_.min_requests_for_rate) {
            // Not enough data, assume healthy
            return true;
        }

        // Calculate failure rate
        uint32_t failures = std::count(state.request_results.begin(),
                                      state.request_results.end(), false);
        double failure_rate = static_cast<double>(failures) /
                            state.request_results.size();

        state.stats.current_failure_rate = failure_rate;

        return failure_rate < config_.unhealthy_failure_rate;
    }

    bool checkCustom(EndpointHealthState& state) {
        std::lock_guard<std::mutex> lock(callback_mutex_);

        if (!custom_check_callback_) {
            return false;
        }

        return custom_check_callback_(state.endpoint);
    }

    void updateHealthStatus(EndpointHealthState& state, bool check_result) {
        std::lock_guard<std::mutex> lock(state.mutex);

        HealthStatus old_status = state.status;
        HealthStatus new_status = old_status;

        if (check_result) {
            // Success path
            if (state.stats.consecutive_successes >= config_.healthy_threshold) {
                new_status = HealthStatus::HEALTHY;
            } else if (old_status == HealthStatus::UNHEALTHY) {
                new_status = HealthStatus::DEGRADED;
            }
        } else {
            // Failure path
            if (state.stats.consecutive_failures >= config_.unhealthy_threshold) {
                new_status = HealthStatus::UNHEALTHY;
            } else if (old_status == HealthStatus::HEALTHY) {
                new_status = HealthStatus::DEGRADED;
            }
        }

        if (new_status != old_status) {
            state.status = new_status;
            state.stats.current_status = new_status;
            state.stats.last_status_change_time = std::chrono::steady_clock::now();

            // Notify status change
            notifyStatusChange(state.endpoint, old_status, new_status);
        }
    }

    void recordPassiveResult(EndpointHealthState& state, bool success) {
        if (!config_.enable_passive_monitoring) {
            return;
        }

        std::lock_guard<std::mutex> lock(state.mutex);

        // Add result to rolling window
        state.request_results.push_back(success);

        // Trim to window size
        while (state.request_results.size() > config_.passive_window_size) {
            state.request_results.pop_front();
        }

        // Calculate failure rate if we have enough data
        if (state.request_results.size() >= config_.min_requests_for_rate) {
            uint32_t failures = std::count(state.request_results.begin(),
                                          state.request_results.end(), false);
            double failure_rate = static_cast<double>(failures) /
                                state.request_results.size();

            state.stats.current_failure_rate = failure_rate;

            // Update status based on failure rate
            HealthStatus old_status = state.status;
            HealthStatus new_status = old_status;

            if (failure_rate >= config_.unhealthy_failure_rate) {
                new_status = HealthStatus::UNHEALTHY;
            } else if (failure_rate >= config_.degraded_threshold) {
                new_status = HealthStatus::DEGRADED;
            } else {
                new_status = HealthStatus::HEALTHY;
            }

            if (new_status != old_status) {
                state.status = new_status;
                state.stats.current_status = new_status;
                state.stats.last_status_change_time = std::chrono::steady_clock::now();

                notifyStatusChange(state.endpoint, old_status, new_status);
            }
        }
    }

    void notifyStatusChange(const std::string& endpoint,
                          HealthStatus old_status,
                          HealthStatus new_status) {
        std::lock_guard<std::mutex> lock(callback_mutex_);

        if (status_change_callback_) {
            status_change_callback_(endpoint, old_status, new_status);
        }
    }

    HealthCheckConfig config_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;

    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<EndpointHealthState>> endpoints_;

    std::thread check_thread_;
    std::condition_variable cv_;

    mutable std::mutex callback_mutex_;
    StatusChangeCallback status_change_callback_;
    CustomCheckCallback custom_check_callback_;
};

// HealthChecker implementation

HealthChecker::HealthChecker(const HealthCheckConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {
}

HealthChecker::~HealthChecker() = default;

HealthChecker::HealthChecker(HealthChecker&&) noexcept = default;
HealthChecker& HealthChecker::operator=(HealthChecker&&) noexcept = default;

bool HealthChecker::start() {
    return pImpl_->start();
}

void HealthChecker::stop() {
    pImpl_->stop();
}

bool HealthChecker::isRunning() const {
    return pImpl_->isRunning();
}

bool HealthChecker::addEndpoint(const std::string& endpoint, TransportPtr transport) {
    return pImpl_->addEndpoint(endpoint, transport);
}

bool HealthChecker::removeEndpoint(const std::string& endpoint) {
    return pImpl_->removeEndpoint(endpoint);
}

bool HealthChecker::checkNow(const std::string& endpoint) {
    return pImpl_->checkNow(endpoint);
}

HealthStatus HealthChecker::getStatus(const std::string& endpoint) const {
    return pImpl_->getStatus(endpoint);
}

bool HealthChecker::isHealthy(const std::string& endpoint) const {
    return pImpl_->isHealthy(endpoint);
}

void HealthChecker::recordSuccess(const std::string& endpoint) {
    pImpl_->recordSuccess(endpoint);
}

void HealthChecker::recordFailure(const std::string& endpoint) {
    pImpl_->recordFailure(endpoint);
}

HealthCheckStats HealthChecker::getStats(const std::string& endpoint) const {
    return pImpl_->getStats(endpoint);
}

void HealthChecker::resetStats(const std::string& endpoint) {
    pImpl_->resetStats(endpoint);
}

void HealthChecker::setStatusChangeCallback(StatusChangeCallback callback) {
    pImpl_->setStatusChangeCallback(std::move(callback));
}

void HealthChecker::setCustomCheckCallback(CustomCheckCallback callback) {
    pImpl_->setCustomCheckCallback(std::move(callback));
}

void HealthChecker::updateConfig(const HealthCheckConfig& config) {
    pImpl_->updateConfig(config);
}

HealthCheckConfig HealthChecker::getConfig() const {
    return pImpl_->getConfig();
}

// HealthCheckerBuilder implementation

HealthCheckerBuilder::HealthCheckerBuilder() {
    config_ = HealthCheckConfig{};
}

HealthCheckerBuilder& HealthCheckerBuilder::withStrategy(HealthCheckStrategy strategy) {
    config_.strategy = strategy;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withCheckInterval(std::chrono::milliseconds interval) {
    config_.check_interval = interval;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withCheckTimeout(std::chrono::milliseconds timeout) {
    config_.check_timeout = timeout;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withUnhealthyThreshold(uint32_t threshold) {
    config_.unhealthy_threshold = threshold;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withHealthyThreshold(uint32_t threshold) {
    config_.healthy_threshold = threshold;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::enableActiveChecks(bool enable) {
    config_.enable_active_checks = enable;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::enablePassiveMonitoring(bool enable) {
    config_.enable_passive_monitoring = enable;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withPassiveWindowSize(uint32_t size) {
    config_.passive_window_size = size;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withDegradedThreshold(double threshold) {
    config_.degraded_threshold = threshold;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withUnhealthyFailureRate(double rate) {
    config_.unhealthy_failure_rate = rate;
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::onStatusChange(
    HealthChecker::StatusChangeCallback callback) {
    status_change_callback_ = std::move(callback);
    return *this;
}

HealthCheckerBuilder& HealthCheckerBuilder::withCustomCheck(
    HealthChecker::CustomCheckCallback callback) {
    custom_check_callback_ = std::move(callback);
    return *this;
}

std::unique_ptr<HealthChecker> HealthCheckerBuilder::build() {
    auto checker = std::make_unique<HealthChecker>(config_);

    if (status_change_callback_) {
        checker->setStatusChangeCallback(std::move(status_change_callback_));
    }

    if (custom_check_callback_) {
        checker->setCustomCheckCallback(std::move(custom_check_callback_));
    }

    return checker;
}

} // namespace ipc
} // namespace cdmf
