/**
 * @file service_proxy_factory.cpp
 * @brief Service proxy factory implementation
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Service Proxy Factory Agent
 */

#include "ipc/service_proxy_factory.h"
#include "ipc/reflection_proxy_generator.h"
#include "utils/log.h"
#include <sstream>
#include <algorithm>
#include <random>
#include <thread>

namespace ipc {
namespace proxy {

// ============================================================================
// TransportInvocationHandler Implementation
// ============================================================================

TransportInvocationHandler::TransportInvocationHandler(
    cdmf::ipc::TransportPtr transport,
    cdmf::ipc::SerializerPtr serializer,
    const ProxyConfig& config)
    : transport_(transport)
    , serializer_(serializer)
    , config_(config) {
    LOGD_FMT("TransportInvocationHandler constructor called");
}

TransportInvocationHandler::~TransportInvocationHandler() {
    LOGD_FMT("TransportInvocationHandler destructor called");
    if (transport_ && transport_->isConnected()) {
        transport_->disconnect();
        LOGD_FMT("TransportInvocationHandler: transport disconnected");
    }
}

InvocationResult TransportInvocationHandler::invoke(const InvocationContext& context) {
    LOGD_FMT("TransportInvocationHandler::invoke called");
    std::lock_guard<std::mutex> lock(mutex_);

    // Create request message
    auto request = createRequestMessage(context);
    if (!request) {
        LOGE_FMT("TransportInvocationHandler::invoke: failed to create request message");
        return handleError("Failed to create request message", context);
    }

    // Send request with retry
    bool sent = retryOperation([&]() {
        auto result = transport_->send(*request);
        if (!result.success()) {
            lastError_ = result.error_message;
            return false;
        }
        return true;
    });

    if (!sent) {
        LOGE_FMT("TransportInvocationHandler::invoke: failed to send request - " << lastError_);
        return handleError("Failed to send request: " + lastError_, context);
    }

    // Receive response with timeout
    uint32_t timeout = context.timeoutMs > 0 ? context.timeoutMs : config_.requestTimeoutMs;
    auto recvResult = transport_->receive(timeout);

    if (!recvResult.success()) {
        if (recvResult.error == cdmf::ipc::TransportError::TIMEOUT) {
            LOGW_FMT("TransportInvocationHandler::invoke: request timeout after " << timeout << "ms");
            InvocationResult result;
            result.success = false;
            result.errorMessage = "Request timeout after " + std::to_string(timeout) + "ms";
            result.exceptionType = "TimeoutException";
            result.errorCode = -1;
            return result;
        }
        LOGE_FMT("TransportInvocationHandler::invoke: failed to receive response - " << recvResult.error_message);
        return handleError("Failed to receive response: " + recvResult.error_message, context);
    }

    LOGD_FMT("TransportInvocationHandler::invoke: request completed successfully");
    return processResponse(recvResult.value, context);
}

std::future<InvocationResult> TransportInvocationHandler::invokeAsync(
    const InvocationContext& context) {
    LOGD_FMT("TransportInvocationHandler::invokeAsync called");

    return std::async(std::launch::async, [this, context]() {
        return invoke(context);
    });
}

void TransportInvocationHandler::invokeOneway(const InvocationContext& context) {
    LOGD_FMT("TransportInvocationHandler::invokeOneway called");
    std::lock_guard<std::mutex> lock(mutex_);

    auto request = createRequestMessage(context);
    if (!request) {
        LOGE_FMT("TransportInvocationHandler::invokeOneway: failed to create request message");
        return;
    }

    // Fire and forget - don't wait for response
    auto result = transport_->send(*request);
    if (!result.success()) {
        LOGW_FMT("TransportInvocationHandler::invokeOneway: send failed - " << result.error_message);
    } else {
        LOGD_FMT("TransportInvocationHandler::invokeOneway: message sent");
    }
}

cdmf::ipc::TransportStats TransportInvocationHandler::getTransportStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_->getStats();
}

bool TransportInvocationHandler::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_->isConnected();
}

bool TransportInvocationHandler::reconnect() {
    LOGD_FMT("TransportInvocationHandler::reconnect called");
    std::lock_guard<std::mutex> lock(mutex_);

    if (transport_->isConnected()) {
        transport_->disconnect();
        LOGD_FMT("TransportInvocationHandler::reconnect: disconnected existing connection");
    }

    auto result = transport_->connect();
    if (result.success()) {
        LOGI_FMT("TransportInvocationHandler::reconnect: reconnection successful");
        return true;
    } else {
        LOGE_FMT("TransportInvocationHandler::reconnect: reconnection failed - " << result.error_message);
        return false;
    }
}

std::string TransportInvocationHandler::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

cdmf::ipc::MessagePtr TransportInvocationHandler::createRequestMessage(
    const InvocationContext& context) {

    auto message = std::make_shared<cdmf::ipc::Message>(cdmf::ipc::MessageType::REQUEST);

    // Set message metadata
    if (context.methodMetadata) {
        message->setSubject(context.methodMetadata->getName());

        // Note: MessageMetadata doesn't have properties map
        // Service and method IDs would be encoded in message payload or subject
        // if (context.serviceMetadata) {
        //     message->getMetadata().properties["service_id"] =
        //         std::to_string(context.serviceMetadata->getServiceId());
        //     message->getMetadata().properties["service_name"] =
        //         context.serviceMetadata->getName();
        // }
        // message->getMetadata().properties["method_id"] =
        //     std::to_string(context.methodMetadata->getMethodId());
    }

    // Serialize arguments (simplified - in production, use proper serialization)
    // For now, we'll just set empty payload
    // TODO: Implement proper argument serialization using TypeRegistry

    message->setFormat(serializer_->getFormat());
    message->updateChecksum();

    return message;
}

InvocationResult TransportInvocationHandler::processResponse(
    cdmf::ipc::MessagePtr response,
    const InvocationContext& /* context */) {

    InvocationResult result;

    if (response->getType() == cdmf::ipc::MessageType::ERROR) {
        result.success = false;
        result.errorMessage = response->getErrorInfo().error_message;
        result.errorCode = response->getErrorInfo().error_code;
        result.exceptionType = "RemoteException";
        return result;
    }

    if (response->getType() != cdmf::ipc::MessageType::RESPONSE) {
        result.success = false;
        result.errorMessage = "Unexpected message type: " +
            std::to_string(static_cast<int>(response->getType()));
        result.errorCode = -1;
        return result;
    }

    // Deserialize return value (simplified)
    // TODO: Implement proper deserialization using TypeRegistry and method metadata
    result.success = true;

    return result;
}

InvocationResult TransportInvocationHandler::handleError(
    const std::string& error,
    const InvocationContext& /* context */) {

    InvocationResult result;
    result.success = false;
    result.errorMessage = error;
    result.exceptionType = "RemoteException";
    result.errorCode = -1;
    return result;
}

bool TransportInvocationHandler::retryOperation(std::function<bool()> operation) {
    if (!config_.enableRetry) {
        return operation();
    }

    for (uint32_t attempt = 0; attempt <= config_.maxRetries; ++attempt) {
        if (operation()) {
            return true;
        }

        if (attempt < config_.maxRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.retryDelayMs));
        }
    }

    return false;
}

// ============================================================================
// ServiceProxyFactory Implementation
// ============================================================================

ServiceProxyFactory::ServiceProxyFactory()
    : config_()
    , running_(false) {
    LOGD_FMT("ServiceProxyFactory default constructor called");
    // Use default reflection proxy generator
    proxyGenerator_ = std::make_shared<ReflectionProxyGenerator>();
}

ServiceProxyFactory::ServiceProxyFactory(const ProxyConfig& config)
    : config_(config)
    , running_(false) {
    LOGD_FMT("ServiceProxyFactory constructor called with config");
    proxyGenerator_ = std::make_shared<ReflectionProxyGenerator>();
}

ServiceProxyFactory::~ServiceProxyFactory() {
    LOGD_FMT("ServiceProxyFactory destructor called");
    shutdown();
}

void ServiceProxyFactory::setConfig(const ProxyConfig& config) {
    LOGD_FMT("ServiceProxyFactory::setConfig called");
    config_ = config;
}

const ProxyConfig& ServiceProxyFactory::getConfig() const {
    return config_;
}

void ServiceProxyFactory::setServiceDiscovery(std::shared_ptr<IServiceDiscovery> discovery) {
    LOGD_FMT("ServiceProxyFactory::setServiceDiscovery called");
    serviceDiscovery_ = discovery;
}

void ServiceProxyFactory::setProxyGenerator(std::shared_ptr<ProxyGenerator> generator) {
    LOGD_FMT("ServiceProxyFactory::setProxyGenerator called");
    proxyGenerator_ = generator;
}

std::shared_ptr<ServiceProxy> ServiceProxyFactory::createProxy(
    const std::string& serviceName,
    const std::string& version) {

    LOGI_FMT("ServiceProxyFactory::createProxy called for service=" << serviceName
             << ", version=" << (version.empty() ? "latest" : version));

    if (!serviceDiscovery_) {
        LOGE_FMT("ServiceProxyFactory::createProxy: service discovery not configured");
        throw std::runtime_error("Service discovery not configured");
    }

    // Discover service endpoints
    auto endpoints = discoverService(serviceName, version);
    if (endpoints.empty()) {
        LOGE_FMT("ServiceProxyFactory::createProxy: service not found - " << serviceName);
        throw std::runtime_error("Service not found: " + serviceName +
            (version.empty() ? "" : " version " + version));
    }

    LOGD_FMT("ServiceProxyFactory::createProxy: discovered " << endpoints.size() << " endpoints");

    // Select best endpoint
    auto selectedEndpoint = selectEndpoint(endpoints);
    if (!selectedEndpoint) {
        LOGE_FMT("ServiceProxyFactory::createProxy: no healthy endpoint available for service=" << serviceName);
        throw std::runtime_error("No healthy endpoint available for service: " + serviceName);
    }

    // Get service metadata
    auto metadata = serviceDiscovery_->getServiceMetadata(serviceName, version);
    if (!metadata) {
        LOGE_FMT("ServiceProxyFactory::createProxy: service metadata not found for service=" << serviceName);
        throw std::runtime_error("Service metadata not found: " + serviceName);
    }

    // Create proxy
    LOGD_FMT("ServiceProxyFactory::createProxy: creating proxy for endpoint=" << selectedEndpoint->endpoint);
    return createProxy(metadata, *selectedEndpoint);
}

std::shared_ptr<ServiceProxy> ServiceProxyFactory::createProxy(
    std::shared_ptr<metadata::ServiceMetadata> metadata,
    const ServiceEndpoint& endpoint) {

    if (!metadata) {
        throw std::invalid_argument("Service metadata is null");
    }

    if (!proxyGenerator_) {
        throw std::runtime_error("Proxy generator not configured");
    }

    // Check cache if enabled
    if (config_.enableCaching) {
        std::string proxyId = generateProxyId(
            metadata->getName(),
            metadata->getVersion(),
            endpoint.endpoint
        );

        auto cached = getCachedProxy(proxyId);
        if (cached) {
            updateStats([](ProxyFactoryStats& stats) {
                stats.cacheHits++;
            });
            return cached;
        }

        updateStats([](ProxyFactoryStats& stats) {
            stats.cacheMisses++;
        });
    }

    // Create invocation handler
    auto handler = createInvocationHandler(endpoint);

    // Generate proxy
    auto proxy = proxyGenerator_->generateProxy(metadata, handler);

    updateStats([](ProxyFactoryStats& stats) {
        stats.proxiesCreated++;
    });

    // Cache if enabled
    if (config_.enableCaching && proxy) {
        std::string proxyId = generateProxyId(
            metadata->getName(),
            metadata->getVersion(),
            endpoint.endpoint
        );

        std::lock_guard<std::mutex> lock(cacheMutex_);

        CachedProxy cached;
        cached.proxy = proxy;
        cached.handler = handler;
        cached.metadata = metadata;
        cached.endpoint = endpoint;
        cached.createdAt = std::chrono::system_clock::now();
        cached.lastAccessedAt = cached.createdAt;
        cached.accessCount = 1;

        proxyCache_[proxyId] = cached;

        updateStats([](ProxyFactoryStats& stats) {
            stats.proxiesCached++;
        });
    }

    return proxy;
}

std::shared_ptr<ServiceProxy> ServiceProxyFactory::createProxy(
    std::shared_ptr<metadata::ServiceMetadata> metadata,
    std::shared_ptr<ProxyInvocationHandler> handler) {

    if (!metadata || !handler) {
        throw std::invalid_argument("Metadata and handler must not be null");
    }

    if (!proxyGenerator_) {
        throw std::runtime_error("Proxy generator not configured");
    }

    auto proxy = proxyGenerator_->generateProxy(metadata, handler);

    updateStats([](ProxyFactoryStats& stats) {
        stats.proxiesCreated++;
    });

    return proxy;
}

std::shared_ptr<ServiceProxy> ServiceProxyFactory::getCachedProxy(const std::string& serviceId) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = proxyCache_.find(serviceId);
    if (it == proxyCache_.end()) {
        return nullptr;
    }

    // Check if expired
    if (isProxyExpired(it->second)) {
        proxyCache_.erase(it);
        updateStats([](ProxyFactoryStats& stats) {
            stats.proxiesEvicted++;
        });
        return nullptr;
    }

    // Update access info
    it->second.lastAccessedAt = std::chrono::system_clock::now();
    it->second.accessCount++;

    return it->second.proxy;
}

void ServiceProxyFactory::clearCache() {
    LOGI_FMT("ServiceProxyFactory::clearCache called");
    std::lock_guard<std::mutex> lock(cacheMutex_);
    size_t count = proxyCache_.size();
    proxyCache_.clear();

    updateStats([count](ProxyFactoryStats& stats) {
        stats.proxiesEvicted += count;
    });

    LOGI_FMT("ServiceProxyFactory::clearCache: cleared " << count << " cached proxies");
}

void ServiceProxyFactory::evictExpired() {
    LOGD_FMT("ServiceProxyFactory::evictExpired called");
    std::lock_guard<std::mutex> lock(cacheMutex_);

    uint32_t evicted_count = 0;
    auto it = proxyCache_.begin();
    while (it != proxyCache_.end()) {
        if (isProxyExpired(it->second)) {
            it = proxyCache_.erase(it);
            updateStats([](ProxyFactoryStats& stats) {
                stats.proxiesEvicted++;
            });
            evicted_count++;
        } else {
            ++it;
        }
    }

    if (evicted_count > 0) {
        LOGD_FMT("ServiceProxyFactory::evictExpired: evicted " << evicted_count << " expired proxies");
    }
}

cdmf::ipc::TransportPtr ServiceProxyFactory::selectTransport(const ServiceEndpoint& endpoint) {
    updateStats([](ProxyFactoryStats& stats) {
        stats.transportSelections++;
    });

    auto transport = cdmf::ipc::TransportFactory::create(endpoint.transportType);
    if (!transport) {
        throw std::runtime_error("Failed to create transport of type: " +
            std::to_string(static_cast<int>(endpoint.transportType)));
    }

    return createAndConfigureTransport(endpoint.transportType, endpoint);
}

cdmf::ipc::SerializerPtr ServiceProxyFactory::selectSerializer(
    cdmf::ipc::SerializationFormat format) {

    updateStats([](ProxyFactoryStats& stats) {
        stats.serializerSelections++;
    });

    return cdmf::ipc::SerializerFactory::createSerializer(format);
}

cdmf::ipc::TransportType ServiceProxyFactory::determineTransportType(
    bool isLocal,
    bool isHighPerformance) {

    if (isLocal) {
        // Local service - choose based on performance requirements
        if (isHighPerformance) {
            return cdmf::ipc::TransportType::SHARED_MEMORY;
        } else {
            return cdmf::ipc::TransportType::UNIX_SOCKET;
        }
    } else {
        // Remote service - use gRPC
        return cdmf::ipc::TransportType::GRPC;
    }
}

cdmf::ipc::SerializationFormat ServiceProxyFactory::determineSerializationFormat(
    bool isLocal,
    cdmf::ipc::TransportType transportType) {

    if (isLocal && transportType == cdmf::ipc::TransportType::SHARED_MEMORY) {
        // Shared memory - use binary for best performance
        return cdmf::ipc::SerializationFormat::BINARY;
    } else if (isLocal) {
        // Local but not shared memory - use binary
        return cdmf::ipc::SerializationFormat::BINARY;
    } else {
        // Remote - use ProtoBuf for cross-platform compatibility
        return cdmf::ipc::SerializationFormat::PROTOBUF;
    }
}

std::vector<ServiceEndpoint> ServiceProxyFactory::discoverService(
    const std::string& serviceName,
    const std::string& version) {

    if (!serviceDiscovery_) {
        return {};
    }

    return serviceDiscovery_->findService(serviceName, version);
}

std::optional<ServiceEndpoint> ServiceProxyFactory::selectEndpoint(
    const std::vector<ServiceEndpoint>& endpoints) {

    if (endpoints.empty()) {
        return std::nullopt;
    }

    // Filter healthy endpoints
    std::vector<ServiceEndpoint> healthyEndpoints;
    for (const auto& ep : endpoints) {
        if (ep.isHealthy) {
            healthyEndpoints.push_back(ep);
        }
    }

    if (healthyEndpoints.empty()) {
        // No healthy endpoints, try first available
        return endpoints[0];
    }

    if (!config_.enableLoadBalancing || healthyEndpoints.size() == 1) {
        return healthyEndpoints[0];
    }

    // Load balancing: weighted random selection based on priority
    uint32_t totalWeight = 0;
    for (const auto& ep : healthyEndpoints) {
        totalWeight += ep.priority;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, totalWeight - 1);

    uint32_t selected = dis(gen);
    uint32_t cumulative = 0;

    for (const auto& ep : healthyEndpoints) {
        cumulative += ep.priority;
        if (selected < cumulative) {
            return ep;
        }
    }

    return healthyEndpoints[0];
}

bool ServiceProxyFactory::checkHealth(const ServiceEndpoint& endpoint) {
    LOGD_FMT("ServiceProxyFactory::checkHealth called for endpoint=" << endpoint.endpoint);

    // Simple health check: try to connect
    auto transport = cdmf::ipc::TransportFactory::create(endpoint.transportType);
    if (!transport) {
        LOGE_FMT("ServiceProxyFactory::checkHealth: failed to create transport");
        return false;
    }

    cdmf::ipc::TransportConfig config;
    config.type = endpoint.transportType;
    config.endpoint = endpoint.endpoint;
    config.connect_timeout_ms = 1000;  // 1 second timeout for health check

    auto initResult = transport->init(config);
    if (!initResult.success()) {
        LOGW_FMT("ServiceProxyFactory::checkHealth: transport init failed - " << initResult.error_message);
        return false;
    }

    auto connectResult = transport->connect();
    bool healthy = connectResult.success();

    if (healthy) {
        transport->disconnect();
        updateStats([](ProxyFactoryStats& stats) {
            stats.healthChecksPassed++;
        });
        LOGD_FMT("ServiceProxyFactory::checkHealth: endpoint=" << endpoint.endpoint << " is healthy");
    } else {
        updateStats([](ProxyFactoryStats& stats) {
            stats.healthChecksFailed++;
        });
        LOGW_FMT("ServiceProxyFactory::checkHealth: endpoint=" << endpoint.endpoint << " is unhealthy - " << connectResult.error_message);
    }

    return healthy;
}

void ServiceProxyFactory::updateHealth(const std::string& serviceId, bool healthy) {
    LOGD_FMT("ServiceProxyFactory::updateHealth called for serviceId=" << serviceId << ", healthy=" << healthy);
    if (serviceDiscovery_) {
        serviceDiscovery_->updateHealth(serviceId, healthy);
    }
}

ProxyFactoryStats ServiceProxyFactory::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void ServiceProxyFactory::resetStats() {
    LOGI_FMT("ServiceProxyFactory::resetStats called");
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = ProxyFactoryStats();
    LOGD_FMT("ServiceProxyFactory::resetStats: statistics reset");
}

size_t ServiceProxyFactory::getProxyCount() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_.proxiesCreated;
}

size_t ServiceProxyFactory::getCachedProxyCount() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return proxyCache_.size();
}

bool ServiceProxyFactory::start() {
    LOGI_FMT("ServiceProxyFactory::start called");
    std::lock_guard<std::mutex> lock(runningMutex_);
    if (running_) {
        LOGW_FMT("ServiceProxyFactory::start: already running");
        return true;
    }

    running_ = true;
    LOGI_FMT("ServiceProxyFactory::start: started successfully");
    return true;
}

bool ServiceProxyFactory::stop() {
    LOGI_FMT("ServiceProxyFactory::stop called");
    std::lock_guard<std::mutex> lock(runningMutex_);
    if (!running_) {
        LOGW_FMT("ServiceProxyFactory::stop: already stopped");
        return true;
    }

    clearCache();
    running_ = false;
    LOGI_FMT("ServiceProxyFactory::stop: stopped successfully");
    return true;
}

void ServiceProxyFactory::shutdown() {
    LOGI_FMT("ServiceProxyFactory::shutdown called");
    stop();
}

bool ServiceProxyFactory::isRunning() const {
    std::lock_guard<std::mutex> lock(runningMutex_);
    return running_;
}

std::string ServiceProxyFactory::generateProxyId(
    const std::string& serviceName,
    const std::string& version,
    const std::string& endpoint) {

    return serviceName + ":" + version + "@" + endpoint;
}

std::shared_ptr<ProxyInvocationHandler> ServiceProxyFactory::createInvocationHandler(
    const ServiceEndpoint& endpoint) {

    auto transport = selectTransport(endpoint);
    auto serializer = selectSerializer(endpoint.serializationFormat);

    if (!transport || !serializer) {
        throw std::runtime_error("Failed to create transport or serializer");
    }

    // Configure and connect transport
    transport = createAndConfigureTransport(endpoint.transportType, endpoint);

    return std::make_shared<TransportInvocationHandler>(transport, serializer, config_);
}

cdmf::ipc::TransportPtr ServiceProxyFactory::createAndConfigureTransport(
    cdmf::ipc::TransportType type,
    const ServiceEndpoint& endpoint) {

    auto transport = cdmf::ipc::TransportFactory::create(type);
    if (!transport) {
        throw std::runtime_error("Failed to create transport");
    }

    cdmf::ipc::TransportConfig config;
    config.type = type;
    config.endpoint = endpoint.endpoint;
    config.connect_timeout_ms = config_.connectTimeoutMs;
    config.send_timeout_ms = config_.requestTimeoutMs;
    config.recv_timeout_ms = config_.requestTimeoutMs;
    config.auto_reconnect = config_.enableRetry;
    config.max_reconnect_attempts = config_.maxRetries;

    // Copy endpoint-specific properties
    for (const auto& [key, value] : endpoint.properties) {
        config.properties[key] = value;
    }

    auto initResult = transport->init(config);
    if (!initResult.success()) {
        throw std::runtime_error("Failed to initialize transport: " + initResult.error_message);
    }

    auto connectResult = transport->connect();
    if (!connectResult.success()) {
        throw std::runtime_error("Failed to connect transport: " + connectResult.error_message);
    }

    return transport;
}

bool ServiceProxyFactory::isProxyExpired(const CachedProxy& cached) const {
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - cached.createdAt).count();

    return age > config_.cacheExpirationMs;
}

void ServiceProxyFactory::updateStats(const std::function<void(ProxyFactoryStats&)>& updater) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    updater(stats_);
    stats_.lastOperationTime = std::chrono::system_clock::now();
}

// ============================================================================
// InMemoryServiceDiscovery Implementation
// ============================================================================

std::vector<ServiceEndpoint> InMemoryServiceDiscovery::findService(
    const std::string& serviceName,
    const std::string& version) {

    LOGD_FMT("InMemoryServiceDiscovery::findService called for service=" << serviceName
             << ", version=" << (version.empty() ? "any" : version));

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServiceEndpoint> results;

    for (const auto& [id, reg] : services_) {
        if (reg.endpoint.serviceName == serviceName) {
            if (version.empty() || reg.endpoint.version == version) {
                results.push_back(reg.endpoint);
            }
        }
    }

    LOGD_FMT("InMemoryServiceDiscovery::findService: found " << results.size() << " endpoints");
    return results;
}

std::shared_ptr<metadata::ServiceMetadata> InMemoryServiceDiscovery::getServiceMetadata(
    const std::string& serviceName,
    const std::string& version) {

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [id, reg] : services_) {
        if (reg.endpoint.serviceName == serviceName) {
            if (version.empty() || reg.endpoint.version == version) {
                return reg.metadata;
            }
        }
    }

    return nullptr;
}

bool InMemoryServiceDiscovery::registerService(
    const ServiceEndpoint& endpoint,
    std::shared_ptr<metadata::ServiceMetadata> metadata) {

    LOGI_FMT("InMemoryServiceDiscovery::registerService called for serviceId=" << endpoint.serviceId);

    std::lock_guard<std::mutex> lock(mutex_);

    ServiceRegistration reg;
    reg.endpoint = endpoint;
    reg.metadata = metadata;

    services_[endpoint.serviceId] = reg;
    LOGD_FMT("InMemoryServiceDiscovery::registerService: registered service, total=" << services_.size());
    return true;
}

bool InMemoryServiceDiscovery::unregisterService(const std::string& serviceId) {
    LOGI_FMT("InMemoryServiceDiscovery::unregisterService called for serviceId=" << serviceId);
    std::lock_guard<std::mutex> lock(mutex_);
    bool result = services_.erase(serviceId) > 0;
    if (result) {
        LOGD_FMT("InMemoryServiceDiscovery::unregisterService: service removed");
    } else {
        LOGW_FMT("InMemoryServiceDiscovery::unregisterService: service not found");
    }
    return result;
}

bool InMemoryServiceDiscovery::updateHealth(const std::string& serviceId, bool healthy) {
    LOGD_FMT("InMemoryServiceDiscovery::updateHealth called for serviceId=" << serviceId << ", healthy=" << healthy);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = services_.find(serviceId);
    if (it == services_.end()) {
        LOGW_FMT("InMemoryServiceDiscovery::updateHealth: service not found");
        return false;
    }

    it->second.endpoint.isHealthy = healthy;
    it->second.endpoint.lastHealthCheck = std::chrono::system_clock::now();
    LOGD_FMT("InMemoryServiceDiscovery::updateHealth: health updated");
    return true;
}

std::vector<ServiceEndpoint> InMemoryServiceDiscovery::getAllServices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServiceEndpoint> results;

    for (const auto& [id, reg] : services_) {
        results.push_back(reg.endpoint);
    }

    return results;
}

void InMemoryServiceDiscovery::clear() {
    LOGI_FMT("InMemoryServiceDiscovery::clear called");
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = services_.size();
    services_.clear();
    LOGD_FMT("InMemoryServiceDiscovery::clear: cleared " << count << " services");
}

// ============================================================================
// CircuitBreaker Implementation
// ============================================================================

CircuitBreaker::CircuitBreaker(uint32_t threshold, uint32_t timeoutMs)
    : state_(State::CLOSED)
    , failureCount_(0)
    , threshold_(threshold)
    , timeoutMs_(timeoutMs) {
    LOGD_FMT("CircuitBreaker created with threshold=" << threshold << ", timeout=" << timeoutMs << "ms");
}

bool CircuitBreaker::isCallAllowed() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == State::CLOSED) {
        return true;
    }

    if (state_ == State::OPEN) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastFailureTime_).count();

        if (elapsed >= timeoutMs_) {
            transitionToHalfOpen();
            return true;
        }
        return false;
    }

    // HALF_OPEN state - allow one test call
    return true;
}

void CircuitBreaker::recordSuccess() {
    LOGD_FMT("CircuitBreaker::recordSuccess called");
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == State::HALF_OPEN) {
        LOGI_FMT("CircuitBreaker: transitioning from HALF_OPEN to CLOSED after success");
        transitionToClosed();
    }

    failureCount_ = 0;
}

void CircuitBreaker::recordFailure() {
    LOGD_FMT("CircuitBreaker::recordFailure called");
    std::lock_guard<std::mutex> lock(mutex_);

    failureCount_++;
    lastFailureTime_ = std::chrono::system_clock::now();

    if (state_ == State::HALF_OPEN) {
        LOGW_FMT("CircuitBreaker: transitioning from HALF_OPEN to OPEN after failure");
        transitionToOpen();
    } else if (state_ == State::CLOSED && failureCount_ >= threshold_) {
        LOGW_FMT("CircuitBreaker: transitioning from CLOSED to OPEN, failures=" << failureCount_);
        transitionToOpen();
    }
}

CircuitBreaker::State CircuitBreaker::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void CircuitBreaker::reset() {
    LOGI_FMT("CircuitBreaker::reset called");
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::CLOSED;
    failureCount_ = 0;
    LOGD_FMT("CircuitBreaker: reset to CLOSED state");
}

void CircuitBreaker::transitionToOpen() {
    state_ = State::OPEN;
}

void CircuitBreaker::transitionToHalfOpen() {
    state_ = State::HALF_OPEN;
}

void CircuitBreaker::transitionToClosed() {
    state_ = State::CLOSED;
    failureCount_ = 0;
}

} // namespace proxy
} // namespace ipc
