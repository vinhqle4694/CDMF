/**
 * @file proxy_factory.h
 * @brief Proxy factory for creating and managing service proxies
 *
 * Provides a factory interface for creating, caching, and managing service proxy
 * instances with lifecycle management, health monitoring, and statistics aggregation.
 * Supports multiple transport types and automatic reconnection.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Factory Agent
 */

#ifndef CDMF_IPC_PROXY_FACTORY_H
#define CDMF_IPC_PROXY_FACTORY_H

#include "service_proxy.h"
#include "transport.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <functional>
#include <chrono>
#include <atomic>
#include <vector>

namespace cdmf {
namespace ipc {

/**
 * @brief Proxy instance information
 */
struct ProxyInstanceInfo {
    /** Service name/identifier */
    std::string service_name;

    /** Service endpoint */
    std::string endpoint;

    /** Transport type */
    TransportType transport_type;

    /** Creation timestamp */
    std::chrono::system_clock::time_point created_at;

    /** Last access timestamp */
    std::chrono::system_clock::time_point last_accessed;

    /** Number of active references */
    uint32_t ref_count;

    /** Connected status */
    bool is_connected;

    /** Health check status */
    bool is_healthy;

    /** Last health check timestamp */
    std::chrono::system_clock::time_point last_health_check;

    ProxyInstanceInfo()
        : transport_type(TransportType::UNKNOWN)
        , created_at(std::chrono::system_clock::now())
        , last_accessed(std::chrono::system_clock::now())
        , ref_count(0)
        , is_connected(false)
        , is_healthy(false)
        , last_health_check(std::chrono::system_clock::now())
    {}
};

/**
 * @brief Proxy factory configuration
 */
struct ProxyFactoryConfig {
    /** Enable proxy caching/reuse */
    bool enable_caching = true;

    /** Maximum number of cached proxies */
    uint32_t max_cached_proxies = 100;

    /** Idle timeout for cached proxies (seconds) */
    uint32_t idle_timeout_seconds = 300;

    /** Enable automatic health checking */
    bool enable_health_check = true;

    /** Health check interval (seconds) */
    uint32_t health_check_interval_seconds = 30;

    /** Enable automatic reconnection for failed proxies */
    bool enable_auto_reconnect = true;

    /** Maximum reconnection attempts */
    uint32_t max_reconnect_attempts = 3;

    /** Enable statistics aggregation */
    bool enable_statistics = true;

    /** Default proxy configuration template */
    ProxyConfig default_proxy_config;

    ProxyFactoryConfig() = default;
};

/**
 * @brief Aggregated statistics snapshot (copyable)
 */
struct AggregatedStatsSnapshot {
    /** Total number of proxies created */
    uint64_t total_proxies_created = 0;

    /** Number of currently active proxies */
    uint32_t active_proxies = 0;

    /** Number of cached proxies */
    uint32_t cached_proxies = 0;

    /** Total calls across all proxies */
    uint64_t total_calls = 0;

    /** Total successful calls */
    uint64_t successful_calls = 0;

    /** Total failed calls */
    uint64_t failed_calls = 0;

    /** Total timeout calls */
    uint64_t timeout_calls = 0;

    /** Number of cache hits */
    uint64_t cache_hits = 0;

    /** Number of cache misses */
    uint64_t cache_misses = 0;

    /** Number of health check failures */
    uint64_t health_check_failures = 0;

    /** Number of reconnection attempts */
    uint64_t reconnection_attempts = 0;

    /** Number of successful reconnections */
    uint64_t successful_reconnections = 0;
};

/**
 * @brief Aggregated statistics across all proxies (internal, uses atomics)
 */
struct AggregatedStats {
    /** Total number of proxies created */
    std::atomic<uint64_t> total_proxies_created{0};

    /** Number of currently active proxies */
    std::atomic<uint32_t> active_proxies{0};

    /** Number of cached proxies */
    std::atomic<uint32_t> cached_proxies{0};

    /** Total calls across all proxies */
    std::atomic<uint64_t> total_calls{0};

    /** Total successful calls */
    std::atomic<uint64_t> successful_calls{0};

    /** Total failed calls */
    std::atomic<uint64_t> failed_calls{0};

    /** Total timeout calls */
    std::atomic<uint64_t> timeout_calls{0};

    /** Number of cache hits */
    std::atomic<uint64_t> cache_hits{0};

    /** Number of cache misses */
    std::atomic<uint64_t> cache_misses{0};

    /** Number of health check failures */
    std::atomic<uint64_t> health_check_failures{0};

    /** Number of reconnection attempts */
    std::atomic<uint64_t> reconnection_attempts{0};

    /** Number of successful reconnections */
    std::atomic<uint64_t> successful_reconnections{0};
};

/**
 * @brief Health check callback type
 *
 * Called periodically to check proxy health. Should return true if healthy.
 * Parameters: service_name, proxy instance
 */
using HealthCheckCallback = std::function<bool(const std::string&, ServiceProxyPtr)>;

/**
 * @brief Proxy creation callback type
 *
 * Called after a new proxy is created.
 * Parameters: service_name, proxy instance
 */
using ProxyCreatedCallback = std::function<void(const std::string&, ServiceProxyPtr)>;

/**
 * @brief Proxy destroyed callback type
 *
 * Called before a proxy is destroyed.
 * Parameters: service_name, proxy instance
 */
using ProxyDestroyedCallback = std::function<void(const std::string&, ServiceProxyPtr)>;

/**
 * @brief Proxy cache entry
 */
struct ProxyCacheEntry {
    /** Proxy instance */
    ServiceProxyPtr proxy;

    /** Instance information */
    ProxyInstanceInfo info;

    /** Configuration used to create this proxy */
    ProxyConfig config;

    /** Weak references for reference counting */
    std::weak_ptr<ServiceProxy> weak_ref;

    ProxyCacheEntry() = default;
    ProxyCacheEntry(ServiceProxyPtr p, const ProxyConfig& cfg)
        : proxy(p), config(cfg), weak_ref(p)
    {
        info.created_at = std::chrono::system_clock::now();
        info.last_accessed = info.created_at;
    }
};

/**
 * @brief Proxy factory class
 *
 * Factory for creating and managing service proxy instances. Provides:
 * - Proxy caching and reuse for same endpoints
 * - Lifecycle management (creation, destruction, cleanup)
 * - Health monitoring and automatic reconnection
 * - Configuration management per service
 * - Statistics aggregation across all proxies
 * - Thread-safe operations
 *
 * Thread Safety: This class is thread-safe for all operations.
 */
class ProxyFactory {
public:
    /**
     * @brief Get singleton instance of proxy factory
     * @return Reference to proxy factory instance
     */
    static ProxyFactory& getInstance();

    /**
     * @brief Initialize the factory with configuration
     * @param config Factory configuration
     * @return true if initialized successfully, false otherwise
     */
    bool initialize(const ProxyFactoryConfig& config);

    /**
     * @brief Shutdown the factory and cleanup all proxies
     */
    void shutdown();

    /**
     * @brief Check if factory is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;

    // Proxy creation and retrieval

    /**
     * @brief Create or get cached proxy for a service
     * @param service_name Service name/identifier
     * @param config Proxy configuration
     * @return Proxy instance or nullptr on failure
     */
    ServiceProxyPtr getProxy(const std::string& service_name, const ProxyConfig& config);

    /**
     * @brief Create or get cached proxy with endpoint
     * @param service_name Service name/identifier
     * @param endpoint Service endpoint
     * @param transport_type Transport type
     * @return Proxy instance or nullptr on failure
     */
    ServiceProxyPtr getProxy(
        const std::string& service_name,
        const std::string& endpoint,
        TransportType transport_type = TransportType::UNIX_SOCKET
    );

    /**
     * @brief Create a new proxy (bypass cache)
     * @param config Proxy configuration
     * @return Proxy instance or nullptr on failure
     */
    ServiceProxyPtr createProxy(const ProxyConfig& config);

    /**
     * @brief Create and connect a proxy
     * @param config Proxy configuration
     * @return Connected proxy instance or nullptr on failure
     */
    ServiceProxyPtr createAndConnect(const ProxyConfig& config);

    /**
     * @brief Create and connect a proxy with endpoint
     * @param service_name Service name/identifier
     * @param endpoint Service endpoint
     * @param transport_type Transport type
     * @return Connected proxy instance or nullptr on failure
     */
    ServiceProxyPtr createAndConnect(
        const std::string& service_name,
        const std::string& endpoint,
        TransportType transport_type = TransportType::UNIX_SOCKET
    );

    // Cache management

    /**
     * @brief Remove proxy from cache
     * @param service_name Service name
     */
    void removeFromCache(const std::string& service_name);

    /**
     * @brief Clear all cached proxies
     */
    void clearCache();

    /**
     * @brief Get number of cached proxies
     * @return Number of cached proxies
     */
    uint32_t getCachedProxyCount() const;

    /**
     * @brief Check if a proxy is cached
     * @param service_name Service name
     * @return true if cached, false otherwise
     */
    bool isCached(const std::string& service_name) const;

    // Lifecycle management

    /**
     * @brief Destroy a proxy instance
     * @param service_name Service name
     * @return true if destroyed, false if not found
     */
    bool destroyProxy(const std::string& service_name);

    /**
     * @brief Destroy all proxies
     */
    void destroyAllProxies();

    /**
     * @brief Cleanup idle proxies (exceeding idle timeout)
     * @return Number of proxies cleaned up
     */
    uint32_t cleanupIdleProxies();

    // Health monitoring

    /**
     * @brief Perform health check on a specific proxy
     * @param service_name Service name
     * @return true if healthy, false otherwise
     */
    bool checkProxyHealth(const std::string& service_name);

    /**
     * @brief Perform health check on all cached proxies
     * @return Number of unhealthy proxies
     */
    uint32_t checkAllProxiesHealth();

    /**
     * @brief Set custom health check callback
     * @param callback Health check function
     */
    void setHealthCheckCallback(HealthCheckCallback callback);

    /**
     * @brief Reconnect a failed proxy
     * @param service_name Service name
     * @return true if reconnected successfully, false otherwise
     */
    bool reconnectProxy(const std::string& service_name);

    /**
     * @brief Reconnect all failed proxies
     * @return Number of successfully reconnected proxies
     */
    uint32_t reconnectAllProxies();

    // Configuration

    /**
     * @brief Get factory configuration
     * @return Current configuration
     */
    const ProxyFactoryConfig& getConfig() const;

    /**
     * @brief Update factory configuration
     * @param config New configuration
     */
    void updateConfig(const ProxyFactoryConfig& config);

    /**
     * @brief Set default proxy configuration template
     * @param config Default proxy configuration
     */
    void setDefaultProxyConfig(const ProxyConfig& config);

    /**
     * @brief Get default proxy configuration template
     * @return Default proxy configuration
     */
    const ProxyConfig& getDefaultProxyConfig() const;

    // Statistics

    /**
     * @brief Get aggregated statistics
     * @return Aggregated statistics across all proxies
     */
    AggregatedStatsSnapshot getAggregatedStats() const;

    /**
     * @brief Get proxy instance information
     * @param service_name Service name
     * @return Proxy instance info or nullptr if not found
     */
    std::shared_ptr<ProxyInstanceInfo> getProxyInfo(const std::string& service_name) const;

    /**
     * @brief Get all proxy instance information
     * @return Map of service name to instance info
     */
    std::map<std::string, ProxyInstanceInfo> getAllProxyInfo() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStats();

    // Callbacks

    /**
     * @brief Set proxy created callback
     * @param callback Callback function
     */
    void setProxyCreatedCallback(ProxyCreatedCallback callback);

    /**
     * @brief Set proxy destroyed callback
     * @param callback Callback function
     */
    void setProxyDestroyedCallback(ProxyDestroyedCallback callback);

    // Utility

    /**
     * @brief Start background tasks (health checking, cleanup)
     * @return true if started successfully, false otherwise
     */
    bool startBackgroundTasks();

    /**
     * @brief Stop background tasks
     */
    void stopBackgroundTasks();

private:
    /** Private constructor for singleton */
    ProxyFactory();

    /** Destructor */
    ~ProxyFactory();

    // Disable copy and move
    ProxyFactory(const ProxyFactory&) = delete;
    ProxyFactory& operator=(const ProxyFactory&) = delete;
    ProxyFactory(ProxyFactory&&) = delete;
    ProxyFactory& operator=(ProxyFactory&&) = delete;

    /**
     * @brief Generate cache key from service name and endpoint
     * @param service_name Service name
     * @param endpoint Endpoint
     * @return Cache key
     */
    std::string generateCacheKey(const std::string& service_name, const std::string& endpoint) const;

    /**
     * @brief Get proxy from cache (internal, assumes lock held)
     * @param cache_key Cache key
     * @return Cached proxy or nullptr
     */
    ServiceProxyPtr getFromCacheInternal(const std::string& cache_key);

    /**
     * @brief Add proxy to cache (internal, assumes lock held)
     * @param cache_key Cache key
     * @param proxy Proxy instance
     * @param config Proxy configuration
     */
    void addToCacheInternal(const std::string& cache_key, ServiceProxyPtr proxy, const ProxyConfig& config);

    /**
     * @brief Remove expired proxies from cache
     * @return Number of proxies removed
     */
    uint32_t removeExpiredProxies();

    /**
     * @brief Default health check implementation
     * @param service_name Service name
     * @param proxy Proxy instance
     * @return true if healthy, false otherwise
     */
    bool defaultHealthCheck(const std::string& service_name, ServiceProxyPtr proxy);

    /**
     * @brief Background health check thread
     */
    void healthCheckThread();

    /**
     * @brief Background cleanup thread
     */
    void cleanupThread();

    /**
     * @brief Update proxy access time
     * @param cache_key Cache key
     */
    void updateAccessTime(const std::string& cache_key);

    /**
     * @brief Update aggregated statistics from proxy
     * @param proxy Proxy instance
     */
    void updateAggregatedStats(ServiceProxyPtr proxy);

    /** Factory configuration */
    ProxyFactoryConfig config_;

    /** Proxy cache (cache_key -> entry) */
    std::map<std::string, ProxyCacheEntry> proxy_cache_;

    /** Mutex for thread safety */
    mutable std::mutex mutex_;

    /** Initialized flag */
    std::atomic<bool> initialized_{false};

    /** Running flag for background tasks */
    std::atomic<bool> running_{false};

    /** Health check thread */
    std::thread health_check_thread_;

    /** Cleanup thread */
    std::thread cleanup_thread_;

    /** Aggregated statistics */
    AggregatedStats stats_;

    /** Health check callback */
    HealthCheckCallback health_check_callback_;

    /** Proxy created callback */
    ProxyCreatedCallback proxy_created_callback_;

    /** Proxy destroyed callback */
    ProxyDestroyedCallback proxy_destroyed_callback_;
};

/**
 * @brief Proxy builder for fluent configuration
 */
class ProxyBuilder {
public:
    /**
     * @brief Constructor
     */
    ProxyBuilder();

    /**
     * @brief Set service name
     * @param name Service name
     * @return Reference to builder
     */
    ProxyBuilder& withServiceName(const std::string& name);

    /**
     * @brief Set endpoint
     * @param endpoint Endpoint address/path
     * @return Reference to builder
     */
    ProxyBuilder& withEndpoint(const std::string& endpoint);

    /**
     * @brief Set transport type
     * @param type Transport type
     * @return Reference to builder
     */
    ProxyBuilder& withTransportType(TransportType type);

    /**
     * @brief Set timeout
     * @param timeout_ms Timeout in milliseconds
     * @return Reference to builder
     */
    ProxyBuilder& withTimeout(uint32_t timeout_ms);

    /**
     * @brief Set retry policy
     * @param policy Retry policy
     * @return Reference to builder
     */
    ProxyBuilder& withRetryPolicy(const RetryPolicy& policy);

    /**
     * @brief Enable auto reconnect
     * @param enabled Enable flag
     * @return Reference to builder
     */
    ProxyBuilder& withAutoReconnect(bool enabled);

    /**
     * @brief Set serialization format
     * @param format Serialization format
     * @return Reference to builder
     */
    ProxyBuilder& withSerializationFormat(SerializationFormat format);

    /**
     * @brief Set transport configuration
     * @param config Transport configuration
     * @return Reference to builder
     */
    ProxyBuilder& withTransportConfig(const TransportConfig& config);

    /**
     * @brief Build and create proxy
     * @return Proxy instance or nullptr on failure
     */
    ServiceProxyPtr build();

    /**
     * @brief Build and create connected proxy
     * @return Connected proxy instance or nullptr on failure
     */
    ServiceProxyPtr buildAndConnect();

    /**
     * @brief Build configuration only (no proxy creation)
     * @return Proxy configuration
     */
    ProxyConfig buildConfig() const;

private:
    /** Service name */
    std::string service_name_;

    /** Proxy configuration */
    ProxyConfig config_;
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_PROXY_FACTORY_H
