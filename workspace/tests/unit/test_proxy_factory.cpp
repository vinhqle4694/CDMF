/**
 * @file test_proxy_factory.cpp
 * @brief Unit tests for proxy factory
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Factory Agent
 */

#include <gtest/gtest.h>
#include "ipc/proxy_factory.h"
#include "ipc/service_proxy.h"
#include <thread>
#include <chrono>
#include <vector>

using namespace cdmf::ipc;

/**
 * @brief Test fixture for proxy factory tests
 */
class ProxyFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset factory to clean state
        auto& factory = ProxyFactory::getInstance();
        factory.shutdown();

        // Initialize with test configuration
        ProxyFactoryConfig config;
        config.enable_caching = true;
        config.max_cached_proxies = 10;
        config.idle_timeout_seconds = 2;
        config.enable_health_check = true;
        config.health_check_interval_seconds = 1;
        config.enable_auto_reconnect = true;
        config.max_reconnect_attempts = 3;
        config.enable_statistics = true;

        // Set default proxy config
        config.default_proxy_config.default_timeout_ms = 1000;
        config.default_proxy_config.auto_reconnect = true;
        config.default_proxy_config.serialization_format = SerializationFormat::BINARY;
        config.default_proxy_config.transport_config.type = TransportType::UNIX_SOCKET;
        config.default_proxy_config.transport_config.endpoint = "/tmp/test.sock";

        factory.initialize(config);
    }

    void TearDown() override {
        auto& factory = ProxyFactory::getInstance();
        factory.shutdown();
    }

    ProxyConfig createTestConfig(const std::string& service_name, const std::string& endpoint) {
        ProxyConfig config;
        config.service_name = service_name;
        config.default_timeout_ms = 1000;
        config.auto_reconnect = true;
        config.serialization_format = SerializationFormat::BINARY;
        config.transport_config.type = TransportType::UNIX_SOCKET;
        config.transport_config.endpoint = endpoint;
        config.transport_config.connect_timeout_ms = 1000;
        return config;
    }
};

// Singleton and initialization tests

TEST_F(ProxyFactoryTest, GetInstanceReturnsSameInstance) {
    auto& factory1 = ProxyFactory::getInstance();
    auto& factory2 = ProxyFactory::getInstance();

    EXPECT_EQ(&factory1, &factory2);
}

TEST_F(ProxyFactoryTest, InitializeSucceeds) {
    auto& factory = ProxyFactory::getInstance();
    EXPECT_TRUE(factory.isInitialized());
}

TEST_F(ProxyFactoryTest, ShutdownCleansUpProxies) {
    auto& factory = ProxyFactory::getInstance();

    // Create some proxies
    auto config = createTestConfig("test_service", "/tmp/test1.sock");
    auto proxy1 = factory.getProxy("test_service", config);
    ASSERT_NE(proxy1, nullptr);

    auto stats_before = factory.getAggregatedStats();
    EXPECT_GT(stats_before.active_proxies, 0);

    // Shutdown
    factory.shutdown();

    EXPECT_FALSE(factory.isInitialized());
}

// Proxy creation tests

TEST_F(ProxyFactoryTest, CreateProxySucceeds) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.createProxy(config);

    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->getConfig().service_name, "test_service");
}

TEST_F(ProxyFactoryTest, GetProxyCreatesNewProxy) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->getConfig().service_name, "test_service");
}

TEST_F(ProxyFactoryTest, GetProxyWithEndpointSucceeds) {
    auto& factory = ProxyFactory::getInstance();

    auto proxy = factory.getProxy("test_service", "/tmp/test.sock", TransportType::UNIX_SOCKET);

    ASSERT_NE(proxy, nullptr);
}

// Caching tests

TEST_F(ProxyFactoryTest, GetProxyReturnsCachedInstance) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");

    auto proxy1 = factory.getProxy("test_service", config);
    auto proxy2 = factory.getProxy("test_service", config);

    ASSERT_NE(proxy1, nullptr);
    ASSERT_NE(proxy2, nullptr);
    EXPECT_EQ(proxy1.get(), proxy2.get());  // Same instance

    auto stats = factory.getAggregatedStats();
    EXPECT_GT(stats.cache_hits, 0);
}

TEST_F(ProxyFactoryTest, CacheMissForDifferentEndpoint) {
    auto& factory = ProxyFactory::getInstance();

    auto config1 = createTestConfig("service1", "/tmp/test1.sock");
    auto config2 = createTestConfig("service2", "/tmp/test2.sock");

    auto proxy1 = factory.getProxy("service1", config1);
    auto proxy2 = factory.getProxy("service2", config2);

    ASSERT_NE(proxy1, nullptr);
    ASSERT_NE(proxy2, nullptr);
    EXPECT_NE(proxy1.get(), proxy2.get());  // Different instances

    auto stats = factory.getAggregatedStats();
    EXPECT_GT(stats.cache_misses, 0);
}

TEST_F(ProxyFactoryTest, IsCachedReturnsCorrectStatus) {
    auto& factory = ProxyFactory::getInstance();

    EXPECT_FALSE(factory.isCached("test_service"));

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    EXPECT_TRUE(factory.isCached("test_service"));
}

TEST_F(ProxyFactoryTest, GetCachedProxyCountReturnsCorrectCount) {
    auto& factory = ProxyFactory::getInstance();

    EXPECT_EQ(factory.getCachedProxyCount(), 0);

    auto config1 = createTestConfig("service1", "/tmp/test1.sock");
    auto config2 = createTestConfig("service2", "/tmp/test2.sock");

    factory.getProxy("service1", config1);
    EXPECT_EQ(factory.getCachedProxyCount(), 1);

    factory.getProxy("service2", config2);
    EXPECT_EQ(factory.getCachedProxyCount(), 2);
}

TEST_F(ProxyFactoryTest, RemoveFromCacheRemovesProxy) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    EXPECT_TRUE(factory.isCached("test_service"));

    factory.removeFromCache("test_service");

    EXPECT_FALSE(factory.isCached("test_service"));
}

TEST_F(ProxyFactoryTest, ClearCacheRemovesAllProxies) {
    auto& factory = ProxyFactory::getInstance();

    auto config1 = createTestConfig("service1", "/tmp/test1.sock");
    auto config2 = createTestConfig("service2", "/tmp/test2.sock");

    factory.getProxy("service1", config1);
    factory.getProxy("service2", config2);

    EXPECT_EQ(factory.getCachedProxyCount(), 2);

    factory.clearCache();

    EXPECT_EQ(factory.getCachedProxyCount(), 0);
}

TEST_F(ProxyFactoryTest, MaxCachedProxiesEnforcesLimit) {
    auto& factory = ProxyFactory::getInstance();

    // Create more proxies than the limit (10)
    for (int i = 0; i < 15; i++) {
        std::string endpoint = "/tmp/test" + std::to_string(i) + ".sock";
        auto config = createTestConfig("service" + std::to_string(i), endpoint);
        factory.getProxy("service" + std::to_string(i), config);
    }

    // Cache should not exceed limit
    EXPECT_LE(factory.getCachedProxyCount(), 10);
}

// Lifecycle management tests

TEST_F(ProxyFactoryTest, DestroyProxyRemovesProxy) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    EXPECT_TRUE(factory.isCached("test_service"));

    bool destroyed = factory.destroyProxy("test_service");

    EXPECT_TRUE(destroyed);
    EXPECT_FALSE(factory.isCached("test_service"));
}

TEST_F(ProxyFactoryTest, DestroyAllProxiesRemovesAll) {
    auto& factory = ProxyFactory::getInstance();

    auto config1 = createTestConfig("service1", "/tmp/test1.sock");
    auto config2 = createTestConfig("service2", "/tmp/test2.sock");

    factory.getProxy("service1", config1);
    factory.getProxy("service2", config2);

    factory.destroyAllProxies();

    EXPECT_EQ(factory.getCachedProxyCount(), 0);
}

TEST_F(ProxyFactoryTest, CleanupIdleProxiesRemovesIdleProxies) {
    // Reconfigure with shorter timeout and disabled background tasks to avoid race conditions
    auto& factory = ProxyFactory::getInstance();
    factory.shutdown();

    ProxyFactoryConfig config;
    config.enable_caching = true;
    config.max_cached_proxies = 10;
    config.idle_timeout_seconds = 1;  // Reduced from 2
    config.enable_health_check = false;  // Disable background threads
    config.enable_auto_reconnect = false;
    config.enable_statistics = true;
    config.default_proxy_config.default_timeout_ms = 1000;
    config.default_proxy_config.auto_reconnect = false;
    config.default_proxy_config.serialization_format = SerializationFormat::BINARY;
    config.default_proxy_config.transport_config.type = TransportType::UNIX_SOCKET;
    config.default_proxy_config.transport_config.endpoint = "/tmp/test.sock";

    factory.initialize(config);

    auto proxy_config = createTestConfig("test_service", "/tmp/test.sock");
    {
        auto proxy = factory.getProxy("test_service", proxy_config);
        EXPECT_EQ(factory.getCachedProxyCount(), 1);
        // proxy goes out of scope here, allowing cleanup
    }

    // Wait for idle timeout (1 second + larger margin for CI/Docker)
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Check if background cleanup thread already removed the proxy
    size_t count_before = factory.getCachedProxyCount();
    uint32_t cleaned = factory.cleanupIdleProxies();

    // Either manual cleanup removed it, or background thread already did
    EXPECT_EQ(factory.getCachedProxyCount(), 0);
    if (count_before > 0) {
        EXPECT_GT(cleaned, 0);  // We cleaned it
    } else {
        EXPECT_EQ(cleaned, 0);  // Background thread already cleaned it
    }
}

// Health monitoring tests

TEST_F(ProxyFactoryTest, CheckProxyHealthReturnsStatus) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    // Health check should work (even if not connected)
    bool healthy = factory.checkProxyHealth("test_service");
    // Default health check returns connection status, which is false if not connected
    EXPECT_FALSE(healthy);
}

TEST_F(ProxyFactoryTest, CheckAllProxiesHealthReturnsCount) {
    auto& factory = ProxyFactory::getInstance();

    auto config1 = createTestConfig("service1", "/tmp/test1.sock");
    auto config2 = createTestConfig("service2", "/tmp/test2.sock");

    factory.getProxy("service1", config1);
    factory.getProxy("service2", config2);

    uint32_t unhealthy = factory.checkAllProxiesHealth();

    // Both should be unhealthy (not connected)
    EXPECT_EQ(unhealthy, 2);
}

TEST_F(ProxyFactoryTest, CustomHealthCheckCallbackWorks) {
    auto& factory = ProxyFactory::getInstance();

    bool callback_called = false;
    factory.setHealthCheckCallback([&callback_called](const std::string& /* name */, ServiceProxyPtr /* proxy */) {
        callback_called = true;
        return true;  // Always healthy
    });

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    bool healthy = factory.checkProxyHealth("test_service");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(healthy);
}

// Configuration tests

TEST_F(ProxyFactoryTest, GetConfigReturnsCurrentConfig) {
    auto& factory = ProxyFactory::getInstance();

    auto config = factory.getConfig();

    EXPECT_TRUE(config.enable_caching);
    EXPECT_EQ(config.max_cached_proxies, 10);
}

TEST_F(ProxyFactoryTest, UpdateConfigUpdatesConfiguration) {
    auto& factory = ProxyFactory::getInstance();

    ProxyFactoryConfig new_config;
    new_config.enable_caching = false;
    new_config.max_cached_proxies = 20;

    factory.updateConfig(new_config);

    auto config = factory.getConfig();
    EXPECT_FALSE(config.enable_caching);
    EXPECT_EQ(config.max_cached_proxies, 20);
}

TEST_F(ProxyFactoryTest, SetDefaultProxyConfigWorks) {
    auto& factory = ProxyFactory::getInstance();

    ProxyConfig default_config;
    default_config.default_timeout_ms = 2000;
    default_config.serialization_format = SerializationFormat::JSON;

    factory.setDefaultProxyConfig(default_config);

    auto config = factory.getDefaultProxyConfig();
    EXPECT_EQ(config.default_timeout_ms, 2000);
    EXPECT_EQ(config.serialization_format, SerializationFormat::JSON);
}

// Statistics tests

TEST_F(ProxyFactoryTest, GetAggregatedStatsReturnsStats) {
    auto& factory = ProxyFactory::getInstance();

    auto stats = factory.getAggregatedStats();

    EXPECT_GE(stats.total_proxies_created, 0);
    EXPECT_GE(stats.active_proxies, 0);
}

TEST_F(ProxyFactoryTest, StatisticsTrackProxyCreation) {
    auto& factory = ProxyFactory::getInstance();

    auto stats_before = factory.getAggregatedStats();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    auto stats_after = factory.getAggregatedStats();

    EXPECT_GT(stats_after.total_proxies_created, stats_before.total_proxies_created);
    EXPECT_GT(stats_after.active_proxies, stats_before.active_proxies);
}

TEST_F(ProxyFactoryTest, StatisticsTrackCacheHits) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");

    factory.getProxy("test_service", config);  // Miss
    auto stats_before = factory.getAggregatedStats();

    factory.getProxy("test_service", config);  // Hit

    auto stats_after = factory.getAggregatedStats();

    EXPECT_GT(stats_after.cache_hits, stats_before.cache_hits);
}

TEST_F(ProxyFactoryTest, ResetStatsClearsCounters) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    factory.getProxy("test_service", config);

    auto stats_before = factory.getAggregatedStats();
    EXPECT_GT(stats_before.cache_misses, 0);

    factory.resetStats();

    auto stats_after = factory.getAggregatedStats();
    EXPECT_EQ(stats_after.cache_misses, 0);
}

TEST_F(ProxyFactoryTest, GetProxyInfoReturnsInstanceInfo) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    auto info = factory.getProxyInfo("test_service");

    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->service_name, "test_service");
    EXPECT_EQ(info->endpoint, "/tmp/test.sock");
}

TEST_F(ProxyFactoryTest, GetAllProxyInfoReturnsAllInfo) {
    auto& factory = ProxyFactory::getInstance();

    auto config1 = createTestConfig("service1", "/tmp/test1.sock");
    auto config2 = createTestConfig("service2", "/tmp/test2.sock");

    factory.getProxy("service1", config1);
    factory.getProxy("service2", config2);

    auto all_info = factory.getAllProxyInfo();

    EXPECT_EQ(all_info.size(), 2);
}

// Callback tests

TEST_F(ProxyFactoryTest, ProxyCreatedCallbackInvoked) {
    auto& factory = ProxyFactory::getInstance();

    bool callback_called = false;
    std::string created_service_name;

    factory.setProxyCreatedCallback([&](const std::string& name, ServiceProxyPtr /* proxy */) {
        callback_called = true;
        created_service_name = name;
    });

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(created_service_name, "test_service");

    // Clear callback to avoid dangling reference in subsequent tests
    factory.setProxyCreatedCallback(nullptr);
}

TEST_F(ProxyFactoryTest, ProxyDestroyedCallbackInvoked) {
    auto& factory = ProxyFactory::getInstance();

    bool callback_called = false;
    std::string destroyed_service_name;

    factory.setProxyDestroyedCallback([&](const std::string& name, ServiceProxyPtr /* proxy */) {
        callback_called = true;
        destroyed_service_name = name;
    });

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    auto proxy = factory.getProxy("test_service", config);

    factory.destroyProxy("test_service");

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(destroyed_service_name, "test_service");

    // Clear callback to avoid dangling reference in subsequent tests
    factory.setProxyDestroyedCallback(nullptr);
}

// Concurrent access tests

TEST_F(ProxyFactoryTest, ConcurrentProxyCreationThreadSafe) {
    auto& factory = ProxyFactory::getInstance();

    const int num_threads = 10;
    const int proxies_per_thread = 5;

    std::vector<std::thread> threads;
    std::atomic<int> successful_creations{0};

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < proxies_per_thread; j++) {
                std::string service_name = "service_" + std::to_string(i) + "_" + std::to_string(j);
                std::string endpoint = "/tmp/" + service_name + ".sock";
                auto config = createTestConfig(service_name, endpoint);

                auto proxy = factory.getProxy(service_name, config);
                if (proxy) {
                    successful_creations++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successful_creations, num_threads * proxies_per_thread);
}

TEST_F(ProxyFactoryTest, ConcurrentCacheAccessThreadSafe) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("shared_service", "/tmp/shared.sock");
    auto initial_proxy = factory.getProxy("shared_service", config);

    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successful_retrievals{0};

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 10; j++) {
                auto proxy = factory.getProxy("shared_service", config);
                if (proxy) {
                    successful_retrievals++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successful_retrievals, num_threads * 10);

    auto stats = factory.getAggregatedStats();
    EXPECT_GT(stats.cache_hits, 0);
}

// ProxyBuilder tests

TEST_F(ProxyFactoryTest, ProxyBuilderCreatesProxyWithConfiguration) {
    ProxyBuilder builder;

    auto proxy = builder
        .withServiceName("test_service")
        .withEndpoint("/tmp/test.sock")
        .withTransportType(TransportType::UNIX_SOCKET)
        .withTimeout(2000)
        .build();

    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->getConfig().service_name, "test_service");
    EXPECT_EQ(proxy->getConfig().default_timeout_ms, 2000);
}

TEST_F(ProxyFactoryTest, ProxyBuilderWithRetryPolicy) {
    RetryPolicy retry;
    retry.enabled = true;
    retry.max_attempts = 5;
    retry.exponential_backoff = true;

    ProxyBuilder builder;

    auto proxy = builder
        .withServiceName("test_service")
        .withEndpoint("/tmp/test.sock")
        .withRetryPolicy(retry)
        .build();

    ASSERT_NE(proxy, nullptr);
    EXPECT_TRUE(proxy->getConfig().retry_policy.enabled);
    EXPECT_EQ(proxy->getConfig().retry_policy.max_attempts, 5);
}

TEST_F(ProxyFactoryTest, ProxyBuilderBuildConfigReturnsConfiguration) {
    ProxyBuilder builder;

    auto config = builder
        .withServiceName("test_service")
        .withEndpoint("/tmp/test.sock")
        .withTimeout(3000)
        .buildConfig();

    EXPECT_EQ(config.service_name, "test_service");
    EXPECT_EQ(config.transport_config.endpoint, "/tmp/test.sock");
    EXPECT_EQ(config.default_timeout_ms, 3000);
}

// Performance tests

TEST_F(ProxyFactoryTest, ProxyCreationPerformance) {
    auto& factory = ProxyFactory::getInstance();

    const int num_proxies = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_proxies; i++) {
        std::string service_name = "service_" + std::to_string(i);
        std::string endpoint = "/tmp/" + service_name + ".sock";
        auto config = createTestConfig(service_name, endpoint);
        factory.getProxy(service_name, config);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average creation time should be < 100 microseconds
    double avg_time_us = duration.count() / static_cast<double>(num_proxies);
    EXPECT_LT(avg_time_us, 100.0);

    std::cout << "Average proxy creation time: " << avg_time_us << " microseconds" << std::endl;
}

TEST_F(ProxyFactoryTest, CacheRetrievalPerformance) {
    auto& factory = ProxyFactory::getInstance();

    auto config = createTestConfig("test_service", "/tmp/test.sock");
    factory.getProxy("test_service", config);  // Prime the cache

    const int num_retrievals = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_retrievals; i++) {
        factory.getProxy("test_service", config);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average retrieval time should be < 10 microseconds
    double avg_time_us = duration.count() / static_cast<double>(num_retrievals);
    EXPECT_LT(avg_time_us, 10.0);

    std::cout << "Average cache retrieval time: " << avg_time_us << " microseconds" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
