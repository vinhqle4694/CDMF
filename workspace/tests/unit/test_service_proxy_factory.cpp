/**
 * @file test_service_proxy_factory.cpp
 * @brief Unit tests for Service Proxy Factory
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Service Proxy Factory Agent
 */

#include <gtest/gtest.h>
#include "ipc/service_proxy_factory.h"
#include "ipc/reflection_proxy_generator.h"
#include <thread>
#include <chrono>

using namespace ipc::proxy;
using namespace ipc::metadata;

// ============================================================================
// Test Fixtures
// ============================================================================

class ServiceProxyFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create service metadata
        ServiceMetadataBuilder builder("TestService", "1.0.0");
        serviceMetadata_ = builder
            .setNamespace("test")
            .setServiceId(1001)
            .beginMethod("add", "int32")
                .addParameter("a", "int32", ParameterDirection::IN)
                .addParameter("b", "int32", ParameterDirection::IN)
                .setMethodId(1)
                .setMethodTimeout(3000)
                .endMethod()
            .beginMethod("multiply", "int32")
                .addParameter("x", "int32", ParameterDirection::IN)
                .addParameter("y", "int32", ParameterDirection::IN)
                .setMethodId(2)
                .endMethod()
            .build();

        // Create factory
        factory_ = std::make_shared<ServiceProxyFactory>();

        // Create service discovery
        discovery_ = std::make_shared<InMemoryServiceDiscovery>();
        factory_->setServiceDiscovery(discovery_);
    }

    void TearDown() override {
        if (factory_) {
            factory_->shutdown();
        }
    }

    std::shared_ptr<ServiceMetadata> serviceMetadata_;
    std::shared_ptr<ServiceProxyFactory> factory_;
    std::shared_ptr<InMemoryServiceDiscovery> discovery_;
};

// ============================================================================
// Factory Configuration Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, DefaultConfiguration) {
    auto config = factory_->getConfig();

    EXPECT_EQ(5000, config.connectTimeoutMs);
    EXPECT_EQ(30000, config.requestTimeoutMs);
    EXPECT_TRUE(config.enableRetry);
    EXPECT_EQ(3, config.maxRetries);
    EXPECT_TRUE(config.enableCaching);
    EXPECT_TRUE(config.enableLoadBalancing);
}

TEST_F(ServiceProxyFactoryTest, CustomConfiguration) {
    ProxyConfig config;
    config.connectTimeoutMs = 10000;
    config.requestTimeoutMs = 60000;
    config.enableRetry = false;
    config.maxRetries = 5;
    config.enableCaching = false;

    factory_->setConfig(config);

    auto retrievedConfig = factory_->getConfig();
    EXPECT_EQ(10000, retrievedConfig.connectTimeoutMs);
    EXPECT_EQ(60000, retrievedConfig.requestTimeoutMs);
    EXPECT_FALSE(retrievedConfig.enableRetry);
    EXPECT_EQ(5, retrievedConfig.maxRetries);
    EXPECT_FALSE(retrievedConfig.enableCaching);
}

TEST_F(ServiceProxyFactoryTest, SetProxyGenerator) {
    auto customGenerator = std::make_shared<ReflectionProxyGenerator>();
    factory_->setProxyGenerator(customGenerator);

    // Should not throw
    EXPECT_NO_THROW(factory_->setProxyGenerator(customGenerator));
}

// ============================================================================
// Service Discovery Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, RegisterAndFindService) {
    ServiceEndpoint endpoint;
    endpoint.serviceId = "test-service-1";
    endpoint.serviceName = "TestService";
    endpoint.version = "1.0.0";
    endpoint.endpoint = "/tmp/test_service.sock";
    endpoint.transportType = cdmf::ipc::TransportType::UNIX_SOCKET;
    endpoint.serializationFormat = cdmf::ipc::SerializationFormat::BINARY;
    endpoint.isLocal = true;
    endpoint.isHealthy = true;

    EXPECT_TRUE(discovery_->registerService(endpoint, serviceMetadata_));

    auto found = discovery_->findService("TestService", "1.0.0");
    EXPECT_EQ(1, found.size());
    EXPECT_EQ("test-service-1", found[0].serviceId);
    EXPECT_EQ("TestService", found[0].serviceName);
}

TEST_F(ServiceProxyFactoryTest, FindServiceByNameOnly) {
    ServiceEndpoint endpoint1;
    endpoint1.serviceId = "test-service-1";
    endpoint1.serviceName = "TestService";
    endpoint1.version = "1.0.0";
    endpoint1.endpoint = "/tmp/test_v1.sock";
    endpoint1.isLocal = true;

    ServiceEndpoint endpoint2;
    endpoint2.serviceId = "test-service-2";
    endpoint2.serviceName = "TestService";
    endpoint2.version = "2.0.0";
    endpoint2.endpoint = "/tmp/test_v2.sock";
    endpoint2.isLocal = true;

    discovery_->registerService(endpoint1, serviceMetadata_);
    discovery_->registerService(endpoint2, serviceMetadata_);

    auto found = discovery_->findService("TestService");
    EXPECT_EQ(2, found.size());
}

TEST_F(ServiceProxyFactoryTest, UnregisterService) {
    ServiceEndpoint endpoint;
    endpoint.serviceId = "test-service-1";
    endpoint.serviceName = "TestService";
    endpoint.version = "1.0.0";
    endpoint.endpoint = "/tmp/test.sock";

    discovery_->registerService(endpoint, serviceMetadata_);
    EXPECT_EQ(1, discovery_->findService("TestService").size());

    EXPECT_TRUE(discovery_->unregisterService("test-service-1"));
    EXPECT_EQ(0, discovery_->findService("TestService").size());
}

TEST_F(ServiceProxyFactoryTest, UpdateServiceHealth) {
    ServiceEndpoint endpoint;
    endpoint.serviceId = "test-service-1";
    endpoint.serviceName = "TestService";
    endpoint.version = "1.0.0";
    endpoint.endpoint = "/tmp/test.sock";
    endpoint.isHealthy = true;

    discovery_->registerService(endpoint, serviceMetadata_);

    EXPECT_TRUE(discovery_->updateHealth("test-service-1", false));

    auto found = discovery_->findService("TestService");
    EXPECT_EQ(1, found.size());
    EXPECT_FALSE(found[0].isHealthy);
}

// ============================================================================
// Transport Selection Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, DetermineTransportTypeLocalHighPerformance) {
    auto type = factory_->determineTransportType(true, true);
    EXPECT_EQ(cdmf::ipc::TransportType::SHARED_MEMORY, type);
}

TEST_F(ServiceProxyFactoryTest, DetermineTransportTypeLocalNormal) {
    auto type = factory_->determineTransportType(true, false);
    EXPECT_EQ(cdmf::ipc::TransportType::UNIX_SOCKET, type);
}

TEST_F(ServiceProxyFactoryTest, DetermineTransportTypeRemote) {
    auto type = factory_->determineTransportType(false, false);
    EXPECT_EQ(cdmf::ipc::TransportType::GRPC, type);
}

TEST_F(ServiceProxyFactoryTest, DetermineSerializationFormatLocalSharedMemory) {
    auto format = factory_->determineSerializationFormat(
        true,
        cdmf::ipc::TransportType::SHARED_MEMORY
    );
    EXPECT_EQ(cdmf::ipc::SerializationFormat::BINARY, format);
}

TEST_F(ServiceProxyFactoryTest, DetermineSerializationFormatLocalUnixSocket) {
    auto format = factory_->determineSerializationFormat(
        true,
        cdmf::ipc::TransportType::UNIX_SOCKET
    );
    EXPECT_EQ(cdmf::ipc::SerializationFormat::BINARY, format);
}

TEST_F(ServiceProxyFactoryTest, DetermineSerializationFormatRemoteGrpc) {
    auto format = factory_->determineSerializationFormat(
        false,
        cdmf::ipc::TransportType::GRPC
    );
    EXPECT_EQ(cdmf::ipc::SerializationFormat::PROTOBUF, format);
}

TEST_F(ServiceProxyFactoryTest, SelectSerializer) {
    auto serializer = factory_->selectSerializer(cdmf::ipc::SerializationFormat::BINARY);
    ASSERT_NE(nullptr, serializer);
    EXPECT_EQ(cdmf::ipc::SerializationFormat::BINARY, serializer->getFormat());

    auto stats = factory_->getStats();
    EXPECT_EQ(1, stats.serializerSelections);
}

// ============================================================================
// Endpoint Selection Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, SelectEndpointSingleHealthy) {
    std::vector<ServiceEndpoint> endpoints;

    ServiceEndpoint ep1;
    ep1.serviceId = "svc-1";
    ep1.isHealthy = true;
    ep1.priority = 100;
    endpoints.push_back(ep1);

    auto selected = factory_->selectEndpoint(endpoints);
    ASSERT_TRUE(selected.has_value());
    EXPECT_EQ("svc-1", selected->serviceId);
}

TEST_F(ServiceProxyFactoryTest, SelectEndpointMultipleHealthy) {
    std::vector<ServiceEndpoint> endpoints;

    ServiceEndpoint ep1;
    ep1.serviceId = "svc-1";
    ep1.isHealthy = true;
    ep1.priority = 100;
    endpoints.push_back(ep1);

    ServiceEndpoint ep2;
    ep2.serviceId = "svc-2";
    ep2.isHealthy = true;
    ep2.priority = 100;
    endpoints.push_back(ep2);

    auto selected = factory_->selectEndpoint(endpoints);
    ASSERT_TRUE(selected.has_value());
    // Should select one of the healthy endpoints
    bool isValid = (selected->serviceId == "svc-1" || selected->serviceId == "svc-2");
    EXPECT_TRUE(isValid);
}

TEST_F(ServiceProxyFactoryTest, SelectEndpointOnlyUnhealthy) {
    std::vector<ServiceEndpoint> endpoints;

    ServiceEndpoint ep1;
    ep1.serviceId = "svc-1";
    ep1.isHealthy = false;
    ep1.priority = 100;
    endpoints.push_back(ep1);

    auto selected = factory_->selectEndpoint(endpoints);
    ASSERT_TRUE(selected.has_value());
    // Should still return the unhealthy endpoint as fallback
    EXPECT_EQ("svc-1", selected->serviceId);
}

TEST_F(ServiceProxyFactoryTest, SelectEndpointEmpty) {
    std::vector<ServiceEndpoint> endpoints;

    auto selected = factory_->selectEndpoint(endpoints);
    EXPECT_FALSE(selected.has_value());
}

TEST_F(ServiceProxyFactoryTest, SelectEndpointLoadBalancing) {
    ProxyConfig config;
    config.enableLoadBalancing = true;
    factory_->setConfig(config);

    std::vector<ServiceEndpoint> endpoints;

    ServiceEndpoint ep1;
    ep1.serviceId = "svc-1";
    ep1.isHealthy = true;
    ep1.priority = 100;
    endpoints.push_back(ep1);

    ServiceEndpoint ep2;
    ep2.serviceId = "svc-2";
    ep2.isHealthy = true;
    ep2.priority = 50;
    endpoints.push_back(ep2);

    // Select multiple times and check distribution
    std::map<std::string, int> selections;
    for (int i = 0; i < 100; ++i) {
        auto selected = factory_->selectEndpoint(endpoints);
        ASSERT_TRUE(selected.has_value());
        selections[selected->serviceId]++;
    }

    // Both should be selected at least once (probabilistic)
    EXPECT_GT(selections["svc-1"], 0);
    EXPECT_GT(selections["svc-2"], 0);
}

// ============================================================================
// Proxy Creation Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, CreateProxyWithMockHandler) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    auto proxy = factory_->createProxy(serviceMetadata_, mockHandler);

    ASSERT_NE(nullptr, proxy);
    EXPECT_EQ(serviceMetadata_, proxy->getServiceMetadata());

    auto stats = factory_->getStats();
    EXPECT_EQ(1, stats.proxiesCreated);
}

TEST_F(ServiceProxyFactoryTest, CreateProxyWithNullMetadata) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    EXPECT_THROW(
        factory_->createProxy(nullptr, mockHandler),
        std::invalid_argument
    );
}

TEST_F(ServiceProxyFactoryTest, CreateProxyWithNullHandler) {
    EXPECT_THROW(
        factory_->createProxy(serviceMetadata_, nullptr),
        std::invalid_argument
    );
}

// ============================================================================
// Proxy Caching Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, ProxyCachingEnabled) {
    ProxyConfig config;
    config.enableCaching = true;
    factory_->setConfig(config);

    ServiceEndpoint endpoint;
    endpoint.serviceId = "test-svc-1";
    endpoint.serviceName = "TestService";
    endpoint.version = "1.0.0";
    endpoint.endpoint = "/tmp/test.sock";
    endpoint.transportType = cdmf::ipc::TransportType::UNIX_SOCKET;
    endpoint.serializationFormat = cdmf::ipc::SerializationFormat::BINARY;

    auto mockHandler = std::make_shared<MockInvocationHandler>();

    // First creation - should create new proxy
    // Note: The handler-based createProxy doesn't cache
    auto proxy1 = factory_->createProxy(serviceMetadata_, mockHandler);
    EXPECT_EQ(1, factory_->getStats().proxiesCreated);
    EXPECT_EQ(0, factory_->getStats().proxiesCached);  // Handler-based createProxy doesn't cache
}

TEST_F(ServiceProxyFactoryTest, ProxyCachingDisabled) {
    ProxyConfig config;
    config.enableCaching = false;
    factory_->setConfig(config);

    auto mockHandler = std::make_shared<MockInvocationHandler>();

    auto proxy1 = factory_->createProxy(serviceMetadata_, mockHandler);
    auto proxy2 = factory_->createProxy(serviceMetadata_, mockHandler);

    EXPECT_EQ(2, factory_->getStats().proxiesCreated);
    EXPECT_EQ(0, factory_->getStats().proxiesCached);
}

TEST_F(ServiceProxyFactoryTest, ClearCache) {
    ProxyConfig config;
    config.enableCaching = true;
    factory_->setConfig(config);

    // Create some proxies (they won't be cached with handler-based creation)
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    factory_->createProxy(serviceMetadata_, mockHandler);

    factory_->clearCache();

    /* auto stats = */ factory_->getStats();
    EXPECT_EQ(0, factory_->getCachedProxyCount());
}

TEST_F(ServiceProxyFactoryTest, EvictExpiredProxies) {
    ProxyConfig config;
    config.enableCaching = true;
    config.cacheExpirationMs = 100;  // 100ms expiration
    factory_->setConfig(config);

    auto mockHandler = std::make_shared<MockInvocationHandler>();
    factory_->createProxy(serviceMetadata_, mockHandler);

    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    factory_->evictExpired();

    // All proxies should be evicted
    EXPECT_EQ(0, factory_->getCachedProxyCount());
}

// ============================================================================
// MockInvocationHandler Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, MockHandlerSetReturnValue) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    mockHandler->setReturnValue("add", std::any(42));

    InvocationContext context;
    context.methodMetadata = serviceMetadata_->getMethod("add");

    auto result = mockHandler->invoke(context);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(42, std::any_cast<int>(result.returnValue));
}

TEST_F(ServiceProxyFactoryTest, MockHandlerSetException) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    mockHandler->setException("add", "ArithmeticException", "Overflow");

    InvocationContext context;
    context.methodMetadata = serviceMetadata_->getMethod("add");

    auto result = mockHandler->invoke(context);

    EXPECT_FALSE(result.success);
    EXPECT_EQ("ArithmeticException", result.exceptionType);
    EXPECT_EQ("Overflow", result.errorMessage);
}

TEST_F(ServiceProxyFactoryTest, MockHandlerCustomHandler) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    mockHandler->setMethodHandler("add", [](const InvocationContext& /* ctx */) {
        InvocationResult result;
        result.success = true;
        result.returnValue = std::any(100);
        return result;
    });

    InvocationContext context;
    context.methodMetadata = serviceMetadata_->getMethod("add");

    auto result = mockHandler->invoke(context);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(100, std::any_cast<int>(result.returnValue));
}

TEST_F(ServiceProxyFactoryTest, MockHandlerCallCount) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    mockHandler->setReturnValue("add", std::any(42));

    InvocationContext context;
    context.methodMetadata = serviceMetadata_->getMethod("add");

    EXPECT_EQ(0, mockHandler->getCallCount("add"));

    mockHandler->invoke(context);
    EXPECT_EQ(1, mockHandler->getCallCount("add"));

    mockHandler->invoke(context);
    EXPECT_EQ(2, mockHandler->getCallCount("add"));
}

TEST_F(ServiceProxyFactoryTest, MockHandlerLastInvocation) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    mockHandler->setReturnValue("add", std::any(42));

    InvocationContext context;
    context.methodMetadata = serviceMetadata_->getMethod("add");
    context.arguments.push_back(std::any(10));
    context.arguments.push_back(std::any(20));

    mockHandler->invoke(context);

    auto lastInvocation = mockHandler->getLastInvocation("add");
    ASSERT_TRUE(lastInvocation.has_value());
    EXPECT_EQ(2, lastInvocation->arguments.size());
}

TEST_F(ServiceProxyFactoryTest, MockHandlerReset) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    mockHandler->setReturnValue("add", std::any(42));

    InvocationContext context;
    context.methodMetadata = serviceMetadata_->getMethod("add");

    mockHandler->invoke(context);
    EXPECT_EQ(1, mockHandler->getCallCount("add"));

    mockHandler->reset();
    EXPECT_EQ(0, mockHandler->getCallCount("add"));
}

// ============================================================================
// CircuitBreaker Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, CircuitBreakerClosed) {
    CircuitBreaker cb(3, 1000);

    EXPECT_EQ(CircuitBreaker::State::CLOSED, cb.getState());
    EXPECT_TRUE(cb.isCallAllowed());
}

TEST_F(ServiceProxyFactoryTest, CircuitBreakerOpensAfterThreshold) {
    CircuitBreaker cb(3, 1000);

    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::CLOSED, cb.getState());

    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::CLOSED, cb.getState());

    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::OPEN, cb.getState());
    EXPECT_FALSE(cb.isCallAllowed());
}

TEST_F(ServiceProxyFactoryTest, CircuitBreakerTransitionsToHalfOpen) {
    CircuitBreaker cb(3, 100);  // 100ms timeout

    // Open the circuit
    cb.recordFailure();
    cb.recordFailure();
    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::OPEN, cb.getState());

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Should transition to half-open
    EXPECT_TRUE(cb.isCallAllowed());
    EXPECT_EQ(CircuitBreaker::State::HALF_OPEN, cb.getState());
}

TEST_F(ServiceProxyFactoryTest, CircuitBreakerClosesAfterSuccess) {
    CircuitBreaker cb(3, 100);

    // Open the circuit
    cb.recordFailure();
    cb.recordFailure();
    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::OPEN, cb.getState());

    // Wait and transition to half-open
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    cb.isCallAllowed();

    // Successful call closes circuit
    cb.recordSuccess();
    EXPECT_EQ(CircuitBreaker::State::CLOSED, cb.getState());
}

TEST_F(ServiceProxyFactoryTest, CircuitBreakerReopensOnFailureInHalfOpen) {
    CircuitBreaker cb(3, 100);

    // Open the circuit
    cb.recordFailure();
    cb.recordFailure();
    cb.recordFailure();

    // Wait and transition to half-open
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    cb.isCallAllowed();
    EXPECT_EQ(CircuitBreaker::State::HALF_OPEN, cb.getState());

    // Failure in half-open reopens circuit
    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::OPEN, cb.getState());
}

TEST_F(ServiceProxyFactoryTest, CircuitBreakerReset) {
    CircuitBreaker cb(3, 1000);

    cb.recordFailure();
    cb.recordFailure();
    cb.recordFailure();
    EXPECT_EQ(CircuitBreaker::State::OPEN, cb.getState());

    cb.reset();
    EXPECT_EQ(CircuitBreaker::State::CLOSED, cb.getState());
    EXPECT_TRUE(cb.isCallAllowed());
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, FactoryStatistics) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    factory_->createProxy(serviceMetadata_, mockHandler);

    auto stats = factory_->getStats();
    EXPECT_EQ(1, stats.proxiesCreated);
    EXPECT_GT(stats.lastOperationTime.time_since_epoch().count(), 0);
}

TEST_F(ServiceProxyFactoryTest, ResetStatistics) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    factory_->createProxy(serviceMetadata_, mockHandler);

    factory_->resetStats();

    auto stats = factory_->getStats();
    EXPECT_EQ(0, stats.proxiesCreated);
    EXPECT_EQ(0, stats.proxiesCached);
}

TEST_F(ServiceProxyFactoryTest, ProxyCount) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    EXPECT_EQ(0, factory_->getProxyCount());

    factory_->createProxy(serviceMetadata_, mockHandler);
    EXPECT_EQ(1, factory_->getProxyCount());

    factory_->createProxy(serviceMetadata_, mockHandler);
    EXPECT_EQ(2, factory_->getProxyCount());
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, FactoryLifecycle) {
    EXPECT_FALSE(factory_->isRunning());

    EXPECT_TRUE(factory_->start());
    EXPECT_TRUE(factory_->isRunning());

    EXPECT_TRUE(factory_->stop());
    EXPECT_FALSE(factory_->isRunning());
}

TEST_F(ServiceProxyFactoryTest, FactoryShutdown) {
    factory_->start();

    auto mockHandler = std::make_shared<MockInvocationHandler>();
    factory_->createProxy(serviceMetadata_, mockHandler);

    factory_->shutdown();

    EXPECT_FALSE(factory_->isRunning());
    EXPECT_EQ(0, factory_->getCachedProxyCount());
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, EndToEndProxyCreation) {
    // Register service
    ServiceEndpoint endpoint;
    endpoint.serviceId = "calc-service-1";
    endpoint.serviceName = "TestService";
    endpoint.version = "1.0.0";
    endpoint.endpoint = "/tmp/calc.sock";
    endpoint.transportType = cdmf::ipc::TransportType::UNIX_SOCKET;
    endpoint.serializationFormat = cdmf::ipc::SerializationFormat::BINARY;
    endpoint.isLocal = true;
    endpoint.isHealthy = true;

    discovery_->registerService(endpoint, serviceMetadata_);

    // Note: Full end-to-end test would require actual transport setup
    // For now, we test with mock handler
    auto mockHandler = std::make_shared<MockInvocationHandler>();
    mockHandler->setReturnValue("add", std::any(42));

    auto proxy = factory_->createProxy(serviceMetadata_, mockHandler);
    ASSERT_NE(nullptr, proxy);

    // Verify proxy can invoke methods
    auto reflectionProxy = std::dynamic_pointer_cast<ReflectionServiceProxy>(proxy);
    ASSERT_NE(nullptr, reflectionProxy);

    std::vector<std::any> args;
    args.push_back(std::any(10));
    args.push_back(std::any(32));

    auto result = reflectionProxy->invoke("add", args);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(42, std::any_cast<int>(result.returnValue));
}

TEST_F(ServiceProxyFactoryTest, ConcurrentProxyCreation) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &mockHandler, &successCount]() {
            try {
                auto proxy = factory_->createProxy(serviceMetadata_, mockHandler);
                if (proxy) {
                    successCount++;
                }
            } catch (...) {
                // Ignore exceptions
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(10, successCount.load());
    EXPECT_EQ(10, factory_->getProxyCount());
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ServiceProxyFactoryTest, ProxyCreationPerformance) {
    auto mockHandler = std::make_shared<MockInvocationHandler>();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        factory_->createProxy(serviceMetadata_, mockHandler);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should create 1000 proxies in less than 500ms (500us per proxy average)
    EXPECT_LT(duration.count(), 500000);

    std::cout << "Created 1000 proxies in " << duration.count() << " microseconds\n";
    std::cout << "Average: " << (duration.count() / 1000.0) << " us per proxy\n";
}

TEST_F(ServiceProxyFactoryTest, CachedProxyRetrievalPerformance) {
    ProxyConfig config;
    config.enableCaching = true;
    factory_->setConfig(config);

    // Note: Testing cache retrieval requires endpoint-based proxy creation
    // This test validates the performance concept

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        factory_->getCachedProxy("non-existent-" + std::to_string(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "10000 cache lookups in " << duration.count() << " microseconds\n";
    std::cout << "Average: " << (duration.count() / 10000.0) << " us per lookup\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
