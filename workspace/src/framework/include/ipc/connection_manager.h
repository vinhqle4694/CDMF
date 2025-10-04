/**
 * @file connection_manager.h
 * @brief Central connection management infrastructure
 *
 * Provides centralized management of all connections with integration
 * of health checking, circuit breaker, retry policy, and connection
 * pooling. Manages per-endpoint configuration, automatic recovery,
 * and graceful shutdown with connection draining.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Connection Management Agent
 */

#ifndef CDMF_IPC_CONNECTION_MANAGER_H
#define CDMF_IPC_CONNECTION_MANAGER_H

#include "circuit_breaker.h"
#include "connection_pool.h"
#include "health_checker.h"
#include "retry_policy.h"
#include "transport.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cdmf {
namespace ipc {

/**
 * @brief Endpoint configuration
 */
struct EndpointConfig {
    /** Endpoint identifier */
    std::string endpoint;

    /** Transport configuration */
    TransportConfig transport_config;

    /** Connection pool configuration */
    ConnectionPoolConfig pool_config;

    /** Health check configuration */
    HealthCheckConfig health_config;

    /** Retry policy configuration */
    RetryConfig retry_config;

    /** Circuit breaker configuration */
    CircuitBreakerConfig circuit_config;

    /** Enable connection pooling */
    bool enable_pooling = true;

    /** Enable health checking */
    bool enable_health_check = true;

    /** Enable circuit breaker */
    bool enable_circuit_breaker = true;

    /** Enable automatic retry */
    bool enable_retry = true;

    /** Priority for this endpoint (higher = more important) */
    uint32_t priority = 0;
};

/**
 * @brief Connection manager statistics
 */
struct ConnectionManagerStats {
    /** Total number of managed endpoints */
    uint32_t total_endpoints = 0;

    /** Number of healthy endpoints */
    uint32_t healthy_endpoints = 0;

    /** Number of degraded endpoints */
    uint32_t degraded_endpoints = 0;

    /** Number of unhealthy endpoints */
    uint32_t unhealthy_endpoints = 0;

    /** Total number of active connections */
    uint32_t total_active_connections = 0;

    /** Total number of idle connections */
    uint32_t total_idle_connections = 0;

    /** Total messages sent */
    uint64_t total_messages_sent = 0;

    /** Total messages received */
    uint64_t total_messages_received = 0;

    /** Total connection failures */
    uint64_t total_connection_failures = 0;

    /** Total retries performed */
    uint64_t total_retries = 0;

    /** Total circuit breaker rejections */
    uint64_t total_circuit_rejections = 0;
};

/**
 * @brief Endpoint information
 */
struct EndpointInfo {
    /** Endpoint identifier */
    std::string endpoint;

    /** Current health status */
    HealthStatus health_status = HealthStatus::UNKNOWN;

    /** Current circuit state */
    CircuitState circuit_state = CircuitState::CLOSED;

    /** Transport state */
    TransportState transport_state = TransportState::UNINITIALIZED;

    /** Number of active connections */
    uint32_t active_connections = 0;

    /** Number of idle connections */
    uint32_t idle_connections = 0;

    /** Endpoint priority */
    uint32_t priority = 0;

    /** Last successful communication timestamp */
    std::chrono::steady_clock::time_point last_success_time;

    /** Last failure timestamp */
    std::chrono::steady_clock::time_point last_failure_time;
};

/**
 * @brief Connection manager for centralized connection management
 *
 * Manages all aspects of connection lifecycle including:
 * - Connection pooling per endpoint
 * - Health checking and monitoring
 * - Circuit breaker for fault isolation
 * - Automatic retry with configurable policy
 * - Connection recovery and reconnection
 * - Graceful shutdown with connection draining
 *
 * Thread-safe for concurrent operations.
 */
class ConnectionManager {
public:
    /**
     * @brief Callback for connection events
     * @param endpoint Endpoint identifier
     * @param event Event description
     */
    using ConnectionEventCallback = std::function<void(
        const std::string& endpoint,
        const std::string& event)>;

    /**
     * @brief Construct connection manager
     */
    ConnectionManager();

    /**
     * @brief Destructor - performs graceful shutdown
     */
    ~ConnectionManager();

    // Prevent copying, allow moving
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;
    ConnectionManager(ConnectionManager&&) noexcept;
    ConnectionManager& operator=(ConnectionManager&&) noexcept;

    /**
     * @brief Start the connection manager
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the connection manager
     * @param graceful Perform graceful shutdown with draining
     */
    void stop(bool graceful = true);

    /**
     * @brief Check if manager is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Register an endpoint with configuration
     * @param config Endpoint configuration
     * @return true if registered successfully
     */
    bool registerEndpoint(const EndpointConfig& config);

    /**
     * @brief Unregister an endpoint
     * @param endpoint Endpoint identifier
     * @return true if unregistered successfully
     */
    bool unregisterEndpoint(const std::string& endpoint);

    /**
     * @brief Get a connection to an endpoint
     * @param endpoint Endpoint identifier
     * @return Pooled connection or empty on failure
     */
    PooledConnection getConnection(const std::string& endpoint);

    /**
     * @brief Get a connection with timeout
     * @param endpoint Endpoint identifier
     * @param timeout Acquisition timeout
     * @return Pooled connection or empty on timeout
     */
    PooledConnection getConnection(const std::string& endpoint,
                                   std::chrono::milliseconds timeout);

    /**
     * @brief Send a message to an endpoint
     * @param endpoint Endpoint identifier
     * @param message Message to send
     * @return Transport result
     */
    TransportResult<bool> sendMessage(const std::string& endpoint,
                                     const Message& message);

    /**
     * @brief Send a message with retry
     * @param endpoint Endpoint identifier
     * @param message Message to send
     * @return Transport result
     */
    TransportResult<bool> sendMessageWithRetry(const std::string& endpoint,
                                               const Message& message);

    /**
     * @brief Receive a message from an endpoint
     * @param endpoint Endpoint identifier
     * @param timeout_ms Receive timeout
     * @return Transport result with message
     */
    TransportResult<MessagePtr> receiveMessage(const std::string& endpoint,
                                              int32_t timeout_ms = 0);

    /**
     * @brief Get endpoint information
     * @param endpoint Endpoint identifier
     * @return Endpoint information
     */
    EndpointInfo getEndpointInfo(const std::string& endpoint) const;

    /**
     * @brief Get all registered endpoints
     * @return List of endpoint identifiers
     */
    std::vector<std::string> getEndpoints() const;

    /**
     * @brief Get healthy endpoints only
     * @return List of healthy endpoint identifiers
     */
    std::vector<std::string> getHealthyEndpoints() const;

    /**
     * @brief Check if endpoint is healthy
     * @param endpoint Endpoint identifier
     * @return true if healthy
     */
    bool isEndpointHealthy(const std::string& endpoint) const;

    /**
     * @brief Get connection manager statistics
     * @return Manager statistics
     */
    ConnectionManagerStats getStats() const;

    /**
     * @brief Get statistics for a specific endpoint
     * @param endpoint Endpoint identifier
     * @return Endpoint-specific statistics (pool + health + circuit)
     */
    struct EndpointStats {
        ConnectionPoolStats pool_stats;
        HealthCheckStats health_stats;
        CircuitBreakerStats circuit_stats;
        RetryStats retry_stats;
    };
    EndpointStats getEndpointStats(const std::string& endpoint) const;

    /**
     * @brief Reset statistics for an endpoint
     * @param endpoint Endpoint identifier
     */
    void resetStats(const std::string& endpoint);

    /**
     * @brief Reset all statistics
     */
    void resetAllStats();

    /**
     * @brief Manually trigger health check for an endpoint
     * @param endpoint Endpoint identifier
     * @return true if healthy
     */
    bool checkEndpointHealth(const std::string& endpoint);

    /**
     * @brief Manually reset circuit breaker for an endpoint
     * @param endpoint Endpoint identifier
     */
    void resetCircuitBreaker(const std::string& endpoint);

    /**
     * @brief Close all connections to an endpoint
     * @param endpoint Endpoint identifier
     */
    void closeEndpointConnections(const std::string& endpoint);

    /**
     * @brief Close all idle connections across all endpoints
     * @return Total number of connections closed
     */
    uint32_t closeAllIdleConnections();

    /**
     * @brief Set connection event callback
     * @param callback Event callback function
     */
    void setEventCallback(ConnectionEventCallback callback);

    /**
     * @brief Update endpoint configuration
     * @param endpoint Endpoint identifier
     * @param config New configuration
     * @return true if updated successfully
     */
    bool updateEndpointConfig(const std::string& endpoint,
                             const EndpointConfig& config);

    /**
     * @brief Get endpoint configuration
     * @param endpoint Endpoint identifier
     * @return Current configuration
     */
    EndpointConfig getEndpointConfig(const std::string& endpoint) const;

    /**
     * @brief Wait for graceful shutdown completion
     * @param timeout Maximum time to wait
     * @return true if shutdown completed, false if timed out
     */
    bool waitForShutdown(std::chrono::milliseconds timeout);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Builder for constructing ConnectionManager with fluent API
 */
class ConnectionManagerBuilder {
public:
    /**
     * @brief Construct builder
     */
    ConnectionManagerBuilder();

    /**
     * @brief Destructor
     */
    ~ConnectionManagerBuilder();

    /**
     * @brief Add an endpoint configuration
     */
    ConnectionManagerBuilder& withEndpoint(const EndpointConfig& config);

    /**
     * @brief Set default pool configuration for all endpoints
     */
    ConnectionManagerBuilder& withDefaultPoolConfig(const ConnectionPoolConfig& config);

    /**
     * @brief Set default health check configuration
     */
    ConnectionManagerBuilder& withDefaultHealthConfig(const HealthCheckConfig& config);

    /**
     * @brief Set default retry configuration
     */
    ConnectionManagerBuilder& withDefaultRetryConfig(const RetryConfig& config);

    /**
     * @brief Set default circuit breaker configuration
     */
    ConnectionManagerBuilder& withDefaultCircuitConfig(const CircuitBreakerConfig& config);

    /**
     * @brief Set connection event callback
     */
    ConnectionManagerBuilder& onConnectionEvent(
        ConnectionManager::ConnectionEventCallback callback);

    /**
     * @brief Enable automatic health checking
     */
    ConnectionManagerBuilder& enableHealthCheck(bool enable = true);

    /**
     * @brief Enable circuit breaker
     */
    ConnectionManagerBuilder& enableCircuitBreaker(bool enable = true);

    /**
     * @brief Enable automatic retry
     */
    ConnectionManagerBuilder& enableRetry(bool enable = true);

    /**
     * @brief Build the ConnectionManager instance
     */
    std::unique_ptr<ConnectionManager> build();

private:
    class BuilderImpl;
    std::unique_ptr<BuilderImpl> pImpl_;
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_CONNECTION_MANAGER_H
