/**
 * @file service_proxy_factory.h
 * @brief Service proxy factory for managing service proxy lifecycle
 *
 * Provides a comprehensive factory for creating, caching, and managing service proxies
 * with integrated transport selection, serialization, service discovery, and health checking.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Service Proxy Factory Agent
 */

#ifndef IPC_SERVICE_PROXY_FACTORY_H
#define IPC_SERVICE_PROXY_FACTORY_H

#include "ipc/proxy_generator.h"
#include "ipc/metadata.h"
#include "ipc/transport.h"
#include "ipc/serializer.h"
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <optional>

namespace ipc {
namespace proxy {

// Forward declarations
class ProxyInvocationHandler;
class TransportInvocationHandler;
class ServiceProxyFactory;

/**
 * @brief Service endpoint information
 */
struct ServiceEndpoint {
    std::string serviceId;
    std::string serviceName;
    std::string version;
    std::string endpoint;           // Transport endpoint (e.g., "/tmp/socket", "localhost:50051")
    cdmf::ipc::TransportType transportType;
    cdmf::ipc::SerializationFormat serializationFormat;
    bool isLocal;                   // Same machine vs remote
    bool isHealthy;                 // Health status
    uint32_t priority;              // For load balancing
    std::chrono::system_clock::time_point lastHealthCheck;
    std::map<std::string, std::string> properties;

    ServiceEndpoint()
        : transportType(cdmf::ipc::TransportType::UNKNOWN)
        , serializationFormat(cdmf::ipc::SerializationFormat::BINARY)
        , isLocal(true)
        , isHealthy(true)
        , priority(100) {
    }
};

/**
 * @brief Proxy configuration
 */
struct ProxyConfig {
    uint32_t connectTimeoutMs = 5000;
    uint32_t requestTimeoutMs = 30000;
    uint32_t healthCheckIntervalMs = 30000;
    bool enableRetry = true;
    uint32_t maxRetries = 3;
    uint32_t retryDelayMs = 1000;
    bool enableCircuitBreaker = true;
    uint32_t circuitBreakerThreshold = 5;
    uint32_t circuitBreakerTimeoutMs = 60000;
    bool enableLoadBalancing = true;
    bool enableCaching = true;
    uint32_t cacheExpirationMs = 300000;  // 5 minutes
    std::map<std::string, std::string> properties;

    ProxyConfig() = default;
};

/**
 * @brief Proxy factory statistics
 */
struct ProxyFactoryStats {
    uint64_t proxiesCreated = 0;
    uint64_t proxiesCached = 0;
    uint64_t proxiesEvicted = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t healthChecksPassed = 0;
    uint64_t healthChecksFailed = 0;
    uint64_t transportSelections = 0;
    uint64_t serializerSelections = 0;
    std::chrono::system_clock::time_point lastOperationTime;

    ProxyFactoryStats() = default;
};

/**
 * @brief Service discovery interface
 */
class IServiceDiscovery {
public:
    virtual ~IServiceDiscovery() = default;

    /**
     * @brief Find service endpoints by name and version
     */
    virtual std::vector<ServiceEndpoint> findService(
        const std::string& serviceName,
        const std::string& version = "") = 0;

    /**
     * @brief Get service metadata by name and version
     */
    virtual std::shared_ptr<metadata::ServiceMetadata> getServiceMetadata(
        const std::string& serviceName,
        const std::string& version = "") = 0;

    /**
     * @brief Register service endpoint
     */
    virtual bool registerService(const ServiceEndpoint& endpoint,
                                 std::shared_ptr<metadata::ServiceMetadata> metadata) = 0;

    /**
     * @brief Unregister service endpoint
     */
    virtual bool unregisterService(const std::string& serviceId) = 0;

    /**
     * @brief Update service health status
     */
    virtual bool updateHealth(const std::string& serviceId, bool healthy) = 0;
};

/**
 * @brief Transport-based invocation handler
 *
 * Implements ProxyInvocationHandler using transport layer for actual communication.
 * Handles serialization, transport operations, and error handling.
 */
class TransportInvocationHandler : public ProxyInvocationHandler {
public:
    TransportInvocationHandler(
        cdmf::ipc::TransportPtr transport,
        cdmf::ipc::SerializerPtr serializer,
        const ProxyConfig& config);

    ~TransportInvocationHandler() override;

    InvocationResult invoke(const InvocationContext& context) override;
    std::future<InvocationResult> invokeAsync(const InvocationContext& context) override;
    void invokeOneway(const InvocationContext& context) override;

    /**
     * @brief Get transport statistics
     */
    cdmf::ipc::TransportStats getTransportStats() const;

    /**
     * @brief Check if transport is connected
     */
    bool isConnected() const;

    /**
     * @brief Reconnect transport
     */
    bool reconnect();

    /**
     * @brief Get last error
     */
    std::string getLastError() const;

private:
    cdmf::ipc::TransportPtr transport_;
    cdmf::ipc::SerializerPtr serializer_;
    ProxyConfig config_;
    mutable std::mutex mutex_;
    std::string lastError_;

    cdmf::ipc::MessagePtr createRequestMessage(const InvocationContext& context);
    InvocationResult processResponse(cdmf::ipc::MessagePtr response,
                                     const InvocationContext& context);
    InvocationResult handleError(const std::string& error,
                                 const InvocationContext& context);
    bool retryOperation(std::function<bool()> operation);
};

/**
 * @brief Service proxy factory
 *
 * Central factory for creating and managing service proxies with:
 * - Metadata-based proxy generation
 * - Automatic transport selection
 * - Serialization format negotiation
 * - Service discovery integration
 * - Proxy caching and lifecycle management
 * - Health checking
 * - Load balancing
 */
class ServiceProxyFactory {
public:
    ServiceProxyFactory();
    explicit ServiceProxyFactory(const ProxyConfig& config);
    ~ServiceProxyFactory();

    // Factory configuration

    /**
     * @brief Set proxy configuration
     */
    void setConfig(const ProxyConfig& config);

    /**
     * @brief Get current configuration
     */
    const ProxyConfig& getConfig() const;

    /**
     * @brief Set service discovery provider
     */
    void setServiceDiscovery(std::shared_ptr<IServiceDiscovery> discovery);

    /**
     * @brief Set proxy generator
     */
    void setProxyGenerator(std::shared_ptr<ProxyGenerator> generator);

    // Proxy creation

    /**
     * @brief Create proxy from service name (uses service discovery)
     * @param serviceName Service name
     * @param version Service version (empty for latest)
     * @return Shared pointer to service proxy
     */
    std::shared_ptr<ServiceProxy> createProxy(
        const std::string& serviceName,
        const std::string& version = "");

    /**
     * @brief Create proxy from service metadata
     * @param metadata Service metadata
     * @param endpoint Service endpoint
     * @return Shared pointer to service proxy
     */
    std::shared_ptr<ServiceProxy> createProxy(
        std::shared_ptr<metadata::ServiceMetadata> metadata,
        const ServiceEndpoint& endpoint);

    /**
     * @brief Create proxy with custom handler
     * @param metadata Service metadata
     * @param handler Custom invocation handler
     * @return Shared pointer to service proxy
     */
    std::shared_ptr<ServiceProxy> createProxy(
        std::shared_ptr<metadata::ServiceMetadata> metadata,
        std::shared_ptr<ProxyInvocationHandler> handler);

    /**
     * @brief Get cached proxy (if available)
     * @param serviceId Service identifier
     * @return Shared pointer to cached proxy, or nullptr if not found
     */
    std::shared_ptr<ServiceProxy> getCachedProxy(const std::string& serviceId);

    /**
     * @brief Clear proxy cache
     */
    void clearCache();

    /**
     * @brief Evict expired proxies from cache
     */
    void evictExpired();

    // Transport and serializer selection

    /**
     * @brief Select transport based on endpoint
     * @param endpoint Service endpoint
     * @return Transport instance
     */
    cdmf::ipc::TransportPtr selectTransport(const ServiceEndpoint& endpoint);

    /**
     * @brief Select serializer based on format
     * @param format Serialization format
     * @return Serializer instance
     */
    cdmf::ipc::SerializerPtr selectSerializer(cdmf::ipc::SerializationFormat format);

    /**
     * @brief Determine optimal transport for service
     * @param isLocal Is service on same machine
     * @param isHighPerformance Requires high performance
     * @return Recommended transport type
     */
    cdmf::ipc::TransportType determineTransportType(bool isLocal, bool isHighPerformance = false);

    /**
     * @brief Determine optimal serialization format
     * @param isLocal Is service on same machine
     * @param transportType Transport being used
     * @return Recommended serialization format
     */
    cdmf::ipc::SerializationFormat determineSerializationFormat(
        bool isLocal,
        cdmf::ipc::TransportType transportType);

    // Service discovery and health checking

    /**
     * @brief Discover service endpoints
     * @param serviceName Service name
     * @param version Service version
     * @return Vector of service endpoints
     */
    std::vector<ServiceEndpoint> discoverService(
        const std::string& serviceName,
        const std::string& version = "");

    /**
     * @brief Select best endpoint based on health and load balancing
     * @param endpoints Available endpoints
     * @return Selected endpoint
     */
    std::optional<ServiceEndpoint> selectEndpoint(const std::vector<ServiceEndpoint>& endpoints);

    /**
     * @brief Check health of service endpoint
     * @param endpoint Service endpoint
     * @return true if healthy, false otherwise
     */
    bool checkHealth(const ServiceEndpoint& endpoint);

    /**
     * @brief Update health status for endpoint
     * @param serviceId Service identifier
     * @param healthy Health status
     */
    void updateHealth(const std::string& serviceId, bool healthy);

    // Statistics and monitoring

    /**
     * @brief Get factory statistics
     */
    ProxyFactoryStats getStats() const;

    /**
     * @brief Reset statistics
     */
    void resetStats();

    /**
     * @brief Get proxy count
     */
    size_t getProxyCount() const;

    /**
     * @brief Get cached proxy count
     */
    size_t getCachedProxyCount() const;

    // Lifecycle management

    /**
     * @brief Start factory (initialize resources)
     */
    bool start();

    /**
     * @brief Stop factory (cleanup resources)
     */
    bool stop();

    /**
     * @brief Shutdown factory gracefully
     */
    void shutdown();

    /**
     * @brief Check if factory is running
     */
    bool isRunning() const;

private:
    struct CachedProxy {
        std::shared_ptr<ServiceProxy> proxy;
        std::shared_ptr<ProxyInvocationHandler> handler;
        std::shared_ptr<metadata::ServiceMetadata> metadata;
        ServiceEndpoint endpoint;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point lastAccessedAt;
        uint64_t accessCount;
    };

    ProxyConfig config_;
    std::shared_ptr<IServiceDiscovery> serviceDiscovery_;
    std::shared_ptr<ProxyGenerator> proxyGenerator_;

    std::map<std::string, CachedProxy> proxyCache_;
    mutable std::mutex cacheMutex_;

    ProxyFactoryStats stats_;
    mutable std::mutex statsMutex_;

    bool running_;
    mutable std::mutex runningMutex_;

    // Helper methods
    std::string generateProxyId(const std::string& serviceName,
                               const std::string& version,
                               const std::string& endpoint);

    std::shared_ptr<ProxyInvocationHandler> createInvocationHandler(
        const ServiceEndpoint& endpoint);

    cdmf::ipc::TransportPtr createAndConfigureTransport(
        cdmf::ipc::TransportType type,
        const ServiceEndpoint& endpoint);

    bool isProxyExpired(const CachedProxy& cached) const;

    void updateStats(const std::function<void(ProxyFactoryStats&)>& updater);
};

/**
 * @brief Simple in-memory service discovery implementation
 */
class InMemoryServiceDiscovery : public IServiceDiscovery {
public:
    InMemoryServiceDiscovery() = default;
    ~InMemoryServiceDiscovery() override = default;

    std::vector<ServiceEndpoint> findService(
        const std::string& serviceName,
        const std::string& version = "") override;

    std::shared_ptr<metadata::ServiceMetadata> getServiceMetadata(
        const std::string& serviceName,
        const std::string& version = "") override;

    bool registerService(const ServiceEndpoint& endpoint,
                        std::shared_ptr<metadata::ServiceMetadata> metadata) override;

    bool unregisterService(const std::string& serviceId) override;

    bool updateHealth(const std::string& serviceId, bool healthy) override;

    /**
     * @brief Get all registered services
     */
    std::vector<ServiceEndpoint> getAllServices() const;

    /**
     * @brief Clear all registrations
     */
    void clear();

private:
    struct ServiceRegistration {
        ServiceEndpoint endpoint;
        std::shared_ptr<metadata::ServiceMetadata> metadata;
    };

    std::map<std::string, ServiceRegistration> services_;
    mutable std::mutex mutex_;
};

/**
 * @brief Circuit breaker for fault tolerance
 */
class CircuitBreaker {
public:
    enum class State {
        CLOSED,      // Normal operation
        OPEN,        // Failing, reject calls
        HALF_OPEN    // Testing if service recovered
    };

    CircuitBreaker(uint32_t threshold, uint32_t timeoutMs);

    /**
     * @brief Check if call is allowed
     */
    bool isCallAllowed();

    /**
     * @brief Record successful call
     */
    void recordSuccess();

    /**
     * @brief Record failed call
     */
    void recordFailure();

    /**
     * @brief Get current state
     */
    State getState() const;

    /**
     * @brief Reset circuit breaker
     */
    void reset();

private:
    State state_;
    uint32_t failureCount_;
    uint32_t threshold_;
    uint32_t timeoutMs_;
    std::chrono::system_clock::time_point lastFailureTime_;
    mutable std::mutex mutex_;

    void transitionToOpen();
    void transitionToHalfOpen();
    void transitionToClosed();
};

} // namespace proxy
} // namespace ipc

#endif // IPC_SERVICE_PROXY_FACTORY_H
