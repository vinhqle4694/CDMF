/**
 * @file connection_pool.h
 * @brief Connection pooling infrastructure for efficient connection reuse
 *
 * Provides connection pool management with configurable pool sizes,
 * connection validation, idle connection eviction, and load balancing
 * strategies. Supports per-endpoint connection pooling with statistics
 * tracking and lifecycle management.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Connection Management Agent
 */

#ifndef CDMF_IPC_CONNECTION_POOL_H
#define CDMF_IPC_CONNECTION_POOL_H

#include "transport.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Load balancing strategies for connection selection
 */
enum class LoadBalancingStrategy {
    /** Round-robin selection */
    ROUND_ROBIN,

    /** Select least-loaded connection */
    LEAST_LOADED,

    /** Random selection */
    RANDOM,

    /** Select least recently used */
    LEAST_RECENTLY_USED
};

/**
 * @brief Connection pool configuration
 */
struct ConnectionPoolConfig {
    /** Minimum number of connections to maintain in pool */
    uint32_t min_pool_size = 1;

    /** Maximum number of connections allowed in pool */
    uint32_t max_pool_size = 10;

    /** Timeout for acquiring a connection from pool */
    std::chrono::milliseconds acquire_timeout{5000};

    /** Maximum time a connection can be idle before eviction */
    std::chrono::milliseconds max_idle_time{60000};

    /** Interval between idle connection checks */
    std::chrono::milliseconds eviction_interval{30000};

    /** Maximum lifetime of a connection before forced close */
    std::chrono::milliseconds max_connection_lifetime{300000};

    /** Validate connections before returning from pool */
    bool validate_on_acquire = true;

    /** Validate connections when returning to pool */
    bool validate_on_release = false;

    /** Load balancing strategy for connection selection */
    LoadBalancingStrategy load_balancing = LoadBalancingStrategy::ROUND_ROBIN;

    /** Enable connection health checking */
    bool enable_health_check = true;

    /** Wait for connection if pool exhausted (vs fail immediately) */
    bool wait_if_exhausted = true;

    /** Create connections on demand (vs pre-populate) */
    bool create_on_demand = true;
};

/**
 * @brief Connection pool statistics
 */
struct ConnectionPoolStats {
    /** Total number of connections in pool (active + idle) */
    uint32_t total_connections = 0;

    /** Number of active (in-use) connections */
    uint32_t active_connections = 0;

    /** Number of idle (available) connections */
    uint32_t idle_connections = 0;

    /** Total connection acquisitions */
    uint64_t total_acquisitions = 0;

    /** Total connection releases */
    uint64_t total_releases = 0;

    /** Number of times acquire timed out */
    uint64_t acquire_timeouts = 0;

    /** Number of connections created */
    uint64_t connections_created = 0;

    /** Number of connections destroyed */
    uint64_t connections_destroyed = 0;

    /** Number of connections evicted due to idle time */
    uint64_t evictions_idle = 0;

    /** Number of connections evicted due to lifetime */
    uint64_t evictions_lifetime = 0;

    /** Number of failed connection validations */
    uint64_t validation_failures = 0;

    /** Average time to acquire a connection (microseconds) */
    std::chrono::microseconds avg_acquire_time{0};

    /** Peak number of connections */
    uint32_t peak_connections = 0;
};

/**
 * @brief RAII wrapper for pooled connections
 *
 * Automatically returns connection to pool when destroyed.
 */
class PooledConnection {
public:
    /**
     * @brief Destructor - returns connection to pool
     */
    ~PooledConnection();

    // Prevent copying, allow moving
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&&) noexcept;
    PooledConnection& operator=(PooledConnection&&) noexcept;

    /**
     * @brief Get the underlying transport
     * @return Transport pointer
     */
    TransportPtr get() const;

    /**
     * @brief Arrow operator for direct transport access
     */
    TransportPtr operator->() const;

    /**
     * @brief Boolean conversion - check if connection is valid
     */
    explicit operator bool() const;

    /**
     * @brief Manually release connection back to pool
     */
    void release();

private:
    friend class ConnectionPool;
    friend class ConnectionManager;

    /**
     * @brief Private constructor - only ConnectionPool can create
     */
    PooledConnection(TransportPtr transport, std::function<void(TransportPtr)> releaser);

    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Connection pool for managing reusable connections
 *
 * Maintains a pool of connections per endpoint with configurable
 * min/max sizes, connection validation, idle eviction, and load
 * balancing strategies.
 *
 * Thread-safe for concurrent connection acquisition and release.
 */
class ConnectionPool {
public:
    /**
     * @brief Callback for connection creation
     * @param endpoint Endpoint to connect to
     * @return Created transport or nullptr on failure
     */
    using ConnectionFactory = std::function<TransportPtr(const std::string& endpoint)>;

    /**
     * @brief Callback for connection validation
     * @param transport Transport to validate
     * @return true if valid, false if invalid
     */
    using ConnectionValidator = std::function<bool(TransportPtr transport)>;

    /**
     * @brief Construct connection pool with configuration
     * @param config Pool configuration
     * @param factory Connection creation callback
     */
    ConnectionPool(const ConnectionPoolConfig& config, ConnectionFactory factory);

    /**
     * @brief Destructor - closes all connections
     */
    ~ConnectionPool();

    // Prevent copying, allow moving
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) noexcept;
    ConnectionPool& operator=(ConnectionPool&&) noexcept;

    /**
     * @brief Start the connection pool
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the connection pool and close all connections
     */
    void stop();

    /**
     * @brief Check if pool is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Acquire a connection from the pool
     * @param endpoint Endpoint identifier
     * @return Pooled connection (RAII wrapper)
     */
    PooledConnection acquire(const std::string& endpoint);

    /**
     * @brief Acquire a connection with timeout
     * @param endpoint Endpoint identifier
     * @param timeout Acquisition timeout
     * @return Pooled connection or empty on timeout
     */
    PooledConnection acquire(const std::string& endpoint,
                            std::chrono::milliseconds timeout);

    /**
     * @brief Try to acquire a connection (non-blocking)
     * @param endpoint Endpoint identifier
     * @return Pooled connection or empty if none available
     */
    PooledConnection tryAcquire(const std::string& endpoint);

    /**
     * @brief Get pool statistics for an endpoint
     * @param endpoint Endpoint identifier
     * @return Pool statistics
     */
    ConnectionPoolStats getStats(const std::string& endpoint) const;

    /**
     * @brief Get aggregate statistics across all endpoints
     * @return Aggregate statistics
     */
    ConnectionPoolStats getAggregateStats() const;

    /**
     * @brief Reset statistics for an endpoint
     * @param endpoint Endpoint identifier
     */
    void resetStats(const std::string& endpoint);

    /**
     * @brief Close all connections for an endpoint
     * @param endpoint Endpoint identifier
     */
    void closeAll(const std::string& endpoint);

    /**
     * @brief Close idle connections for an endpoint
     * @param endpoint Endpoint identifier
     * @return Number of connections closed
     */
    uint32_t closeIdle(const std::string& endpoint);

    /**
     * @brief Pre-populate pool with connections
     * @param endpoint Endpoint identifier
     * @param count Number of connections to create
     * @return Number of connections successfully created
     */
    uint32_t prepopulate(const std::string& endpoint, uint32_t count);

    /**
     * @brief Set connection validator
     * @param validator Validation callback
     */
    void setValidator(ConnectionValidator validator);

    /**
     * @brief Update pool configuration
     * @param config New configuration
     */
    void updateConfig(const ConnectionPoolConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    ConnectionPoolConfig getConfig() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    /**
     * @brief Release a connection back to the pool
     * @param endpoint Endpoint identifier
     * @param transport Transport to release
     */
    void releaseConnection(const std::string& endpoint, TransportPtr transport);

    friend class PooledConnection;
};

/**
 * @brief Builder for constructing ConnectionPool with fluent API
 */
class ConnectionPoolBuilder {
public:
    /**
     * @brief Construct builder
     */
    ConnectionPoolBuilder();

    /**
     * @brief Set connection factory
     */
    ConnectionPoolBuilder& withFactory(ConnectionPool::ConnectionFactory factory);

    /**
     * @brief Set minimum pool size
     */
    ConnectionPoolBuilder& withMinPoolSize(uint32_t size);

    /**
     * @brief Set maximum pool size
     */
    ConnectionPoolBuilder& withMaxPoolSize(uint32_t size);

    /**
     * @brief Set acquire timeout
     */
    ConnectionPoolBuilder& withAcquireTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set maximum idle time
     */
    ConnectionPoolBuilder& withMaxIdleTime(std::chrono::milliseconds time);

    /**
     * @brief Set eviction interval
     */
    ConnectionPoolBuilder& withEvictionInterval(std::chrono::milliseconds interval);

    /**
     * @brief Set maximum connection lifetime
     */
    ConnectionPoolBuilder& withMaxLifetime(std::chrono::milliseconds lifetime);

    /**
     * @brief Enable validation on acquire
     */
    ConnectionPoolBuilder& validateOnAcquire(bool enable = true);

    /**
     * @brief Enable validation on release
     */
    ConnectionPoolBuilder& validateOnRelease(bool enable = true);

    /**
     * @brief Set load balancing strategy
     */
    ConnectionPoolBuilder& withLoadBalancing(LoadBalancingStrategy strategy);

    /**
     * @brief Enable health checking
     */
    ConnectionPoolBuilder& enableHealthCheck(bool enable = true);

    /**
     * @brief Wait if pool exhausted
     */
    ConnectionPoolBuilder& waitIfExhausted(bool wait = true);

    /**
     * @brief Create connections on demand
     */
    ConnectionPoolBuilder& createOnDemand(bool enable = true);

    /**
     * @brief Set connection validator
     */
    ConnectionPoolBuilder& withValidator(ConnectionPool::ConnectionValidator validator);

    /**
     * @brief Build the ConnectionPool instance
     */
    std::unique_ptr<ConnectionPool> build();

private:
    ConnectionPoolConfig config_;
    ConnectionPool::ConnectionFactory factory_;
    ConnectionPool::ConnectionValidator validator_;
};

/**
 * @brief Convert LoadBalancingStrategy to string
 */
inline const char* to_string(LoadBalancingStrategy strategy) {
    switch (strategy) {
        case LoadBalancingStrategy::ROUND_ROBIN: return "ROUND_ROBIN";
        case LoadBalancingStrategy::LEAST_LOADED: return "LEAST_LOADED";
        case LoadBalancingStrategy::RANDOM: return "RANDOM";
        case LoadBalancingStrategy::LEAST_RECENTLY_USED: return "LEAST_RECENTLY_USED";
        default: return "INVALID";
    }
}

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_CONNECTION_POOL_H
