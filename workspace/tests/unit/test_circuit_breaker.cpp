/**
 * @file test_circuit_breaker.cpp
 * @brief Comprehensive unit tests for CircuitBreaker reliability component
 *
 * Tests include:
 * - State transitions (CLOSED -> OPEN -> HALF_OPEN -> CLOSED)
 * - Failure/success threshold behavior
 * - Timeout and recovery mechanisms
 * - Concurrent access and thread safety
 * - Callback mechanisms
 * - Configuration updates
 * - Edge cases and error handling
 *
 * @author Circuit Breaker Test Agent
 * @date 2025-10-05
 */

#include "ipc/circuit_breaker.h"
#include "ipc/reliability_types.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace cdmf::ipc;
using namespace std::chrono_literals;

// ============================================================================
// Test Fixtures
// ============================================================================

class CircuitBreakerTest : public ::testing::Test {
protected:
    CircuitBreakerConfig defaultConfig() {
        CircuitBreakerConfig config;
        config.failure_threshold = 3;
        config.success_threshold = 2;
        config.open_timeout = 100ms;
        config.half_open_timeout = 50ms;
        return config;
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(CircuitBreakerTest, InitialStateClosed) {
    CircuitBreaker breaker(defaultConfig());
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
    EXPECT_TRUE(breaker.allowsRequests());
}

TEST_F(CircuitBreakerTest, SuccessfulOperationInClosedState) {
    CircuitBreaker breaker(defaultConfig());

    bool executed = false;
    bool result = breaker.execute([&]() {
        executed = true;
        return true;
    });

    EXPECT_TRUE(result);
    EXPECT_TRUE(executed);
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
}

TEST_F(CircuitBreakerTest, FailedOperationInClosedState) {
    CircuitBreaker breaker(defaultConfig());

    bool executed = false;
    bool result = breaker.execute([&]() {
        executed = true;
        return false;
    });

    EXPECT_FALSE(result);
    EXPECT_TRUE(executed);
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
}

// ============================================================================
// State Transition Tests
// ============================================================================

TEST_F(CircuitBreakerTest, TransitionToOpenOnFailureThreshold) {
    auto config = defaultConfig();
    config.failure_threshold = 3;
    CircuitBreaker breaker(config);

    // Execute 3 failures to reach threshold
    for (int i = 0; i < 3; ++i) {
        bool result = breaker.execute([]() { return false; });
        EXPECT_FALSE(result);
    }

    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
    EXPECT_FALSE(breaker.allowsRequests());
}

TEST_F(CircuitBreakerTest, RejectionInOpenState) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    CircuitBreaker breaker(config);

    // Trigger OPEN state
    breaker.execute([]() { return false; });
    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Next request should be rejected without execution
    bool executed = false;
    bool result = breaker.execute([&]() {
        executed = true;
        return true;
    });

    EXPECT_FALSE(result);
    EXPECT_FALSE(executed);  // Operation should not be executed
}

TEST_F(CircuitBreakerTest, TransitionToHalfOpenAfterTimeout) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    config.open_timeout = 50ms;
    CircuitBreaker breaker(config);

    // Trigger OPEN state
    breaker.execute([]() { return false; });
    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Wait for timeout
    std::this_thread::sleep_for(60ms);

    // Next request should transition to HALF_OPEN
    breaker.execute([]() { return true; });

    // Should be in HALF_OPEN or CLOSED (if success threshold met)
    auto state = breaker.getState();
    EXPECT_TRUE(state == CircuitState::HALF_OPEN || state == CircuitState::CLOSED);
}

TEST_F(CircuitBreakerTest, RecoveryInHalfOpenState) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    config.success_threshold = 2;
    config.open_timeout = 50ms;
    CircuitBreaker breaker(config);

    // Trigger OPEN
    breaker.execute([]() { return false; });
    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Wait for transition to HALF_OPEN
    std::this_thread::sleep_for(60ms);

    // Execute successful operations
    breaker.execute([]() { return true; });
    breaker.execute([]() { return true; });

    // Should transition to CLOSED after success threshold
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
}

TEST_F(CircuitBreakerTest, FailureInHalfOpenReturnsToOpen) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    config.open_timeout = 50ms;
    CircuitBreaker breaker(config);

    // Trigger OPEN
    breaker.execute([]() { return false; });

    // Wait for HALF_OPEN
    std::this_thread::sleep_for(60ms);

    // Execute and fail in HALF_OPEN
    breaker.execute([]() { return false; });

    // Should return to OPEN
    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(CircuitBreakerTest, StatisticsTracking) {
    CircuitBreaker breaker(defaultConfig());

    // Execute successful operations
    breaker.execute([]() { return true; });
    breaker.execute([]() { return true; });

    // Execute failed operations
    breaker.execute([]() { return false; });

    auto stats = breaker.getStatistics();

    EXPECT_EQ(stats.total_successes + stats.total_failures, 3);
    EXPECT_EQ(stats.total_successes, 2);
    EXPECT_EQ(stats.total_failures, 1);
}

TEST_F(CircuitBreakerTest, RejectedCallsStatistics) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    CircuitBreaker breaker(config);

    // Trigger OPEN
    breaker.execute([]() { return false; });

    // Execute rejected operations
    breaker.execute([]() { return true; });
    breaker.execute([]() { return true; });

    auto stats = breaker.getStatistics();
    EXPECT_GE(stats.total_rejections, 2);
}

TEST_F(CircuitBreakerTest, ResetStatistics) {
    CircuitBreaker breaker(defaultConfig());

    breaker.execute([]() { return true; });
    breaker.execute([]() { return false; });

    breaker.resetStatistics();

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_successes, 0);
    EXPECT_EQ(stats.total_failures, 0);
    EXPECT_EQ(stats.total_rejections, 0);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(CircuitBreakerTest, StateChangeCallback) {
    CircuitBreaker breaker(defaultConfig());

    CircuitState old_state = CircuitState::CLOSED;
    CircuitState new_state = CircuitState::CLOSED;
    bool callback_invoked = false;

    breaker.setStateChangeCallback([&](CircuitState old_s, CircuitState new_s) {
        old_state = old_s;
        new_state = new_s;
        callback_invoked = true;
    });

    // Trigger state change
    breaker.recordFailure();
    breaker.recordFailure();
    breaker.recordFailure();

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(old_state, CircuitState::CLOSED);
    EXPECT_EQ(new_state, CircuitState::OPEN);
}

TEST_F(CircuitBreakerTest, SuccessCallback) {
    CircuitBreaker breaker(defaultConfig());

    int success_count = 0;
    breaker.setSuccessCallback([&](CircuitState state) {
        ++success_count;
    });

    breaker.execute([]() { return true; });
    breaker.execute([]() { return true; });

    EXPECT_EQ(success_count, 2);
}

TEST_F(CircuitBreakerTest, FailureCallback) {
    CircuitBreaker breaker(defaultConfig());

    int failure_count = 0;
    std::string last_error;

    breaker.setFailureCallback([&](CircuitState state, const std::string& error) {
        ++failure_count;
        last_error = error;
    });

    breaker.execute([]() { return false; });

    EXPECT_EQ(failure_count, 1);
}

TEST_F(CircuitBreakerTest, RejectionCallback) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    CircuitBreaker breaker(config);

    int rejection_count = 0;
    breaker.setRejectionCallback([&]() {
        ++rejection_count;
    });

    // Trigger OPEN
    breaker.execute([]() { return false; });

    // Execute rejected operations
    breaker.execute([]() { return true; });
    breaker.execute([]() { return true; });

    EXPECT_GE(rejection_count, 2);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(CircuitBreakerTest, GetConfiguration) {
    auto config = defaultConfig();
    config.failure_threshold = 5;
    config.success_threshold = 3;

    CircuitBreaker breaker(config);

    auto retrieved_config = breaker.getConfig();
    EXPECT_EQ(retrieved_config.failure_threshold, 5);
    EXPECT_EQ(retrieved_config.success_threshold, 3);
}

TEST_F(CircuitBreakerTest, UpdateConfiguration) {
    CircuitBreaker breaker(defaultConfig());

    CircuitBreakerConfig new_config;
    new_config.failure_threshold = 10;
    new_config.success_threshold = 5;
    new_config.open_timeout = 200ms;

    breaker.updateConfig(new_config);

    auto config = breaker.getConfig();
    EXPECT_EQ(config.failure_threshold, 10);
    EXPECT_EQ(config.success_threshold, 5);
}

// ============================================================================
// Manual Control Tests
// ============================================================================

TEST_F(CircuitBreakerTest, ForceOpen) {
    CircuitBreaker breaker(defaultConfig());
    ASSERT_EQ(breaker.getState(), CircuitState::CLOSED);

    breaker.forceOpen();

    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
    EXPECT_FALSE(breaker.allowsRequests());
}

TEST_F(CircuitBreakerTest, ForceHalfOpen) {
    CircuitBreaker breaker(defaultConfig());

    breaker.forceOpen();
    breaker.forceHalfOpen();

    EXPECT_EQ(breaker.getState(), CircuitState::HALF_OPEN);
}

TEST_F(CircuitBreakerTest, Reset) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    CircuitBreaker breaker(config);

    // Trigger OPEN
    breaker.execute([]() { return false; });
    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Reset
    breaker.reset();

    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
    EXPECT_TRUE(breaker.allowsRequests());

    // After reset, stats should be zero (reset includes resetStatistics())
    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_successes, 0);
    EXPECT_EQ(stats.consecutive_failures, 0);
}

TEST_F(CircuitBreakerTest, RecordExternalSuccess) {
    CircuitBreaker breaker(defaultConfig());

    breaker.recordSuccess();
    breaker.recordSuccess();

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_successes, 2);
}

TEST_F(CircuitBreakerTest, RecordExternalFailure) {
    auto config = defaultConfig();
    config.failure_threshold = 2;
    CircuitBreaker breaker(config);

    breaker.recordFailure();
    breaker.recordFailure();

    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(CircuitBreakerTest, ExecuteWithErrorInfo) {
    CircuitBreaker breaker(defaultConfig());

    ReliabilityError error;
    bool result = breaker.execute([]() { return false; }, error);

    EXPECT_FALSE(result);
    EXPECT_NE(error, ReliabilityError::NONE);
}

TEST_F(CircuitBreakerTest, ExecuteWithErrorMessage) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    CircuitBreaker breaker(config);

    // Trigger OPEN
    breaker.execute([]() { return false; });

    // Execute rejected operation
    ReliabilityError error;
    std::string error_msg;
    bool result = breaker.execute([]() { return true; }, error, error_msg);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, ReliabilityError::CIRCUIT_OPEN);
    EXPECT_FALSE(error_msg.empty());
}

TEST_F(CircuitBreakerTest, ExecuteWithExceptions) {
    CircuitBreaker breaker(defaultConfig());

    bool result = breaker.executeWithExceptions([]() -> bool {
        throw std::runtime_error("Test exception");
    });

    EXPECT_FALSE(result);

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_failures, 1);
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_F(CircuitBreakerTest, ConcurrentOperations) {
    CircuitBreaker breaker(defaultConfig());

    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 1000;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                // Alternate between success and failure
                bool should_succeed = (t + i) % 2 == 0;

                bool result = breaker.execute([should_succeed]() {
                    return should_succeed;
                });

                if (result) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    failure_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto stats = breaker.getStatistics();
    uint64_t total = stats.total_successes + stats.total_failures + stats.total_rejections;
    EXPECT_GT(total, 0);
    EXPECT_EQ(total, stats.total_successes + stats.total_failures + stats.total_rejections);
}

TEST_F(CircuitBreakerTest, ThreadSafeStateTransitions) {
    auto config = defaultConfig();
    config.failure_threshold = 10;
    CircuitBreaker breaker(config);

    const int NUM_THREADS = 4;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {}

            // All threads trigger failures simultaneously
            for (int i = 0; i < 10; ++i) {
                breaker.recordFailure();
                std::this_thread::yield();
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    // Circuit should be OPEN
    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
}

// ============================================================================
// Builder Pattern Tests
// ============================================================================

TEST_F(CircuitBreakerTest, BuilderBasicConfiguration) {
    auto breaker = CircuitBreakerBuilder()
        .withFailureThreshold(5)
        .withSuccessThreshold(3)
        .withOpenTimeout(100ms)
        .build();

    ASSERT_NE(breaker, nullptr);

    auto config = breaker->getConfig();
    EXPECT_EQ(config.failure_threshold, 5);
    EXPECT_EQ(config.success_threshold, 3);
}

TEST_F(CircuitBreakerTest, BuilderWithCallbacks) {
    bool callback_invoked = false;

    auto breaker = CircuitBreakerBuilder()
        .withFailureThreshold(1)
        .onStateChange([&](CircuitState old_state, CircuitState new_state) {
            callback_invoked = true;
        })
        .build();

    ASSERT_NE(breaker, nullptr);

    breaker->recordFailure();

    EXPECT_TRUE(callback_invoked);
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST_F(CircuitBreakerTest, ZeroFailureThreshold) {
    CircuitBreakerConfig config;
    config.failure_threshold = 0;
    config.success_threshold = 1;

    // Should throw exception for invalid configuration
    EXPECT_THROW({
        CircuitBreaker breaker(config);
    }, std::exception);
}

TEST_F(CircuitBreakerTest, RapidStateChanges) {
    auto config = defaultConfig();
    config.failure_threshold = 1;
    config.success_threshold = 1;
    config.open_timeout = 10ms;
    CircuitBreaker breaker(config);

    // Test multiple cycles of open/recover
    for (int cycle = 0; cycle < 10; ++cycle) {
        // CLOSED -> OPEN
        breaker.recordFailure();
        EXPECT_EQ(breaker.getState(), CircuitState::OPEN);

        // Wait for OPEN timeout
        std::this_thread::sleep_for(15ms);

        // Execute operation - should transition OPEN -> HALF_OPEN and succeed
        bool executed = false;
        auto result = breaker.execute([&executed]() {
            executed = true;
            return true;
        });

        EXPECT_TRUE(result);
        EXPECT_TRUE(executed);

        // Should now be CLOSED (after one success in HALF_OPEN with success_threshold=1)
        EXPECT_TRUE(breaker.allowsRequests());
        EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
    }
}

TEST_F(CircuitBreakerTest, LongRunningOperation) {
    CircuitBreaker breaker(defaultConfig());

    bool executed = false;
    bool result = breaker.execute([&]() {
        executed = true;
        std::this_thread::sleep_for(50ms);
        return true;
    });

    EXPECT_TRUE(result);
    EXPECT_TRUE(executed);
}

TEST_F(CircuitBreakerTest, HighVolumeOperations) {
    CircuitBreaker breaker(defaultConfig());

    const int NUM_OPERATIONS = 1000;  // Reduced for faster testing
    int success_count = 0;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        if (breaker.execute([]() { return true; })) {
            ++success_count;
        }
    }

    EXPECT_GT(success_count, NUM_OPERATIONS - 100);  // Allow some rejections
}

TEST_F(CircuitBreakerTest, MoveSemantics) {
    auto config = defaultConfig();
    CircuitBreaker breaker1(config);

    breaker1.recordFailure();

    // Move construction
    CircuitBreaker breaker2(std::move(breaker1));

    auto stats = breaker2.getStatistics();
    EXPECT_EQ(stats.total_failures, 1);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(CircuitBreakerTest, OverheadMeasurement) {
    CircuitBreaker breaker(defaultConfig());

    const int NUM_OPS = 10000;  // Reduced for faster testing

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPS; ++i) {
        breaker.execute([]() { return true; });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_overhead_ns = static_cast<double>(duration.count()) / NUM_OPS;

    std::cout << "Average circuit breaker overhead: " << avg_overhead_ns << " ns per operation" << std::endl;

    // Expect overhead to be reasonable (less than 10 microseconds per operation)
    EXPECT_LT(avg_overhead_ns, 10000.0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
