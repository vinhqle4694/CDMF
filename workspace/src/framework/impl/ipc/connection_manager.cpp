/**
 * @file connection_manager.cpp
 * @brief Implementation of central connection management infrastructure
 */

#include "ipc/connection_manager.h"
#include "utils/log.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <vector>
#include <thread>

namespace cdmf {
namespace ipc {

/**
 * @brief Per-endpoint management state
 */
struct EndpointState {
    std::string endpoint;
    EndpointConfig config;

    // Components
    std::unique_ptr<ConnectionPool> pool;
    std::unique_ptr<HealthChecker> health_checker;
    std::unique_ptr<CircuitBreaker> circuit_breaker;
    std::unique_ptr<RetryPolicy> retry_policy;

    // Statistics
    ConnectionPoolStats pool_stats;
    HealthCheckStats health_stats;
    CircuitBreakerStats circuit_stats;
    RetryStats retry_stats;

    std::chrono::steady_clock::time_point last_success_time;
    std::chrono::steady_clock::time_point last_failure_time;

    mutable std::mutex mutex;
};

/**
 * @brief ConnectionManager implementation
 */
class ConnectionManager::Impl {
public:
    Impl()
        : running_(false)
        , shutdown_in_progress_(false) {
        LOGD_FMT("ConnectionManager::Impl constructed");
    }

    ~Impl() {
        LOGD_FMT("ConnectionManager::Impl destructor called");
        stop(true);
    }

    bool start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            LOGW_FMT("ConnectionManager already running");
            return false;
        }

        LOGI_FMT("Starting ConnectionManager with " << endpoints_.size() << " endpoints");

        // Start all components
        for (auto& [endpoint, state] : endpoints_) {
            if (state->pool && state->config.enable_pooling) {
                LOGD_FMT("Starting connection pool for endpoint: " << endpoint);
                state->pool->start();
            }

            if (state->health_checker && state->config.enable_health_check) {
                LOGD_FMT("Starting health checker for endpoint: " << endpoint);
                state->health_checker->start();
            }
        }

        running_ = true;
        LOGI_FMT("ConnectionManager started successfully");
        return true;
    }

    void stop(bool graceful) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                LOGD_FMT("ConnectionManager not running, nothing to stop");
                return;
            }

            LOGI_FMT("Stopping ConnectionManager (graceful=" << graceful << ")");
            shutdown_in_progress_ = true;
            running_ = false;
        }

        if (graceful) {
            LOGD_FMT("Draining connections gracefully");
            // Drain connections
            drainConnections();
        }

        // Stop all components
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [endpoint, state] : endpoints_) {
            if (state->pool) {
                LOGD_FMT("Stopping pool for endpoint: " << endpoint);
                state->pool->stop();
            }

            if (state->health_checker) {
                LOGD_FMT("Stopping health checker for endpoint: " << endpoint);
                state->health_checker->stop();
            }
        }

        shutdown_in_progress_ = false;
        shutdown_cv_.notify_all();
        LOGI_FMT("ConnectionManager stopped successfully");
    }

    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        LOGD_FMT("ConnectionManager::isRunning - returning: " << running_);
        return running_;
    }

    bool registerEndpoint(const EndpointConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        LOGI_FMT("Registering endpoint: " << config.endpoint);

        if (endpoints_.find(config.endpoint) != endpoints_.end()) {
            LOGW_FMT("Endpoint already registered: " << config.endpoint);
            return false;  // Already registered
        }

        auto state = std::make_unique<EndpointState>();
        state->endpoint = config.endpoint;
        state->config = config;

        // Create connection pool
        if (config.enable_pooling) {
            auto factory = [config](const std::string& /* endpoint */) -> TransportPtr {
                auto transport = TransportFactory::create(config.transport_config);
                if (transport) {
                    transport->init(config.transport_config);
                    transport->connect();
                }
                return transport;
            };

            state->pool = std::make_unique<ConnectionPool>(
                config.pool_config, factory);

            if (running_) {
                state->pool->start();
            }
        }

        // Create health checker
        if (config.enable_health_check) {
            state->health_checker = std::make_unique<HealthChecker>(
                config.health_config);

            // Set up status change callback
            state->health_checker->setStatusChangeCallback(
                [this, endpoint = config.endpoint](
                    const std::string& /* ep */, HealthStatus old_status,
                    HealthStatus new_status) {
                    handleHealthStatusChange(endpoint, old_status, new_status);
                });

            if (running_) {
                state->health_checker->start();
            }

            // Add endpoint to health checker
            // Note: For pooled connections, we'll add transport on demand
            state->health_checker->addEndpoint(config.endpoint);
        }

        // Create circuit breaker
        if (config.enable_circuit_breaker) {
            state->circuit_breaker = std::make_unique<CircuitBreaker>(
                config.circuit_config);

            // Set up state change callback
            state->circuit_breaker->setStateChangeCallback(
                [this, endpoint = config.endpoint](
                    CircuitState old_state, CircuitState new_state) {
                    handleCircuitStateChange(endpoint, old_state, new_state);
                });
        }

        // Create retry policy
        if (config.enable_retry) {
            state->retry_policy = std::make_unique<RetryPolicy>(
                config.retry_config);
        }

        endpoints_[config.endpoint] = std::move(state);
        LOGI_FMT("Endpoint registered successfully: " << config.endpoint);
        return true;
    }

    bool unregisterEndpoint(const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(mutex_);

        LOGI_FMT("Unregistering endpoint: " << endpoint);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for unregistration: " << endpoint);
            return false;
        }

        // Stop components
        if (it->second->pool) {
            it->second->pool->stop();
        }

        if (it->second->health_checker) {
            it->second->health_checker->stop();
        }

        endpoints_.erase(it);
        return true;
    }

    PooledConnection getConnection(const std::string& endpoint,
                                   std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);

        LOGD_FMT("Requesting connection for endpoint: " << endpoint << ", timeout: " << timeout.count() << "ms");

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end() || !it->second->pool) {
            LOGE_FMT("Endpoint not found or pool not available: " << endpoint);
            return PooledConnection(nullptr, {});
        }

        auto& state = *it->second;

        // Check health status
        if (state.health_checker) {
            auto health = state.health_checker->getStatus(endpoint);
            if (health == HealthStatus::UNHEALTHY) {
                LOGW_FMT("Connection request rejected: endpoint unhealthy - " << endpoint);
                notifyEvent(endpoint, "Connection request rejected: endpoint unhealthy");
                return PooledConnection(nullptr, {});
            }
        }

        // Check circuit breaker
        if (state.circuit_breaker) {
            if (!state.circuit_breaker->allowsRequests()) {
                LOGW_FMT("Connection request rejected: circuit open - " << endpoint);
                notifyEvent(endpoint, "Connection request rejected: circuit open");
                return PooledConnection(nullptr, {});
            }
        }

        // Acquire connection from pool
        auto conn = state.pool->acquire(endpoint, timeout);

        if (conn) {
            LOGD_FMT("Connection acquired successfully for endpoint: " << endpoint);
            notifyEvent(endpoint, "Connection acquired");
        } else {
            LOGE_FMT("Connection acquisition failed for endpoint: " << endpoint);
            notifyEvent(endpoint, "Connection acquisition failed");
        }

        return conn;
    }

    TransportResult<bool> sendMessage(const std::string& endpoint,
                                     const Message& message) {
        LOGD_FMT("Sending message to endpoint: " << endpoint << ", type: " << static_cast<int>(message.getType()));

        auto conn = getConnection(endpoint, std::chrono::milliseconds(5000));
        if (!conn) {
            LOGE_FMT("Failed to acquire connection for sending message to: " << endpoint);
            return TransportResult<bool>{
                TransportError::CONNECTION_FAILED,
                false,
                "Failed to acquire connection"
            };
        }

        auto result = conn->send(message);

        // Record result for health monitoring
        recordOperationResult(endpoint, result.success());

        if (result.success()) {
            LOGD_FMT("Message sent successfully to endpoint: " << endpoint);
        } else {
            LOGE_FMT("Failed to send message to endpoint: " << endpoint << ", error: " << result.error_message);
        }

        return result;
    }

    TransportResult<bool> sendMessageWithRetry(const std::string& endpoint,
                                               const Message& message) {
        LOGI_FMT("Sending message with retry to endpoint: " << endpoint << ", type: " << static_cast<int>(message.getType()));
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGE_FMT("Endpoint not found for send with retry: " << endpoint);
            return TransportResult<bool>{
                TransportError::ENDPOINT_NOT_FOUND,
                false,
                "Endpoint not found"
            };
        }

        auto& state = *it->second;

        if (!state.retry_policy) {
            LOGD_FMT("No retry policy configured, sending once to: " << endpoint);
            // No retry policy, just send once
            return sendMessage(endpoint, message);
        }

        // Execute with retry
        TransportResult<bool> final_result;

        auto retry_result = state.retry_policy->execute([&]() -> bool {
            // Circuit breaker check
            if (state.circuit_breaker) {
                return state.circuit_breaker->execute([&]() -> bool {
                    final_result = sendMessage(endpoint, message);
                    return final_result.success();
                });
            } else {
                final_result = sendMessage(endpoint, message);
                return final_result.success();
            }
        });

        if (retry_result != RetryResult::SUCCESS) {
            LOGE_FMT("Send with retry failed for endpoint: " << endpoint);
            final_result.error = TransportError::SEND_FAILED;
            final_result.error_message = "Send failed after retries";
        } else {
            LOGI_FMT("Send with retry succeeded for endpoint: " << endpoint);
        }

        return final_result;
    }

    TransportResult<MessagePtr> receiveMessage(const std::string& endpoint,
                                              int32_t timeout_ms) {
        LOGD_FMT("Receiving message from endpoint: " << endpoint << ", timeout: " << timeout_ms << "ms");

        auto conn = getConnection(endpoint, std::chrono::milliseconds(5000));
        if (!conn) {
            LOGE_FMT("Failed to acquire connection for receiving message from: " << endpoint);
            return TransportResult<MessagePtr>{
                TransportError::CONNECTION_FAILED,
                nullptr,
                "Failed to acquire connection"
            };
        }

        auto result = conn->receive(timeout_ms);

        // Record result for health monitoring
        recordOperationResult(endpoint, result.success());

        if (result.success()) {
            LOGD_FMT("Message received successfully from endpoint: " << endpoint);
        } else {
            LOGE_FMT("Failed to receive message from endpoint: " << endpoint << ", error: " << result.error_message);
        }

        return result;
    }

    EndpointInfo getEndpointInfo(const std::string& endpoint) const {
        LOGD_FMT("Getting endpoint info for: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for info retrieval: " << endpoint);
            return EndpointInfo{};
        }

        const auto& state = *it->second;

        EndpointInfo info;
        info.endpoint = endpoint;
        info.priority = state.config.priority;

        if (state.health_checker) {
            info.health_status = state.health_checker->getStatus(endpoint);
        }

        if (state.circuit_breaker) {
            info.circuit_state = state.circuit_breaker->getState();
        }

        if (state.pool) {
            auto pool_stats = state.pool->getStats(endpoint);
            info.active_connections = pool_stats.active_connections;
            info.idle_connections = pool_stats.idle_connections;
        }

        info.last_success_time = state.last_success_time;
        info.last_failure_time = state.last_failure_time;

        LOGD_FMT("Endpoint info retrieved for: " << endpoint << ", active_connections: " << info.active_connections);
        return info;
    }

    std::vector<std::string> getEndpoints() const {
        LOGD_FMT("Getting all endpoints");
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> endpoints;
        endpoints.reserve(endpoints_.size());

        for (const auto& [endpoint, state] : endpoints_) {
            endpoints.push_back(endpoint);
        }

        LOGD_FMT("Total endpoints: " << endpoints.size());
        return endpoints;
    }

    std::vector<std::string> getHealthyEndpoints() const {
        LOGD_FMT("Getting healthy endpoints");
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> healthy;

        for (const auto& [endpoint, state] : endpoints_) {
            if (state->health_checker) {
                auto status = state->health_checker->getStatus(endpoint);
                if (status == HealthStatus::HEALTHY) {
                    healthy.push_back(endpoint);
                }
            }
        }

        LOGD_FMT("Healthy endpoints: " << healthy.size() << " out of " << endpoints_.size());
        return healthy;
    }

    bool isEndpointHealthy(const std::string& endpoint) const {
        LOGD_FMT("Checking if endpoint is healthy: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for health check: " << endpoint);
            return false;
        }

        if (it->second->health_checker) {
            bool healthy = it->second->health_checker->isHealthy(endpoint);
            LOGD_FMT("Endpoint " << endpoint << " health status: " << (healthy ? "HEALTHY" : "UNHEALTHY"));
            return healthy;
        }

        LOGD_FMT("No health checker for endpoint " << endpoint << ", assuming healthy");
        return true;  // No health checker, assume healthy
    }

    ConnectionManagerStats getStats() const {
        LOGD_FMT("Getting connection manager stats");
        std::lock_guard<std::mutex> lock(mutex_);

        ConnectionManagerStats stats;
        stats.total_endpoints = static_cast<uint32_t>(endpoints_.size());

        for (const auto& [endpoint, state] : endpoints_) {
            if (state->health_checker) {
                auto health = state->health_checker->getStatus(endpoint);
                switch (health) {
                    case HealthStatus::HEALTHY:
                        stats.healthy_endpoints++;
                        break;
                    case HealthStatus::DEGRADED:
                        stats.degraded_endpoints++;
                        break;
                    case HealthStatus::UNHEALTHY:
                        stats.unhealthy_endpoints++;
                        break;
                    default:
                        break;
                }
            }

            if (state->pool) {
                auto pool_stats = state->pool->getStats(endpoint);
                stats.total_active_connections += pool_stats.active_connections;
                stats.total_idle_connections += pool_stats.idle_connections;
            }

            if (state->circuit_breaker) {
                auto circuit_stats = state->circuit_breaker->getStatistics();
                stats.total_circuit_rejections += circuit_stats.total_rejections;
            }

            if (state->retry_policy) {
                auto retry_stats = state->retry_policy->getStatistics();
                stats.total_retries += retry_stats.total_attempts - retry_stats.first_try_successes;
            }
        }

        return stats;
    }

    ConnectionManager::EndpointStats getEndpointStats(
        const std::string& endpoint) const {
        LOGD_FMT("Getting endpoint stats for: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        ConnectionManager::EndpointStats stats;

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for stats retrieval: " << endpoint);
            return stats;
        }

        const auto& state = *it->second;

        if (state.pool) {
            stats.pool_stats = state.pool->getStats(endpoint);
        }

        if (state.health_checker) {
            stats.health_stats = state.health_checker->getStats(endpoint);
        }

        if (state.circuit_breaker) {
            stats.circuit_stats = state.circuit_breaker->getStatistics();
        }

        if (state.retry_policy) {
            stats.retry_stats = state.retry_policy->getStatistics();
        }

        LOGD_FMT("Endpoint stats retrieved for: " << endpoint);
        return stats;
    }

    void resetStats(const std::string& endpoint) {
        LOGI_FMT("Resetting stats for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for stats reset: " << endpoint);
            return;
        }

        auto& state = *it->second;

        if (state.pool) {
            state.pool->resetStats(endpoint);
        }

        if (state.health_checker) {
            state.health_checker->resetStats(endpoint);
        }

        if (state.circuit_breaker) {
            state.circuit_breaker->resetStatistics();
        }

        if (state.retry_policy) {
            state.retry_policy->resetStatistics();
        }

        LOGI_FMT("Stats reset completed for endpoint: " << endpoint);
    }

    void resetAllStats() {
        LOGI_FMT("Resetting all endpoint stats");
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [endpoint, state] : endpoints_) {
            if (state->pool) {
                state->pool->resetStats(endpoint);
            }

            if (state->health_checker) {
                state->health_checker->resetStats(endpoint);
            }

            if (state->circuit_breaker) {
                state->circuit_breaker->resetStatistics();
            }

            if (state->retry_policy) {
                state->retry_policy->resetStatistics();
            }
        }

        LOGI_FMT("All endpoint stats reset completed");
    }

    bool checkEndpointHealth(const std::string& endpoint) {
        LOGD_FMT("Checking endpoint health now: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for health check: " << endpoint);
            return false;
        }

        if (it->second->health_checker) {
            bool result = it->second->health_checker->checkNow(endpoint);
            LOGD_FMT("Endpoint health check result for " << endpoint << ": " << (result ? "PASS" : "FAIL"));
            return result;
        }

        LOGD_FMT("No health checker for endpoint " << endpoint << ", returning true");
        return true;
    }

    void resetCircuitBreaker(const std::string& endpoint) {
        LOGI_FMT("Resetting circuit breaker for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for circuit breaker reset: " << endpoint);
            return;
        }

        if (it->second->circuit_breaker) {
            it->second->circuit_breaker->reset();
            LOGI_FMT("Circuit breaker reset completed for endpoint: " << endpoint);
        } else {
            LOGD_FMT("No circuit breaker for endpoint: " << endpoint);
        }
    }

    void closeEndpointConnections(const std::string& endpoint) {
        LOGI_FMT("Closing all connections for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for connection closure: " << endpoint);
            return;
        }

        if (it->second->pool) {
            it->second->pool->closeAll(endpoint);
            LOGI_FMT("All connections closed for endpoint: " << endpoint);
        }
    }

    uint32_t closeAllIdleConnections() {
        LOGI_FMT("Closing all idle connections");
        std::lock_guard<std::mutex> lock(mutex_);

        uint32_t total_closed = 0;

        for (auto& [endpoint, state] : endpoints_) {
            if (state->pool) {
                total_closed += state->pool->closeIdle(endpoint);
            }
        }

        LOGI_FMT("Total idle connections closed: " << total_closed);
        return total_closed;
    }

    void setEventCallback(ConnectionEventCallback callback) {
        LOGD_FMT("Setting event callback");
        std::lock_guard<std::mutex> lock(callback_mutex_);
        event_callback_ = std::move(callback);
    }

    bool updateEndpointConfig(const std::string& endpoint,
                             const EndpointConfig& config) {
        LOGI_FMT("Updating config for endpoint: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for config update: " << endpoint);
            return false;
        }

        auto& state = *it->second;
        state.config = config;

        // Update component configurations
        if (state.pool) {
            state.pool->updateConfig(config.pool_config);
        }

        if (state.health_checker) {
            state.health_checker->updateConfig(config.health_config);
        }

        if (state.circuit_breaker) {
            state.circuit_breaker->updateConfig(config.circuit_config);
        }

        if (state.retry_policy) {
            state.retry_policy->updateConfig(config.retry_config);
        }

        LOGI_FMT("Endpoint config updated successfully: " << endpoint);
        return true;
    }

    EndpointConfig getEndpointConfig(const std::string& endpoint) const {
        LOGD_FMT("Getting endpoint config for: " << endpoint);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for config retrieval: " << endpoint);
            return EndpointConfig{};
        }

        return it->second->config;
    }

    bool waitForShutdown(std::chrono::milliseconds timeout) {
        LOGD_FMT("Waiting for shutdown, timeout: " << timeout.count() << "ms");
        std::unique_lock<std::mutex> lock(mutex_);

        bool result = shutdown_cv_.wait_for(lock, timeout, [this] {
            return !shutdown_in_progress_;
        });

        LOGD_FMT("Shutdown wait completed, result: " << (result ? "SUCCESS" : "TIMEOUT"));
        return result;
    }

private:
    void drainConnections() {
        LOGD_FMT("Draining connections gracefully");
        // Close all idle connections gracefully
        uint32_t closed = closeAllIdleConnections();
        LOGD_FMT("Drained " << closed << " idle connections");

        // Wait a bit for active connections to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void recordOperationResult(const std::string& endpoint, bool success) {
        LOGD_FMT("Recording operation result for endpoint: " << endpoint << ", success: " << success);
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = endpoints_.find(endpoint);
        if (it == endpoints_.end()) {
            LOGW_FMT("Endpoint not found for recording operation result: " << endpoint);
            return;
        }

        auto& state = *it->second;

        if (success) {
            state.last_success_time = std::chrono::steady_clock::now();

            if (state.health_checker) {
                state.health_checker->recordSuccess(endpoint);
            }

            if (state.circuit_breaker) {
                state.circuit_breaker->recordSuccess();
            }
        } else {
            state.last_failure_time = std::chrono::steady_clock::now();

            if (state.health_checker) {
                state.health_checker->recordFailure(endpoint);
            }

            if (state.circuit_breaker) {
                state.circuit_breaker->recordFailure();
            }
        }
    }

    void handleHealthStatusChange(const std::string& endpoint,
                                  HealthStatus old_status,
                                  HealthStatus new_status) {
        LOGI_FMT("Health status change for endpoint " << endpoint << ": "
                 << to_string(old_status) << " -> " << to_string(new_status));
        std::string event = "Health status changed from " +
                          std::string(to_string(old_status)) + " to " +
                          std::string(to_string(new_status));
        notifyEvent(endpoint, event);
    }

    void handleCircuitStateChange(const std::string& endpoint,
                                 CircuitState old_state,
                                 CircuitState new_state) {
        LOGI_FMT("Circuit state change for endpoint " << endpoint << ": "
                 << to_string(old_state) << " -> " << to_string(new_state));
        std::string event = "Circuit state changed from " +
                          std::string(to_string(old_state)) + " to " +
                          std::string(to_string(new_state));
        notifyEvent(endpoint, event);
    }

    void notifyEvent(const std::string& endpoint, const std::string& event) {
        LOGD_FMT("Notifying event for endpoint " << endpoint << ": " << event);
        std::lock_guard<std::mutex> lock(callback_mutex_);

        if (event_callback_) {
            event_callback_(endpoint, event);
        }
    }

    std::atomic<bool> running_;
    std::atomic<bool> shutdown_in_progress_;

    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<EndpointState>> endpoints_;

    mutable std::mutex callback_mutex_;
    ConnectionEventCallback event_callback_;

    std::condition_variable shutdown_cv_;
};

// ConnectionManager implementation

ConnectionManager::ConnectionManager()
    : pImpl_(std::make_unique<Impl>()) {
}

ConnectionManager::~ConnectionManager() = default;

ConnectionManager::ConnectionManager(ConnectionManager&&) noexcept = default;
ConnectionManager& ConnectionManager::operator=(ConnectionManager&&) noexcept = default;

bool ConnectionManager::start() {
    return pImpl_->start();
}

void ConnectionManager::stop(bool graceful) {
    pImpl_->stop(graceful);
}

bool ConnectionManager::isRunning() const {
    return pImpl_->isRunning();
}

bool ConnectionManager::registerEndpoint(const EndpointConfig& config) {
    return pImpl_->registerEndpoint(config);
}

bool ConnectionManager::unregisterEndpoint(const std::string& endpoint) {
    return pImpl_->unregisterEndpoint(endpoint);
}

PooledConnection ConnectionManager::getConnection(const std::string& endpoint) {
    return pImpl_->getConnection(endpoint, std::chrono::milliseconds(5000));
}

PooledConnection ConnectionManager::getConnection(const std::string& endpoint,
                                                 std::chrono::milliseconds timeout) {
    return pImpl_->getConnection(endpoint, timeout);
}

TransportResult<bool> ConnectionManager::sendMessage(const std::string& endpoint,
                                                     const Message& message) {
    return pImpl_->sendMessage(endpoint, message);
}

TransportResult<bool> ConnectionManager::sendMessageWithRetry(
    const std::string& endpoint, const Message& message) {
    return pImpl_->sendMessageWithRetry(endpoint, message);
}

TransportResult<MessagePtr> ConnectionManager::receiveMessage(
    const std::string& endpoint, int32_t timeout_ms) {
    return pImpl_->receiveMessage(endpoint, timeout_ms);
}

EndpointInfo ConnectionManager::getEndpointInfo(const std::string& endpoint) const {
    return pImpl_->getEndpointInfo(endpoint);
}

std::vector<std::string> ConnectionManager::getEndpoints() const {
    return pImpl_->getEndpoints();
}

std::vector<std::string> ConnectionManager::getHealthyEndpoints() const {
    return pImpl_->getHealthyEndpoints();
}

bool ConnectionManager::isEndpointHealthy(const std::string& endpoint) const {
    return pImpl_->isEndpointHealthy(endpoint);
}

ConnectionManagerStats ConnectionManager::getStats() const {
    return pImpl_->getStats();
}

ConnectionManager::EndpointStats ConnectionManager::getEndpointStats(
    const std::string& endpoint) const {
    return pImpl_->getEndpointStats(endpoint);
}

void ConnectionManager::resetStats(const std::string& endpoint) {
    pImpl_->resetStats(endpoint);
}

void ConnectionManager::resetAllStats() {
    pImpl_->resetAllStats();
}

bool ConnectionManager::checkEndpointHealth(const std::string& endpoint) {
    return pImpl_->checkEndpointHealth(endpoint);
}

void ConnectionManager::resetCircuitBreaker(const std::string& endpoint) {
    pImpl_->resetCircuitBreaker(endpoint);
}

void ConnectionManager::closeEndpointConnections(const std::string& endpoint) {
    pImpl_->closeEndpointConnections(endpoint);
}

uint32_t ConnectionManager::closeAllIdleConnections() {
    return pImpl_->closeAllIdleConnections();
}

void ConnectionManager::setEventCallback(ConnectionEventCallback callback) {
    pImpl_->setEventCallback(std::move(callback));
}

bool ConnectionManager::updateEndpointConfig(const std::string& endpoint,
                                            const EndpointConfig& config) {
    return pImpl_->updateEndpointConfig(endpoint, config);
}

EndpointConfig ConnectionManager::getEndpointConfig(const std::string& endpoint) const {
    return pImpl_->getEndpointConfig(endpoint);
}

bool ConnectionManager::waitForShutdown(std::chrono::milliseconds timeout) {
    return pImpl_->waitForShutdown(timeout);
}

// ConnectionManagerBuilder implementation

class ConnectionManagerBuilder::BuilderImpl {
public:
    std::vector<EndpointConfig> endpoints;
    ConnectionPoolConfig default_pool_config;
    HealthCheckConfig default_health_config;
    RetryConfig default_retry_config;
    CircuitBreakerConfig default_circuit_config;
    ConnectionManager::ConnectionEventCallback event_callback;
    bool enable_health_check = true;
    bool enable_circuit_breaker = true;
    bool enable_retry = true;
};

ConnectionManagerBuilder::ConnectionManagerBuilder()
    : pImpl_(std::make_unique<BuilderImpl>()) {
}

ConnectionManagerBuilder::~ConnectionManagerBuilder() = default;

ConnectionManagerBuilder& ConnectionManagerBuilder::withEndpoint(
    const EndpointConfig& config) {
    pImpl_->endpoints.push_back(config);
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::withDefaultPoolConfig(
    const ConnectionPoolConfig& config) {
    pImpl_->default_pool_config = config;
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::withDefaultHealthConfig(
    const HealthCheckConfig& config) {
    pImpl_->default_health_config = config;
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::withDefaultRetryConfig(
    const RetryConfig& config) {
    pImpl_->default_retry_config = config;
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::withDefaultCircuitConfig(
    const CircuitBreakerConfig& config) {
    pImpl_->default_circuit_config = config;
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::onConnectionEvent(
    ConnectionManager::ConnectionEventCallback callback) {
    pImpl_->event_callback = std::move(callback);
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::enableHealthCheck(bool enable) {
    pImpl_->enable_health_check = enable;
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::enableCircuitBreaker(bool enable) {
    pImpl_->enable_circuit_breaker = enable;
    return *this;
}

ConnectionManagerBuilder& ConnectionManagerBuilder::enableRetry(bool enable) {
    pImpl_->enable_retry = enable;
    return *this;
}

std::unique_ptr<ConnectionManager> ConnectionManagerBuilder::build() {
    auto manager = std::make_unique<ConnectionManager>();

    // Register all endpoints
    for (auto& config : pImpl_->endpoints) {
        // Apply defaults if not set
        if (config.pool_config.max_pool_size == 0) {
            config.pool_config = pImpl_->default_pool_config;
        }

        config.enable_health_check = pImpl_->enable_health_check;
        config.enable_circuit_breaker = pImpl_->enable_circuit_breaker;
        config.enable_retry = pImpl_->enable_retry;

        manager->registerEndpoint(config);
    }

    // Set event callback
    if (pImpl_->event_callback) {
        manager->setEventCallback(std::move(pImpl_->event_callback));
    }

    return manager;
}

} // namespace ipc
} // namespace cdmf
