/**
 * @file health_checker.h
 * @brief Health checking infrastructure for connection monitoring
 *
 * Provides active and passive health checking mechanisms to monitor
 * the health status of connections and endpoints. Supports configurable
 * check intervals, multiple health check strategies, and state change
 * notifications.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Connection Management Agent
 */

#ifndef CDMF_IPC_HEALTH_CHECKER_H
#define CDMF_IPC_HEALTH_CHECKER_H

#include "transport.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Health status enumeration
 */
enum class HealthStatus {
    /** Endpoint is healthy and operating normally */
    HEALTHY,

    /** Endpoint is degraded but still functional */
    DEGRADED,

    /** Endpoint is unhealthy and should not be used */
    UNHEALTHY,

    /** Health status is unknown (not yet checked) */
    UNKNOWN
};

/**
 * @brief Health check strategy types
 */
enum class HealthCheckStrategy {
    /** TCP connection attempt */
    TCP_CONNECT,

    /** Application-level ping/pong */
    APPLICATION_PING,

    /** Passive monitoring of request success/failure */
    PASSIVE_MONITORING,

    /** Custom health check implementation */
    CUSTOM
};

/**
 * @brief Health check configuration
 */
struct HealthCheckConfig {
    /** Strategy to use for health checking */
    HealthCheckStrategy strategy = HealthCheckStrategy::APPLICATION_PING;

    /** Interval between active health checks */
    std::chrono::milliseconds check_interval{30000};

    /** Timeout for each health check attempt */
    std::chrono::milliseconds check_timeout{5000};

    /** Number of consecutive failures before marking unhealthy */
    uint32_t unhealthy_threshold = 3;

    /** Number of consecutive successes before marking healthy */
    uint32_t healthy_threshold = 2;

    /** Enable active health checks */
    bool enable_active_checks = true;

    /** Enable passive health monitoring */
    bool enable_passive_monitoring = true;

    /** Window size for passive monitoring (number of requests) */
    uint32_t passive_window_size = 100;

    /** Failure rate threshold for degraded status (0.0-1.0) */
    double degraded_threshold = 0.1;

    /** Failure rate threshold for unhealthy status (0.0-1.0) */
    double unhealthy_failure_rate = 0.5;

    /** Minimum requests before calculating failure rate */
    uint32_t min_requests_for_rate = 10;
};

/**
 * @brief Health check statistics
 */
struct HealthCheckStats {
    /** Current health status */
    HealthStatus current_status = HealthStatus::UNKNOWN;

    /** Total number of health checks performed */
    uint64_t total_checks = 0;

    /** Number of successful health checks */
    uint64_t successful_checks = 0;

    /** Number of failed health checks */
    uint64_t failed_checks = 0;

    /** Consecutive successful checks */
    uint32_t consecutive_successes = 0;

    /** Consecutive failed checks */
    uint32_t consecutive_failures = 0;

    /** Last check timestamp */
    std::chrono::steady_clock::time_point last_check_time;

    /** Last status change timestamp */
    std::chrono::steady_clock::time_point last_status_change_time;

    /** Current failure rate (passive monitoring) */
    double current_failure_rate = 0.0;

    /** Average check latency */
    std::chrono::microseconds avg_check_latency{0};

    /** Last check latency */
    std::chrono::microseconds last_check_latency{0};
};

/**
 * @brief Health checker for monitoring endpoint health
 *
 * Provides both active health checks (periodic pings) and passive
 * monitoring (tracking request success/failure rates). Supports
 * multiple check strategies and state change notifications.
 *
 * Thread-safe for concurrent use.
 */
class HealthChecker {
public:
    /**
     * @brief Callback for health status changes
     * @param endpoint Endpoint identifier
     * @param old_status Previous health status
     * @param new_status New health status
     */
    using StatusChangeCallback = std::function<void(
        const std::string& endpoint,
        HealthStatus old_status,
        HealthStatus new_status)>;

    /**
     * @brief Callback for custom health check implementation
     * @param endpoint Endpoint to check
     * @return true if healthy, false if unhealthy
     */
    using CustomCheckCallback = std::function<bool(const std::string& endpoint)>;

    /**
     * @brief Construct health checker with configuration
     * @param config Health check configuration
     */
    explicit HealthChecker(const HealthCheckConfig& config);

    /**
     * @brief Destructor
     */
    ~HealthChecker();

    // Prevent copying, allow moving
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;
    HealthChecker(HealthChecker&&) noexcept;
    HealthChecker& operator=(HealthChecker&&) noexcept;

    /**
     * @brief Start health checking
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop health checking
     */
    void stop();

    /**
     * @brief Check if health checker is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Add an endpoint to monitor
     * @param endpoint Endpoint identifier
     * @param transport Transport for health checks (optional for passive only)
     * @return true if added successfully
     */
    bool addEndpoint(const std::string& endpoint, TransportPtr transport = nullptr);

    /**
     * @brief Remove an endpoint from monitoring
     * @param endpoint Endpoint identifier
     * @return true if removed successfully
     */
    bool removeEndpoint(const std::string& endpoint);

    /**
     * @brief Perform immediate health check on an endpoint
     * @param endpoint Endpoint identifier
     * @return true if healthy, false if unhealthy
     */
    bool checkNow(const std::string& endpoint);

    /**
     * @brief Get current health status of an endpoint
     * @param endpoint Endpoint identifier
     * @return Current health status
     */
    HealthStatus getStatus(const std::string& endpoint) const;

    /**
     * @brief Check if endpoint is healthy
     * @param endpoint Endpoint identifier
     * @return true if status is HEALTHY
     */
    bool isHealthy(const std::string& endpoint) const;

    /**
     * @brief Record a successful request (passive monitoring)
     * @param endpoint Endpoint identifier
     */
    void recordSuccess(const std::string& endpoint);

    /**
     * @brief Record a failed request (passive monitoring)
     * @param endpoint Endpoint identifier
     */
    void recordFailure(const std::string& endpoint);

    /**
     * @brief Get health check statistics for an endpoint
     * @param endpoint Endpoint identifier
     * @return Health check statistics
     */
    HealthCheckStats getStats(const std::string& endpoint) const;

    /**
     * @brief Reset statistics for an endpoint
     * @param endpoint Endpoint identifier
     */
    void resetStats(const std::string& endpoint);

    /**
     * @brief Set status change callback
     * @param callback Callback function
     */
    void setStatusChangeCallback(StatusChangeCallback callback);

    /**
     * @brief Set custom health check callback
     * @param callback Custom check function
     */
    void setCustomCheckCallback(CustomCheckCallback callback);

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void updateConfig(const HealthCheckConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    HealthCheckConfig getConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Builder for constructing HealthChecker with fluent API
 */
class HealthCheckerBuilder {
public:
    /**
     * @brief Construct builder with default configuration
     */
    HealthCheckerBuilder();

    /**
     * @brief Set health check strategy
     */
    HealthCheckerBuilder& withStrategy(HealthCheckStrategy strategy);

    /**
     * @brief Set check interval
     */
    HealthCheckerBuilder& withCheckInterval(std::chrono::milliseconds interval);

    /**
     * @brief Set check timeout
     */
    HealthCheckerBuilder& withCheckTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set unhealthy threshold
     */
    HealthCheckerBuilder& withUnhealthyThreshold(uint32_t threshold);

    /**
     * @brief Set healthy threshold
     */
    HealthCheckerBuilder& withHealthyThreshold(uint32_t threshold);

    /**
     * @brief Enable active health checks
     */
    HealthCheckerBuilder& enableActiveChecks(bool enable = true);

    /**
     * @brief Enable passive monitoring
     */
    HealthCheckerBuilder& enablePassiveMonitoring(bool enable = true);

    /**
     * @brief Set passive monitoring window size
     */
    HealthCheckerBuilder& withPassiveWindowSize(uint32_t size);

    /**
     * @brief Set degraded threshold
     */
    HealthCheckerBuilder& withDegradedThreshold(double threshold);

    /**
     * @brief Set unhealthy failure rate
     */
    HealthCheckerBuilder& withUnhealthyFailureRate(double rate);

    /**
     * @brief Set status change callback
     */
    HealthCheckerBuilder& onStatusChange(HealthChecker::StatusChangeCallback callback);

    /**
     * @brief Set custom check callback
     */
    HealthCheckerBuilder& withCustomCheck(HealthChecker::CustomCheckCallback callback);

    /**
     * @brief Build the HealthChecker instance
     */
    std::unique_ptr<HealthChecker> build();

private:
    HealthCheckConfig config_;
    HealthChecker::StatusChangeCallback status_change_callback_;
    HealthChecker::CustomCheckCallback custom_check_callback_;
};

/**
 * @brief Convert HealthStatus to string
 */
inline const char* to_string(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::UNHEALTHY: return "UNHEALTHY";
        case HealthStatus::UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

/**
 * @brief Convert HealthCheckStrategy to string
 */
inline const char* to_string(HealthCheckStrategy strategy) {
    switch (strategy) {
        case HealthCheckStrategy::TCP_CONNECT: return "TCP_CONNECT";
        case HealthCheckStrategy::APPLICATION_PING: return "APPLICATION_PING";
        case HealthCheckStrategy::PASSIVE_MONITORING: return "PASSIVE_MONITORING";
        case HealthCheckStrategy::CUSTOM: return "CUSTOM";
        default: return "INVALID";
    }
}

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_HEALTH_CHECKER_H
