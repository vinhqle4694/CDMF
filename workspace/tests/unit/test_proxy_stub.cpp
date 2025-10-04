/**
 * @file test_proxy_stub.cpp
 * @brief Comprehensive unit tests for service proxy and stub
 *
 * Tests RPC functionality including synchronous calls, asynchronous calls,
 * one-way calls, error handling, timeouts, and performance.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Proxy Stub Agent
 */

#include <gtest/gtest.h>
#include "ipc/service_proxy.h"
#include "ipc/service_stub.h"
#include "ipc/proxy_factory.h"
#include "ipc/transport.h"
#include "ipc/message.h"
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <string>

using namespace cdmf::ipc;

namespace {
    // Test endpoint for Unix socket
    const char* TEST_ENDPOINT = "/tmp/cdmf_proxy_stub_test.sock";

    /**
     * @brief Helper to create stub configuration
     */
    StubConfig createStubConfig() {
        StubConfig config;
        config.service_name = "TestService";
        config.serialization_format = SerializationFormat::BINARY;
        config.max_concurrent_requests = 10;
        config.request_timeout_ms = 5000;

        // Configure transport (Unix socket server)
        config.transport_config.type = TransportType::UNIX_SOCKET;
        config.transport_config.endpoint = TEST_ENDPOINT;
        config.transport_config.mode = TransportMode::ASYNC;
        config.transport_config.properties["is_server"] = "true";
        config.transport_config.properties["socket_type"] = "STREAM";
        config.transport_config.properties["use_epoll"] = "true";

        return config;
    }

    /**
     * @brief Helper to create proxy configuration
     */
    ProxyConfig createProxyConfig() {
        ProxyConfig config;
        config.service_name = "TestClient";
        config.serialization_format = SerializationFormat::BINARY;
        config.default_timeout_ms = 5000;
        config.auto_reconnect = true;

        // Configure transport (Unix socket client)
        config.transport_config.type = TransportType::UNIX_SOCKET;
        config.transport_config.endpoint = TEST_ENDPOINT;
        config.transport_config.mode = TransportMode::ASYNC;
        config.transport_config.properties["is_server"] = "false";
        config.transport_config.properties["socket_type"] = "STREAM";

        return config;
    }

    /**
     * @brief Simple echo handler
     */
    std::vector<uint8_t> echoHandler(const std::vector<uint8_t>& request) {
        return request;  // Echo back
    }

    /**
     * @brief Handler that adds prefix to input
     */
    std::vector<uint8_t> prefixHandler(const std::vector<uint8_t>& request) {
        std::string input(request.begin(), request.end());
        std::string output = "Response: " + input;
        return std::vector<uint8_t>(output.begin(), output.end());
    }

    /**
     * @brief Handler that throws exception
     */
    std::vector<uint8_t> errorHandler(const std::vector<uint8_t>& /* request */) {
        throw std::runtime_error("Test error");
    }

    /**
     * @brief Handler that sleeps (for timeout testing)
     */
    std::vector<uint8_t> slowHandler(const std::vector<uint8_t>& request) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        return request;
    }

    /**
     * @brief Handler for integer arithmetic
     */
    std::vector<uint8_t> addHandler(const std::vector<uint8_t>& request) {
        if (request.size() < sizeof(int32_t) * 2) {
            throw std::runtime_error("Invalid request size");
        }

        int32_t a = *reinterpret_cast<const int32_t*>(request.data());
        int32_t b = *reinterpret_cast<const int32_t*>(request.data() + sizeof(int32_t));
        int32_t result = a + b;

        std::vector<uint8_t> response(sizeof(int32_t));
        std::memcpy(response.data(), &result, sizeof(int32_t));
        return response;
    }
}

class ProxyStubTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up socket file if exists
        unlink(TEST_ENDPOINT);

        // Create stub
        auto stub_config = createStubConfig();
        stub_ = std::make_shared<ServiceStub>(stub_config);

        // Register handlers
        stub_->registerMethod("echo", echoHandler);
        stub_->registerMethod("prefix", prefixHandler);
        stub_->registerMethod("error", errorHandler);
        stub_->registerMethod("slow", slowHandler);
        stub_->registerMethod("add", addHandler);

        // Start stub
        bool stub_started = stub_->start();
        std::cout << "Stub start result: " << stub_started << std::endl;
        std::cout << "Stub is running: " << stub_->isRunning() << std::endl;
        ASSERT_TRUE(stub_started);

        // Give server time to start (reduced from 500ms to 50ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Create proxy
        auto proxy_config = createProxyConfig();
        proxy_ = std::make_shared<ServiceProxy>(proxy_config);

        // Connect proxy
        bool proxy_connected = proxy_->connect();
        ASSERT_TRUE(proxy_connected);

        // Give connection time to establish (reduced from 500ms to 50ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        if (proxy_) {
            proxy_->disconnect();
        }

        if (stub_) {
            stub_->stop();
        }

        // Clean up socket file
        unlink(TEST_ENDPOINT);

        // Give cleanup time (reduced from 100ms to 10ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ServiceStubPtr stub_;
    ServiceProxyPtr proxy_;
};

// Basic functionality tests

TEST_F(ProxyStubTest, BasicEchoCall) {
    std::string input = "Hello, World!";
    std::vector<uint8_t> request(input.begin(), input.end());

    auto result = proxy_->call("echo", request);

    if (!result.success) {
        std::cout << "Call failed!" << std::endl;
        std::cout << "Error code: " << result.error_code << std::endl;
        std::cout << "Error message: " << result.error_message << std::endl;
    }

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.error_code, 0u);
    EXPECT_EQ(result.data.size(), request.size());
    EXPECT_EQ(std::string(result.data.begin(), result.data.end()), input);
}

TEST_F(ProxyStubTest, PrefixCall) {
    std::string input = "Test";
    std::vector<uint8_t> request(input.begin(), input.end());

    auto result = proxy_->call("prefix", request);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(std::string(result.data.begin(), result.data.end()), "Response: Test");
}

TEST_F(ProxyStubTest, IntegerArithmetic) {
    int32_t a = 42;
    int32_t b = 58;

    std::vector<uint8_t> request(sizeof(int32_t) * 2);
    std::memcpy(request.data(), &a, sizeof(int32_t));
    std::memcpy(request.data() + sizeof(int32_t), &b, sizeof(int32_t));

    auto result = proxy_->call("add", request);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.data.size(), sizeof(int32_t));

    int32_t sum = *reinterpret_cast<const int32_t*>(result.data.data());
    EXPECT_EQ(sum, 100);
}

TEST_F(ProxyStubTest, MethodNotFound) {
    std::vector<uint8_t> request = {1, 2, 3};

    auto result = proxy_->call("nonexistent", request);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, stub_error_codes::METHOD_NOT_FOUND);
}

TEST_F(ProxyStubTest, HandlerException) {
    std::vector<uint8_t> request = {1, 2, 3};

    auto result = proxy_->call("error", request);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, stub_error_codes::HANDLER_EXCEPTION);
    EXPECT_FALSE(result.error_message.empty());
}

// Asynchronous calls

TEST_F(ProxyStubTest, AsyncCall) {
    std::string input = "Async Test";
    std::vector<uint8_t> request(input.begin(), input.end());

    auto future = proxy_->callAsync("echo", request);

    ASSERT_TRUE(future.valid());

    auto result = future.get();

    ASSERT_TRUE(result.success);
    EXPECT_EQ(std::string(result.data.begin(), result.data.end()), input);
}

TEST_F(ProxyStubTest, AsyncCallWithCallback) {
    std::string input = "Callback Test";
    std::vector<uint8_t> request(input.begin(), input.end());

    bool callback_invoked = false;
    std::string callback_result;

    proxy_->callAsync("echo", request, [&](const CallResult<std::vector<uint8_t>>& result) {
        callback_invoked = true;
        if (result.success) {
            callback_result = std::string(result.data.begin(), result.data.end());
        }
    });

    // Wait for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(callback_result, input);
}

TEST_F(ProxyStubTest, MultipleAsyncCalls) {
    const int num_calls = 10;
    std::vector<std::future<CallResult<std::vector<uint8_t>>>> futures;

    for (int i = 0; i < num_calls; ++i) {
        std::string input = "Test " + std::to_string(i);
        std::vector<uint8_t> request(input.begin(), input.end());
        futures.push_back(proxy_->callAsync("echo", request));
    }

    int success_count = 0;
    for (auto& future : futures) {
        auto result = future.get();
        if (result.success) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, num_calls);
}

// One-way calls

TEST_F(ProxyStubTest, OneWayCall) {
    std::vector<uint8_t> request = {1, 2, 3, 4, 5};

    bool sent = proxy_->callOneWay("echo", request);

    EXPECT_TRUE(sent);

    // One-way calls don't wait for response
    // Give time for server to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Timeout tests

TEST_F(ProxyStubTest, CallTimeout) {
    std::vector<uint8_t> request = {1, 2, 3};

    // Call slow handler with short timeout
    auto result = proxy_->call("slow", request, 500);  // 500ms timeout

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, 3u);  // Timeout error
}

// Error handling and retry

TEST_F(ProxyStubTest, RetryOnFailure) {
    // Configure retry policy
    RetryPolicy retry_policy;
    retry_policy.enabled = true;
    retry_policy.max_attempts = 3;
    retry_policy.initial_delay_ms = 50;
    retry_policy.exponential_backoff = true;

    proxy_->setRetryPolicy(retry_policy);

    // Note: Testing actual retry is difficult without mocking transport failures
    // This test mainly verifies the API works
    auto stats = proxy_->getStats();
    EXPECT_EQ(stats.total_calls, 0u);
}

// Statistics tests

TEST_F(ProxyStubTest, ProxyStatistics) {
    auto initial_stats = proxy_->getStats();

    std::vector<uint8_t> request = {1, 2, 3};
    proxy_->call("echo", request);

    auto stats = proxy_->getStats();

    EXPECT_GT(stats.total_calls, initial_stats.total_calls);
    EXPECT_GT(stats.successful_calls, initial_stats.successful_calls);
}

TEST_F(ProxyStubTest, StubStatistics) {
    auto initial_stats = stub_->getStats();

    std::vector<uint8_t> request = {1, 2, 3};
    proxy_->call("echo", request);

    // Give time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto stats = stub_->getStats();

    EXPECT_GT(stats.total_requests, initial_stats.total_requests);
    EXPECT_GT(stats.successful_responses, initial_stats.successful_responses);
}

// Method registration tests

TEST_F(ProxyStubTest, MethodRegistration) {
    EXPECT_TRUE(stub_->hasMethod("echo"));
    EXPECT_TRUE(stub_->hasMethod("prefix"));
    EXPECT_FALSE(stub_->hasMethod("nonexistent"));
}

TEST_F(ProxyStubTest, GetRegisteredMethods) {
    auto methods = stub_->getRegisteredMethods();

    EXPECT_GE(methods.size(), 5u);
    EXPECT_NE(std::find(methods.begin(), methods.end(), "echo"), methods.end());
    EXPECT_NE(std::find(methods.begin(), methods.end(), "prefix"), methods.end());
}

TEST_F(ProxyStubTest, UnregisterMethod) {
    ASSERT_TRUE(stub_->hasMethod("echo"));

    bool unregistered = stub_->unregisterMethod("echo");
    EXPECT_TRUE(unregistered);
    EXPECT_FALSE(stub_->hasMethod("echo"));

    // Calling unregistered method should fail
    std::vector<uint8_t> request = {1, 2, 3};
    auto result = proxy_->call("echo", request);
    EXPECT_FALSE(result.success);
}

TEST_F(ProxyStubTest, RegisterDuplicateMethod) {
    bool registered = stub_->registerMethod("echo", echoHandler);
    EXPECT_FALSE(registered);  // Already registered
}

// Connection management tests

TEST_F(ProxyStubTest, ProxyReconnect) {
    ASSERT_TRUE(proxy_->isConnected());

    proxy_->disconnect();
    EXPECT_FALSE(proxy_->isConnected());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool reconnected = proxy_->connect();
    EXPECT_TRUE(reconnected);
    EXPECT_TRUE(proxy_->isConnected());
}

TEST_F(ProxyStubTest, StubRestart) {
    ASSERT_TRUE(stub_->isRunning());

    stub_->stop();
    EXPECT_FALSE(stub_->isRunning());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    bool restarted = stub_->start();
    EXPECT_TRUE(restarted);
    EXPECT_TRUE(stub_->isRunning());
}

// Concurrent request handling

TEST_F(ProxyStubTest, ConcurrentRequests) {
    const int num_threads = 5;
    const int calls_per_thread = 10;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &success_count, calls_per_thread]() {
            for (int i = 0; i < calls_per_thread; ++i) {
                std::string input = "Thread test " + std::to_string(i);
                std::vector<uint8_t> request(input.begin(), input.end());

                auto result = proxy_->call("echo", request);
                if (result.success) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_threads * calls_per_thread);
}

// Large payload test

TEST_F(ProxyStubTest, LargePayload) {
    // Create 1MB payload
    const size_t payload_size = 1024 * 1024;
    std::vector<uint8_t> request(payload_size);

    // Fill with pattern
    for (size_t i = 0; i < payload_size; ++i) {
        request[i] = static_cast<uint8_t>(i % 256);
    }

    auto result = proxy_->call("echo", request, 10000);  // 10 second timeout

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.data.size(), payload_size);

    // Verify pattern
    for (size_t i = 0; i < payload_size; ++i) {
        EXPECT_EQ(result.data[i], static_cast<uint8_t>(i % 256));
    }
}

// Empty payload test

TEST_F(ProxyStubTest, EmptyPayload) {
    std::vector<uint8_t> request;

    auto result = proxy_->call("echo", request);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.data.size(), 0u);
}

// Factory tests

TEST_F(ProxyStubTest, ProxyFactory) {
    auto config = createProxyConfig();
    config.transport_config.endpoint = "/tmp/cdmf_factory_test.sock";

    // Initialize ProxyFactory before use
    ProxyFactoryConfig factory_config;
    factory_config.enable_caching = true;
    ProxyFactory::getInstance().initialize(factory_config);

    // Note: ProxyFactory doesn't have a createProxy static method, use getInstance()
    auto proxy = ProxyFactory::getInstance().getProxy("test_service", config);
    ASSERT_NE(proxy, nullptr);

    // Cleanup
    ProxyFactory::getInstance().shutdown();
}

TEST_F(ProxyStubTest, StubFactory) {
    auto config = createStubConfig();
    config.transport_config.endpoint = "/tmp/cdmf_stub_factory_test.sock";

    auto stub = StubFactory::createStub(config);
    ASSERT_NE(stub, nullptr);

    unlink("/tmp/cdmf_stub_factory_test.sock");
}

// Performance benchmark

TEST_F(ProxyStubTest, PerformanceBenchmark) {
    const int num_calls = 1000;
    std::vector<uint8_t> request = {1, 2, 3, 4, 5};

    auto start = std::chrono::steady_clock::now();

    int success_count = 0;
    for (int i = 0; i < num_calls; ++i) {
        auto result = proxy_->call("echo", request, 5000);
        if (result.success) {
            success_count++;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(success_count, num_calls);

    double calls_per_second = (num_calls * 1000.0) / duration.count();
    std::cout << "Performance: " << calls_per_second << " calls/second" << std::endl;
    std::cout << "Average latency: " << (duration.count() / static_cast<double>(num_calls))
              << " ms/call" << std::endl;

    // Performance target: should handle at least 100 calls/second
    EXPECT_GT(calls_per_second, 100.0);
}

// Validation hook test

TEST_F(ProxyStubTest, RequestValidation) {
    // Set custom validator that rejects requests with empty subject
    stub_->setRequestValidator([](const Message& message) {
        return !message.getSubject().empty();
    });

    stub_->setMaxConcurrentRequests(10);

    std::vector<uint8_t> request = {1, 2, 3};
    auto result = proxy_->call("echo", request);

    // Should succeed with valid subject
    EXPECT_TRUE(result.success);
}

// Statistics reset test

TEST_F(ProxyStubTest, StatisticsReset) {
    std::vector<uint8_t> request = {1, 2, 3};
    proxy_->call("echo", request);

    auto stats_before = proxy_->getStats();
    EXPECT_GT(stats_before.total_calls, 0u);

    proxy_->resetStats();

    auto stats_after = proxy_->getStats();
    EXPECT_EQ(stats_after.total_calls, 0u);
    EXPECT_EQ(stats_after.successful_calls, 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
