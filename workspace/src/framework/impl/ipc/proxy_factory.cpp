/**
 * @file proxy_factory.cpp
 * @brief Proxy factory implementation
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Factory Agent
 */

#include "ipc/proxy_factory.h"
#include "utils/log.h"
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

namespace cdmf {
namespace ipc {

// ProxyFactory implementation

ProxyFactory::ProxyFactory()
    : initialized_(false)
    , running_(false)
{
    LOGD_FMT("ProxyFactory constructor called");
}

ProxyFactory::~ProxyFactory() {
    LOGD_FMT("ProxyFactory destructor called");
    shutdown();
}

ProxyFactory& ProxyFactory::getInstance() {
    LOGD_FMT("Getting ProxyFactory singleton instance");
    static ProxyFactory instance;
    return instance;
}

bool ProxyFactory::initialize(const ProxyFactoryConfig& config) {
    LOGI_FMT("ProxyFactory initializing with max_cached_proxies=" << config.max_cached_proxies
             << ", enable_health_check=" << config.enable_health_check
             << ", enable_caching=" << config.enable_caching);
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        LOGW_FMT("ProxyFactory already initialized");
        return false;  // Already initialized
    }

    config_ = config;
    initialized_ = true;

    // Start background tasks if configured
    if (config_.enable_health_check || config_.enable_caching) {
        startBackgroundTasks();
    }

    LOGI_FMT("ProxyFactory initialized successfully");
    return true;
}

void ProxyFactory::shutdown() {
    LOGI_FMT("ProxyFactory shutting down");
    stopBackgroundTasks();

    std::lock_guard<std::mutex> lock(mutex_);

    // Disconnect and destroy all proxies
    size_t proxy_count = proxy_cache_.size();
    for (auto& entry : proxy_cache_) {
        if (entry.second.proxy) {
            // Call destroyed callback
            if (proxy_destroyed_callback_) {
                proxy_destroyed_callback_(entry.first, entry.second.proxy);
            }

            // Disconnect proxy
            if (entry.second.proxy->isConnected()) {
                entry.second.proxy->disconnect();
            }
        }
    }

    proxy_cache_.clear();
    initialized_ = false;
    LOGI_FMT("ProxyFactory shutdown complete, destroyed " << proxy_count << " proxies");
}

bool ProxyFactory::isInitialized() const {
    return initialized_;
}

ServiceProxyPtr ProxyFactory::getProxy(const std::string& service_name, const ProxyConfig& config) {
    LOGD_FMT("Getting proxy for service=" << service_name << ", endpoint=" << config.transport_config.endpoint);

    if (!initialized_) {
        LOGE_FMT("ProxyFactory not initialized, cannot get proxy for service=" << service_name);
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Generate cache key
    std::string cache_key = generateCacheKey(service_name, config.transport_config.endpoint);

    // Try to get from cache if caching is enabled
    if (config_.enable_caching) {
        ServiceProxyPtr cached_proxy = getFromCacheInternal(cache_key);
        if (cached_proxy) {
            stats_.cache_hits++;
            updateAccessTime(cache_key);
            LOGD_FMT("Cache hit for service=" << service_name << ", cache_hits=" << stats_.cache_hits.load());
            return cached_proxy;
        }
        stats_.cache_misses++;
        LOGD_FMT("Cache miss for service=" << service_name << ", cache_misses=" << stats_.cache_misses.load());
    }

    // Create new proxy
    ServiceProxyPtr proxy = std::make_shared<ServiceProxy>(config);
    if (!proxy) {
        LOGE_FMT("Failed to create proxy for service=" << service_name);
        return nullptr;
    }

    stats_.total_proxies_created++;
    stats_.active_proxies++;

    // Add to cache if enabled
    if (config_.enable_caching) {
        addToCacheInternal(cache_key, proxy, config);
        stats_.cached_proxies++;
    }

    // Call created callback
    if (proxy_created_callback_) {
        proxy_created_callback_(service_name, proxy);
    }

    LOGI_FMT("Created new proxy for service=" << service_name << ", total_created=" << stats_.total_proxies_created.load());
    return proxy;
}

ServiceProxyPtr ProxyFactory::getProxy(
    const std::string& service_name,
    const std::string& endpoint,
    TransportType transport_type
) {
    // Build configuration from defaults
    ProxyConfig config = config_.default_proxy_config;
    config.service_name = service_name;
    config.transport_config.endpoint = endpoint;
    config.transport_config.type = transport_type;

    return getProxy(service_name, config);
}

ServiceProxyPtr ProxyFactory::createProxy(const ProxyConfig& config) {
    LOGD_FMT("ProxyFactory::createProxy called for service=" << config.service_name);

    if (!initialized_) {
        LOGE_FMT("ProxyFactory not initialized, cannot create proxy");
        return nullptr;
    }

    // Create new proxy without caching
    ServiceProxyPtr proxy = std::make_shared<ServiceProxy>(config);
    if (!proxy) {
        LOGE_FMT("Failed to create proxy for service=" << config.service_name);
        return nullptr;
    }

    stats_.total_proxies_created++;
    stats_.active_proxies++;

    // Call created callback
    if (proxy_created_callback_) {
        proxy_created_callback_(config.service_name, proxy);
    }

    LOGI_FMT("Created proxy for service=" << config.service_name << " without caching");
    return proxy;
}

ServiceProxyPtr ProxyFactory::createAndConnect(const ProxyConfig& config) {
    LOGD_FMT("ProxyFactory::createAndConnect called for service=" << config.service_name);

    ServiceProxyPtr proxy = createProxy(config);
    if (!proxy) {
        LOGE_FMT("ProxyFactory::createAndConnect: createProxy failed for service=" << config.service_name);
        return nullptr;
    }

    // Attempt to connect
    if (!proxy->connect()) {
        LOGE_FMT("ProxyFactory::createAndConnect: connect failed for service=" << config.service_name);
        stats_.active_proxies--;
        return nullptr;
    }

    LOGI_FMT("ProxyFactory::createAndConnect: successfully created and connected proxy for service=" << config.service_name);
    return proxy;
}

ServiceProxyPtr ProxyFactory::createAndConnect(
    const std::string& service_name,
    const std::string& endpoint,
    TransportType transport_type
) {
    LOGD_FMT("ProxyFactory::createAndConnect overload called with service=" << service_name
             << ", endpoint=" << endpoint << ", transport_type=" << static_cast<int>(transport_type));

    // Build configuration
    ProxyConfig config = config_.default_proxy_config;
    config.service_name = service_name;
    config.transport_config.endpoint = endpoint;
    config.transport_config.type = transport_type;

    return createAndConnect(config);
}

void ProxyFactory::removeFromCache(const std::string& service_name) {
    LOGD_FMT("ProxyFactory::removeFromCache called for service=" << service_name);
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t removed_count = 0;
    // Find all cache entries matching service name
    auto it = proxy_cache_.begin();
    while (it != proxy_cache_.end()) {
        if (it->second.info.service_name == service_name) {
            // Call destroyed callback
            if (proxy_destroyed_callback_ && it->second.proxy) {
                proxy_destroyed_callback_(service_name, it->second.proxy);
            }

            it = proxy_cache_.erase(it);
            stats_.cached_proxies--;
            stats_.active_proxies--;
            removed_count++;
        } else {
            ++it;
        }
    }

    LOGD_FMT("ProxyFactory::removeFromCache: removed " << removed_count << " proxies for service=" << service_name);
}

void ProxyFactory::clearCache() {
    LOGD_FMT("ProxyFactory::clearCache called");
    std::lock_guard<std::mutex> lock(mutex_);

    // Call destroyed callback for all
    for (auto& entry : proxy_cache_) {
        if (proxy_destroyed_callback_ && entry.second.proxy) {
            proxy_destroyed_callback_(entry.first, entry.second.proxy);
        }
    }

    uint32_t count = proxy_cache_.size();
    proxy_cache_.clear();

    stats_.cached_proxies -= count;
    stats_.active_proxies -= count;

    LOGI_FMT("ProxyFactory::clearCache: cleared " << count << " cached proxies");
}

uint32_t ProxyFactory::getCachedProxyCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(proxy_cache_.size());
}

bool ProxyFactory::isCached(const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& entry : proxy_cache_) {
        if (entry.second.info.service_name == service_name) {
            return true;
        }
    }
    return false;
}

bool ProxyFactory::destroyProxy(const std::string& service_name) {
    LOGD_FMT("ProxyFactory::destroyProxy called for service=" << service_name);
    std::lock_guard<std::mutex> lock(mutex_);

    bool found = false;
    uint32_t destroyed_count = 0;
    auto it = proxy_cache_.begin();
    while (it != proxy_cache_.end()) {
        if (it->second.info.service_name == service_name) {
            // Disconnect if connected
            if (it->second.proxy && it->second.proxy->isConnected()) {
                it->second.proxy->disconnect();
            }

            // Call destroyed callback
            if (proxy_destroyed_callback_ && it->second.proxy) {
                proxy_destroyed_callback_(service_name, it->second.proxy);
            }

            it = proxy_cache_.erase(it);
            stats_.cached_proxies--;
            stats_.active_proxies--;
            destroyed_count++;
            found = true;
        } else {
            ++it;
        }
    }

    if (found) {
        LOGI_FMT("ProxyFactory::destroyProxy: destroyed " << destroyed_count << " proxies for service=" << service_name);
    } else {
        LOGW_FMT("ProxyFactory::destroyProxy: no proxy found for service=" << service_name);
    }

    return found;
}

void ProxyFactory::destroyAllProxies() {
    LOGI_FMT("ProxyFactory::destroyAllProxies called");
    std::lock_guard<std::mutex> lock(mutex_);

    // Disconnect all
    for (auto& entry : proxy_cache_) {
        if (entry.second.proxy && entry.second.proxy->isConnected()) {
            entry.second.proxy->disconnect();
        }

        // Call destroyed callback
        if (proxy_destroyed_callback_ && entry.second.proxy) {
            proxy_destroyed_callback_(entry.first, entry.second.proxy);
        }
    }

    uint32_t count = proxy_cache_.size();
    proxy_cache_.clear();

    stats_.cached_proxies -= count;
    stats_.active_proxies -= count;

    LOGI_FMT("ProxyFactory::destroyAllProxies: destroyed " << count << " proxies");
}

uint32_t ProxyFactory::cleanupIdleProxies() {
    LOGD_FMT("ProxyFactory::cleanupIdleProxies called");
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto idle_timeout = std::chrono::seconds(config_.idle_timeout_seconds);

    uint32_t cleaned = 0;
    auto it = proxy_cache_.begin();
    while (it != proxy_cache_.end()) {
        auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.info.last_accessed
        );

        if (idle_duration >= idle_timeout) {
            // Proxy is idle, remove it
            if (it->second.proxy && it->second.proxy->isConnected()) {
                it->second.proxy->disconnect();
            }

            // Call destroyed callback
            if (proxy_destroyed_callback_ && it->second.proxy) {
                proxy_destroyed_callback_(it->second.info.service_name, it->second.proxy);
            }

            it = proxy_cache_.erase(it);
            stats_.cached_proxies--;
            stats_.active_proxies--;
            cleaned++;
        } else {
            ++it;
        }
    }

    if (cleaned > 0) {
        LOGI_FMT("ProxyFactory::cleanupIdleProxies: cleaned up " << cleaned << " idle proxies");
    } else {
        LOGD_FMT("ProxyFactory::cleanupIdleProxies: no idle proxies to clean");
    }

    return cleaned;
}

bool ProxyFactory::checkProxyHealth(const std::string& service_name) {
    LOGD_FMT("ProxyFactory::checkProxyHealth called for service=" << service_name);
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry : proxy_cache_) {
        if (entry.second.info.service_name == service_name) {
            bool healthy = false;

            // Use custom health check if available
            if (health_check_callback_) {
                healthy = health_check_callback_(service_name, entry.second.proxy);
            } else {
                healthy = defaultHealthCheck(service_name, entry.second.proxy);
            }

            entry.second.info.is_healthy = healthy;
            entry.second.info.last_health_check = std::chrono::system_clock::now();

            if (!healthy) {
                stats_.health_check_failures++;
                LOGW_FMT("ProxyFactory::checkProxyHealth: service=" << service_name << " is unhealthy");
            } else {
                LOGD_FMT("ProxyFactory::checkProxyHealth: service=" << service_name << " is healthy");
            }

            return healthy;
        }
    }

    LOGW_FMT("ProxyFactory::checkProxyHealth: service=" << service_name << " not found in cache");
    return false;
}

uint32_t ProxyFactory::checkAllProxiesHealth() {
    LOGD_FMT("ProxyFactory::checkAllProxiesHealth called");
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t unhealthy_count = 0;
    uint32_t total_proxies = proxy_cache_.size();

    for (auto& entry : proxy_cache_) {
        bool healthy = false;

        // Use custom health check if available
        if (health_check_callback_) {
            healthy = health_check_callback_(entry.second.info.service_name, entry.second.proxy);
        } else {
            healthy = defaultHealthCheck(entry.second.info.service_name, entry.second.proxy);
        }

        entry.second.info.is_healthy = healthy;
        entry.second.info.last_health_check = std::chrono::system_clock::now();

        if (!healthy) {
            unhealthy_count++;
            stats_.health_check_failures++;
        }
    }

    LOGI_FMT("ProxyFactory::checkAllProxiesHealth: checked " << total_proxies << " proxies, "
             << unhealthy_count << " unhealthy");
    return unhealthy_count;
}

void ProxyFactory::setHealthCheckCallback(HealthCheckCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    health_check_callback_ = callback;
}

bool ProxyFactory::reconnectProxy(const std::string& service_name) {
    LOGD_FMT("ProxyFactory::reconnectProxy called for service=" << service_name);
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry : proxy_cache_) {
        if (entry.second.info.service_name == service_name) {
            if (!entry.second.proxy) {
                LOGE_FMT("ProxyFactory::reconnectProxy: proxy is null for service=" << service_name);
                return false;
            }

            stats_.reconnection_attempts++;

            // Disconnect first if connected
            if (entry.second.proxy->isConnected()) {
                entry.second.proxy->disconnect();
            }

            // Attempt reconnection with retries
            uint32_t attempts = 0;
            while (attempts < config_.max_reconnect_attempts) {
                if (entry.second.proxy->connect()) {
                    entry.second.info.is_connected = true;
                    stats_.successful_reconnections++;
                    LOGI_FMT("ProxyFactory::reconnectProxy: successfully reconnected service=" << service_name
                             << " on attempt " << (attempts + 1));
                    return true;
                }
                attempts++;
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << attempts)));
            }

            entry.second.info.is_connected = false;
            LOGE_FMT("ProxyFactory::reconnectProxy: failed to reconnect service=" << service_name
                     << " after " << config_.max_reconnect_attempts << " attempts");
            return false;
        }
    }

    LOGW_FMT("ProxyFactory::reconnectProxy: service=" << service_name << " not found in cache");
    return false;
}

uint32_t ProxyFactory::reconnectAllProxies() {
    LOGI_FMT("ProxyFactory::reconnectAllProxies called");
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t reconnected_count = 0;
    uint32_t total_disconnected = 0;

    for (auto& entry : proxy_cache_) {
        if (!entry.second.proxy || entry.second.proxy->isConnected()) {
            continue;  // Skip if no proxy or already connected
        }

        total_disconnected++;
        stats_.reconnection_attempts++;

        // Attempt reconnection
        uint32_t attempts = 0;
        while (attempts < config_.max_reconnect_attempts) {
            if (entry.second.proxy->connect()) {
                entry.second.info.is_connected = true;
                stats_.successful_reconnections++;
                reconnected_count++;
                LOGD_FMT("ProxyFactory::reconnectAllProxies: reconnected service="
                         << entry.second.info.service_name);
                break;
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << attempts)));
        }

        if (attempts >= config_.max_reconnect_attempts) {
            entry.second.info.is_connected = false;
            LOGW_FMT("ProxyFactory::reconnectAllProxies: failed to reconnect service="
                     << entry.second.info.service_name);
        }
    }

    LOGI_FMT("ProxyFactory::reconnectAllProxies: reconnected " << reconnected_count
             << " out of " << total_disconnected << " disconnected proxies");
    return reconnected_count;
}

const ProxyFactoryConfig& ProxyFactory::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ProxyFactory::updateConfig(const ProxyFactoryConfig& config) {
    LOGI_FMT("ProxyFactory::updateConfig called");
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    LOGD_FMT("ProxyFactory::updateConfig: configuration updated");
}

void ProxyFactory::setDefaultProxyConfig(const ProxyConfig& config) {
    LOGD_FMT("ProxyFactory::setDefaultProxyConfig called for service=" << config.service_name);
    std::lock_guard<std::mutex> lock(mutex_);
    config_.default_proxy_config = config;
    LOGD_FMT("ProxyFactory::setDefaultProxyConfig: default configuration set");
}

const ProxyConfig& ProxyFactory::getDefaultProxyConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.default_proxy_config;
}

AggregatedStatsSnapshot ProxyFactory::getAggregatedStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Create aggregated stats by loading atomic values
    AggregatedStatsSnapshot agg_stats;
    agg_stats.total_proxies_created = stats_.total_proxies_created.load();
    agg_stats.active_proxies = stats_.active_proxies.load();
    agg_stats.cached_proxies = stats_.cached_proxies.load();
    agg_stats.cache_hits = stats_.cache_hits.load();
    agg_stats.cache_misses = stats_.cache_misses.load();

    // Aggregate stats from all proxies
    uint64_t total_calls = 0;
    uint64_t successful_calls = 0;
    uint64_t failed_calls = 0;
    uint64_t timeout_calls = 0;

    for (const auto& entry : proxy_cache_) {
        if (entry.second.proxy) {
            ProxyStatsSnapshot proxy_stats = entry.second.proxy->getStats();
            total_calls += proxy_stats.total_calls;
            successful_calls += proxy_stats.successful_calls;
            failed_calls += proxy_stats.failed_calls;
            timeout_calls += proxy_stats.timeout_calls;
        }
    }

    agg_stats.total_calls = total_calls;
    agg_stats.successful_calls = successful_calls;
    agg_stats.failed_calls = failed_calls;
    agg_stats.timeout_calls = timeout_calls;

    return agg_stats;
}

std::shared_ptr<ProxyInstanceInfo> ProxyFactory::getProxyInfo(const std::string& service_name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& entry : proxy_cache_) {
        if (entry.second.info.service_name == service_name) {
            return std::make_shared<ProxyInstanceInfo>(entry.second.info);
        }
    }

    return nullptr;
}

std::map<std::string, ProxyInstanceInfo> ProxyFactory::getAllProxyInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<std::string, ProxyInstanceInfo> info_map;
    for (const auto& entry : proxy_cache_) {
        info_map[entry.first] = entry.second.info;
    }

    return info_map;
}

void ProxyFactory::resetStats() {
    LOGI_FMT("ProxyFactory::resetStats called");
    std::lock_guard<std::mutex> lock(mutex_);

    stats_.total_calls = 0;
    stats_.successful_calls = 0;
    stats_.failed_calls = 0;
    stats_.timeout_calls = 0;
    stats_.cache_hits = 0;
    stats_.cache_misses = 0;
    stats_.health_check_failures = 0;
    stats_.reconnection_attempts = 0;
    stats_.successful_reconnections = 0;

    LOGD_FMT("ProxyFactory::resetStats: all statistics reset");
}

void ProxyFactory::setProxyCreatedCallback(ProxyCreatedCallback callback) {
    LOGD_FMT("ProxyFactory::setProxyCreatedCallback called");
    std::lock_guard<std::mutex> lock(mutex_);
    proxy_created_callback_ = callback;
}

void ProxyFactory::setProxyDestroyedCallback(ProxyDestroyedCallback callback) {
    LOGD_FMT("ProxyFactory::setProxyDestroyedCallback called");
    std::lock_guard<std::mutex> lock(mutex_);
    proxy_destroyed_callback_ = callback;
}

bool ProxyFactory::startBackgroundTasks() {
    LOGI_FMT("ProxyFactory::startBackgroundTasks called");

    if (running_) {
        LOGW_FMT("ProxyFactory::startBackgroundTasks: background tasks already running");
        return false;  // Already running
    }

    running_ = true;

    // Start health check thread
    if (config_.enable_health_check) {
        health_check_thread_ = std::thread(&ProxyFactory::healthCheckThread, this);
        LOGD_FMT("ProxyFactory::startBackgroundTasks: health check thread started");
    }

    // Start cleanup thread
    if (config_.enable_caching) {
        cleanup_thread_ = std::thread(&ProxyFactory::cleanupThread, this);
        LOGD_FMT("ProxyFactory::startBackgroundTasks: cleanup thread started");
    }

    LOGI_FMT("ProxyFactory::startBackgroundTasks: all background tasks started");
    return true;
}

void ProxyFactory::stopBackgroundTasks() {
    LOGI_FMT("ProxyFactory::stopBackgroundTasks called");
    running_ = false;

    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
        LOGD_FMT("ProxyFactory::stopBackgroundTasks: health check thread stopped");
    }

    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
        LOGD_FMT("ProxyFactory::stopBackgroundTasks: cleanup thread stopped");
    }

    LOGI_FMT("ProxyFactory::stopBackgroundTasks: all background tasks stopped");
}

std::string ProxyFactory::generateCacheKey(const std::string& service_name, const std::string& endpoint) const {
    std::ostringstream oss;
    oss << service_name << ":" << endpoint;
    return oss.str();
}

ServiceProxyPtr ProxyFactory::getFromCacheInternal(const std::string& cache_key) {
    auto it = proxy_cache_.find(cache_key);
    if (it != proxy_cache_.end()) {
        // Return the cached proxy (strong reference)
        it->second.info.ref_count++;
        return it->second.proxy;
    }
    return nullptr;
}

void ProxyFactory::addToCacheInternal(const std::string& cache_key, ServiceProxyPtr proxy, const ProxyConfig& config) {
    // Check cache size limit
    if (proxy_cache_.size() >= config_.max_cached_proxies) {
        // Remove oldest proxy
        removeExpiredProxies();

        // If still at limit, remove the least recently accessed
        if (proxy_cache_.size() >= config_.max_cached_proxies) {
            auto oldest = proxy_cache_.begin();
            for (auto it = proxy_cache_.begin(); it != proxy_cache_.end(); ++it) {
                if (it->second.info.last_accessed < oldest->second.info.last_accessed) {
                    oldest = it;
                }
            }
            proxy_cache_.erase(oldest);
            stats_.cached_proxies--;
        }
    }

    // Add to cache
    ProxyCacheEntry entry(proxy, config);
    entry.info.service_name = config.service_name;
    entry.info.endpoint = config.transport_config.endpoint;
    entry.info.transport_type = config.transport_config.type;
    entry.info.is_connected = proxy->isConnected();

    proxy_cache_[cache_key] = std::move(entry);
}

uint32_t ProxyFactory::removeExpiredProxies() {
    LOGD_FMT("ProxyFactory::removeExpiredProxies called");
    auto now = std::chrono::system_clock::now();
    auto idle_timeout = std::chrono::seconds(config_.idle_timeout_seconds);

    uint32_t removed = 0;
    auto it = proxy_cache_.begin();
    while (it != proxy_cache_.end()) {
        // Check if weak reference is expired
        if (it->second.weak_ref.expired()) {
            it = proxy_cache_.erase(it);
            stats_.cached_proxies--;
            removed++;
            continue;
        }

        // Check idle timeout
        auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.info.last_accessed
        );

        if (idle_duration >= idle_timeout) {
            it = proxy_cache_.erase(it);
            stats_.cached_proxies--;
            removed++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        LOGD_FMT("ProxyFactory::removeExpiredProxies: removed " << removed << " expired proxies");
    }

    return removed;
}

bool ProxyFactory::defaultHealthCheck(const std::string& /* service_name */, ServiceProxyPtr proxy) {
    if (!proxy) {
        return false;
    }

    // Simple health check: verify connection status
    bool is_connected = proxy->isConnected();

    // Optional: could perform a ping or echo call here
    // For now, just check connection status

    return is_connected;
}

void ProxyFactory::healthCheckThread() {
    auto interval = std::chrono::seconds(config_.health_check_interval_seconds);

    while (running_) {
        // Sleep in small chunks to allow quick exit
        auto sleep_end = std::chrono::steady_clock::now() + interval;
        while (running_ && std::chrono::steady_clock::now() < sleep_end) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!running_) {
            break;
        }

        // Perform health check on all proxies
        uint32_t unhealthy = checkAllProxiesHealth();

        // Auto-reconnect unhealthy proxies if enabled
        if (config_.enable_auto_reconnect && unhealthy > 0) {
            reconnectAllProxies();
        }
    }
}

void ProxyFactory::cleanupThread() {
    auto interval = std::chrono::seconds(config_.idle_timeout_seconds / 2);

    while (running_) {
        // Sleep in small chunks to allow quick exit
        auto sleep_end = std::chrono::steady_clock::now() + interval;
        while (running_ && std::chrono::steady_clock::now() < sleep_end) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!running_) {
            break;
        }

        // Cleanup idle proxies
        cleanupIdleProxies();
    }
}

void ProxyFactory::updateAccessTime(const std::string& cache_key) {
    auto it = proxy_cache_.find(cache_key);
    if (it != proxy_cache_.end()) {
        it->second.info.last_accessed = std::chrono::system_clock::now();
    }
}

void ProxyFactory::updateAggregatedStats(ServiceProxyPtr proxy) {
    if (!proxy || !config_.enable_statistics) {
        return;
    }

    ProxyStatsSnapshot proxy_stats = proxy->getStats();
    stats_.total_calls += proxy_stats.total_calls;
    stats_.successful_calls += proxy_stats.successful_calls;
    stats_.failed_calls += proxy_stats.failed_calls;
    stats_.timeout_calls += proxy_stats.timeout_calls;
}

// ProxyBuilder implementation

ProxyBuilder::ProxyBuilder() {
    // Initialize with defaults
    config_.default_timeout_ms = 5000;
    config_.auto_reconnect = true;
    config_.serialization_format = SerializationFormat::BINARY;
    config_.transport_config.type = TransportType::UNIX_SOCKET;
    config_.transport_config.mode = TransportMode::SYNC;
    config_.retry_policy.enabled = false;
}

ProxyBuilder& ProxyBuilder::withServiceName(const std::string& name) {
    service_name_ = name;
    config_.service_name = name;
    return *this;
}

ProxyBuilder& ProxyBuilder::withEndpoint(const std::string& endpoint) {
    config_.transport_config.endpoint = endpoint;
    return *this;
}

ProxyBuilder& ProxyBuilder::withTransportType(TransportType type) {
    config_.transport_config.type = type;
    return *this;
}

ProxyBuilder& ProxyBuilder::withTimeout(uint32_t timeout_ms) {
    config_.default_timeout_ms = timeout_ms;
    config_.transport_config.connect_timeout_ms = timeout_ms;
    config_.transport_config.send_timeout_ms = timeout_ms;
    config_.transport_config.recv_timeout_ms = timeout_ms;
    return *this;
}

ProxyBuilder& ProxyBuilder::withRetryPolicy(const RetryPolicy& policy) {
    config_.retry_policy = policy;
    return *this;
}

ProxyBuilder& ProxyBuilder::withAutoReconnect(bool enabled) {
    config_.auto_reconnect = enabled;
    config_.transport_config.auto_reconnect = enabled;
    return *this;
}

ProxyBuilder& ProxyBuilder::withSerializationFormat(SerializationFormat format) {
    config_.serialization_format = format;
    return *this;
}

ProxyBuilder& ProxyBuilder::withTransportConfig(const TransportConfig& config) {
    config_.transport_config = config;
    return *this;
}

ServiceProxyPtr ProxyBuilder::build() {
    return ProxyFactory::getInstance().getProxy(service_name_, config_);
}

ServiceProxyPtr ProxyBuilder::buildAndConnect() {
    ServiceProxyPtr proxy = build();
    if (proxy && !proxy->connect()) {
        return nullptr;
    }
    return proxy;
}

ProxyConfig ProxyBuilder::buildConfig() const {
    return config_;
}

} // namespace ipc
} // namespace cdmf
