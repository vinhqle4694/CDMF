#include "ipc/circuit_breaker.h"
#include "ipc/reliability_types.h"
#include "ipc/retry_policy.h"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace cdmf::ipc;

// ============================================================================
// RetryPolicy Tests
// ============================================================================

class RetryPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        attempt_count = 0;
        success_on_attempt = 0;
    }

    std::atomic<uint32_t> attempt_count{0};
    uint32_t success_on_attempt = 0;
};

TEST_F(RetryPolicyTest, SuccessOnFirstAttempt) {
    RetryConfig config;
    config.max_retries = 3;

    RetryPolicy policy(config);

    auto result = policy.execute([&]() -> bool {
        attempt_count++;
        return true; // Success on first attempt
    });

    EXPECT_EQ(result, RetryResult::SUCCESS);
    EXPECT_EQ(attempt_count, 1);

    auto stats = policy.getStatistics();
    EXPECT_EQ(stats.total_attempts, 1);
    EXPECT_EQ(stats.first_try_successes, 1);
    EXPECT_EQ(stats.retry_successes, 0);
    EXPECT_EQ(stats.total_failures, 0);
}

TEST_F(RetryPolicyTest, SuccessAfterRetries) {
    RetryConfig config;
    config.max_retries = 5;
    config.initial_delay = std::chrono::milliseconds(10);
    success_on_attempt = 3;

    RetryPolicy policy(config);

    auto result = policy.execute([&]() -> bool {
        attempt_count++;
        return attempt_count >= success_on_attempt;
    });

    EXPECT_EQ(result, RetryResult::SUCCESS);
    EXPECT_EQ(attempt_count, success_on_attempt);

    auto stats = policy.getStatistics();
    EXPECT_EQ(stats.total_attempts, 1);
    EXPECT_EQ(stats.first_try_successes, 0);
    EXPECT_EQ(stats.retry_successes, 1);
    EXPECT_EQ(stats.total_failures, 0);
}

TEST_F(RetryPolicyTest, MaxRetriesExceeded) {
    RetryConfig config;
    config.max_retries = 3;
    config.initial_delay = std::chrono::milliseconds(10);

    RetryPolicy policy(config);

    auto result = policy.execute([&]() -> bool {
        attempt_count++;
        return false; // Always fail
    });

    EXPECT_EQ(result, RetryResult::MAX_RETRIES_EXCEEDED);
    EXPECT_EQ(attempt_count, 4); // 1 initial + 3 retries

    auto stats = policy.getStatistics();
    EXPECT_EQ(stats.total_attempts, 1);
    EXPECT_EQ(stats.total_failures, 1);
}

TEST_F(RetryPolicyTest, ConstantDelayStrategy) {
    RetryConfig config;
    config.max_retries = 3;
    config.strategy = RetryStrategy::CONSTANT;
    config.initial_delay = std::chrono::milliseconds(100);
    config.enable_jitter = false;

    RetryPolicy policy(config);

    for (uint32_t i = 1; i <= 3; i++) {
        auto delay = policy.calculateDelay(i);
        EXPECT_EQ(delay, std::chrono::milliseconds(100));
    }
}

TEST_F(RetryPolicyTest, LinearBackoffStrategy) {
    RetryConfig config;
    config.max_retries = 5;
    config.strategy = RetryStrategy::LINEAR;
    config.initial_delay = std::chrono::milliseconds(100);
    config.linear_increment_ms = 50;
    config.enable_jitter = false;

    RetryPolicy policy(config);

    EXPECT_EQ(policy.calculateDelay(1), std::chrono::milliseconds(100)); // 100 + 0*50
    EXPECT_EQ(policy.calculateDelay(2), std::chrono::milliseconds(150)); // 100 + 1*50
    EXPECT_EQ(policy.calculateDelay(3), std::chrono::milliseconds(200)); // 100 + 2*50
    EXPECT_EQ(policy.calculateDelay(4), std::chrono::milliseconds(250)); // 100 + 3*50
}

TEST_F(RetryPolicyTest, ExponentialBackoffStrategy) {
    RetryConfig config;
    config.max_retries = 5;
    config.strategy = RetryStrategy::EXPONENTIAL;
    config.initial_delay = std::chrono::milliseconds(100);
    config.backoff_multiplier = 2.0;
    config.max_delay = std::chrono::milliseconds(10000);
    config.enable_jitter = false;

    RetryPolicy policy(config);

    EXPECT_EQ(policy.calculateDelay(1), std::chrono::milliseconds(100));  // 100 * 2^0
    EXPECT_EQ(policy.calculateDelay(2), std::chrono::milliseconds(200));  // 100 * 2^1
    EXPECT_EQ(policy.calculateDelay(3), std::chrono::milliseconds(400));  // 100 * 2^2
    EXPECT_EQ(policy.calculateDelay(4), std::chrono::milliseconds(800));  // 100 * 2^3
    EXPECT_EQ(policy.calculateDelay(5), std::chrono::milliseconds(1600)); // 100 * 2^4
}

TEST_F(RetryPolicyTest, MaxDelayEnforced) {
    RetryConfig config;
    config.max_retries = 10;
    config.strategy = RetryStrategy::EXPONENTIAL;
    config.initial_delay = std::chrono::milliseconds(100);
    config.backoff_multiplier = 2.0;
    config.max_delay = std::chrono::milliseconds(1000);
    config.enable_jitter = false;

    RetryPolicy policy(config);

    // After a few attempts, delay should be capped at max_delay
    auto delay = policy.calculateDelay(10);
    EXPECT_LE(delay, std::chrono::milliseconds(1000));
}

TEST_F(RetryPolicyTest, JitterAddsRandomness) {
    RetryConfig config;
    config.max_retries = 5;
    config.strategy = RetryStrategy::EXPONENTIAL;
    config.initial_delay = std::chrono::milliseconds(100);
    config.enable_jitter = true;

    RetryPolicy policy(config);

    // With jitter, delays should vary
    std::set<std::chrono::milliseconds> delays;
    for (int i = 0; i < 10; i++) {
        delays.insert(policy.calculateDelay(3));
    }

    // Should have some variation (not all the same)
    // Note: This test might occasionally fail due to randomness, but is very unlikely
    EXPECT_GT(delays.size(), 1);
}

TEST_F(RetryPolicyTest, SuccessCallback) {
    RetryConfig config;
    config.max_retries = 3;
    config.initial_delay = std::chrono::milliseconds(10);
    success_on_attempt = 2;

    RetryPolicy policy(config);

    uint32_t callback_attempt = 0;
    policy.setSuccessCallback([&](uint32_t attempt) {
        callback_attempt = attempt;
    });

    policy.execute([&]() -> bool {
        attempt_count++;
        return attempt_count >= success_on_attempt;
    });

    EXPECT_EQ(callback_attempt, success_on_attempt);
}

TEST_F(RetryPolicyTest, FailureCallback) {
    RetryConfig config;
    config.max_retries = 3;
    config.initial_delay = std::chrono::milliseconds(10);

    RetryPolicy policy(config);

    uint32_t failure_count = 0;
    policy.setFailureCallback([&](uint32_t /* attempt */, bool /* will_retry */, const std::string& /* msg */) {
        failure_count++;
    });

    policy.execute([&]() -> bool {
        return false; // Always fail
    });

    EXPECT_EQ(failure_count, 4); // 1 initial + 3 retries
}

TEST_F(RetryPolicyTest, ExceptionHandling) {
    RetryConfig config;
    config.max_retries = 2;
    config.initial_delay = std::chrono::milliseconds(10);

    RetryPolicy policy(config);

    auto result = policy.executeWithExceptions([&]() -> bool {
        attempt_count++;
        if (attempt_count < 2) {
            throw std::runtime_error("Test exception");
        }
        return true;
    });

    EXPECT_EQ(result, RetryResult::SUCCESS);
    EXPECT_EQ(attempt_count, 2);
}

TEST_F(RetryPolicyTest, RetryableErrorCheck) {
    RetryConfig config;
    RetryPolicy policy(config);

    // Retryable errors
    EXPECT_TRUE(policy.isRetryableError(EAGAIN));
    EXPECT_TRUE(policy.isRetryableError(EWOULDBLOCK));
    EXPECT_TRUE(policy.isRetryableError(EINTR));
    EXPECT_TRUE(policy.isRetryableError(ECONNREFUSED));
    EXPECT_TRUE(policy.isRetryableError(ETIMEDOUT));

    // Non-retryable errors
    EXPECT_FALSE(policy.isRetryableError(EACCES));
    EXPECT_FALSE(policy.isRetryableError(EPERM));
    EXPECT_FALSE(policy.isRetryableError(EINVAL));
}

TEST_F(RetryPolicyTest, BuilderPattern) {
    uint32_t success_calls = 0;

    auto policy = RetryPolicyBuilder()
        .withMaxRetries(5)
        .withExponentialBackoff(std::chrono::milliseconds(100),
                               std::chrono::milliseconds(5000))
        .withJitter()
        .onSuccess([&](uint32_t /* attempt */) { success_calls++; })
        .build();

    ASSERT_NE(policy, nullptr);

    policy->execute([&]() -> bool { return true; });

    EXPECT_EQ(success_calls, 1);
}

TEST_F(RetryPolicyTest, StatisticsTracking) {
    RetryConfig config;
    config.max_retries = 3;
    config.initial_delay = std::chrono::milliseconds(10);

    RetryPolicy policy(config);

    // First attempt: success on first try
    policy.execute([&]() -> bool { return true; });

    // Second attempt: success after 2 tries
    attempt_count = 0;
    policy.execute([&]() -> bool {
        attempt_count++;
        return attempt_count >= 2;
    });

    // Third attempt: failure
    policy.execute([&]() -> bool { return false; });

    auto stats = policy.getStatistics();
    EXPECT_EQ(stats.total_attempts, 3);
    EXPECT_EQ(stats.first_try_successes, 1);
    EXPECT_EQ(stats.retry_successes, 1);
    EXPECT_EQ(stats.total_failures, 1);
}

// ============================================================================
// CircuitBreaker Tests
// ============================================================================

class CircuitBreakerTest : public ::testing::Test {
protected:
    void SetUp() override {
        attempt_count = 0;
        should_succeed = false;
    }

    std::atomic<uint32_t> attempt_count{0};
    bool should_succeed = false;
};

TEST_F(CircuitBreakerTest, InitialStateClosed) {
    CircuitBreakerConfig config;
    CircuitBreaker breaker(config);

    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
    EXPECT_TRUE(breaker.allowsRequests());
}

TEST_F(CircuitBreakerTest, SuccessfulOperationsStayClosed) {
    CircuitBreakerConfig config;
    config.failure_threshold = 5;

    CircuitBreaker breaker(config);

    for (int i = 0; i < 10; i++) {
        bool result = breaker.execute([&]() -> bool {
            return true;
        });
        EXPECT_TRUE(result);
    }

    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_successes, 10);
    EXPECT_EQ(stats.total_failures, 0);
}

TEST_F(CircuitBreakerTest, FailureThresholdOpensCircuit) {
    CircuitBreakerConfig config;
    config.failure_threshold = 5;

    CircuitBreaker breaker(config);

    // Fail 5 times to reach threshold
    for (uint32_t i = 0; i < config.failure_threshold; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
    EXPECT_FALSE(breaker.allowsRequests());

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.open_count, 1);
}

TEST_F(CircuitBreakerTest, OpenCircuitRejectsRequests) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;

    CircuitBreaker breaker(config);

    // Open the circuit
    for (uint32_t i = 0; i < config.failure_threshold; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Try to execute - should be rejected
    ReliabilityError error;
    bool result = breaker.execute([&]() -> bool {
        attempt_count++;
        return true;
    }, error);

    EXPECT_FALSE(result);
    EXPECT_EQ(error, ReliabilityError::CIRCUIT_OPEN);
    EXPECT_EQ(attempt_count, 0); // Operation should not be executed

    auto stats = breaker.getStatistics();
    EXPECT_GT(stats.total_rejections, 0);
}

TEST_F(CircuitBreakerTest, OpenToHalfOpenTransition) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;
    config.open_timeout = std::chrono::milliseconds(100);

    CircuitBreaker breaker(config);

    // Open the circuit
    for (uint32_t i = 0; i < config.failure_threshold; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Next request should transition to HALF_OPEN
    breaker.execute([&]() -> bool { return true; });

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.half_open_count, 1);
}

TEST_F(CircuitBreakerTest, HalfOpenSuccessClosesCircuit) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;
    config.success_threshold = 2;
    config.open_timeout = std::chrono::milliseconds(50);

    CircuitBreaker breaker(config);

    // Open the circuit
    for (uint32_t i = 0; i < config.failure_threshold; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    // Wait and transition to HALF_OPEN
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // First successful request in HALF_OPEN
    breaker.execute([&]() -> bool { return true; });

    // State should still be HALF_OPEN (need 2 successes)
    auto state_after_first = breaker.getState();
    EXPECT_EQ(state_after_first, CircuitState::HALF_OPEN);

    // Second successful request
    breaker.execute([&]() -> bool { return true; });

    // Should transition to CLOSED
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);

    auto stats = breaker.getStatistics();
    EXPECT_GT(stats.close_count, 0);
}

TEST_F(CircuitBreakerTest, HalfOpenFailureReopensCircuit) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;
    config.open_timeout = std::chrono::milliseconds(50);

    CircuitBreaker breaker(config);

    // Open the circuit
    for (uint32_t i = 0; i < config.failure_threshold; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    uint32_t initial_open_count = breaker.getStatistics().open_count;

    // Wait and transition to HALF_OPEN
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    breaker.execute([&]() -> bool { return true; });

    ASSERT_EQ(breaker.getState(), CircuitState::HALF_OPEN);

    // Fail in HALF_OPEN - should reopen
    breaker.execute([&]() -> bool { return false; });

    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.open_count, initial_open_count + 1);
}

TEST_F(CircuitBreakerTest, StateChangeCallback) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;

    CircuitBreaker breaker(config);

    std::vector<std::pair<CircuitState, CircuitState>> transitions;
    breaker.setStateChangeCallback([&](CircuitState old_state, CircuitState new_state) {
        transitions.emplace_back(old_state, new_state);
    });

    // Trigger state change to OPEN
    for (uint32_t i = 0; i < config.failure_threshold; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    ASSERT_EQ(transitions.size(), 1);
    EXPECT_EQ(transitions[0].first, CircuitState::CLOSED);
    EXPECT_EQ(transitions[0].second, CircuitState::OPEN);
}

TEST_F(CircuitBreakerTest, SuccessAndFailureCallbacks) {
    CircuitBreakerConfig config;
    CircuitBreaker breaker(config);

    uint32_t success_count = 0;
    uint32_t failure_count = 0;

    breaker.setSuccessCallback([&](CircuitState /* state */) { success_count++; });
    breaker.setFailureCallback([&](CircuitState /* state */, const std::string& /* msg */) { failure_count++; });

    breaker.execute([&]() -> bool { return true; });
    breaker.execute([&]() -> bool { return false; });

    EXPECT_EQ(success_count, 1);
    EXPECT_EQ(failure_count, 1);
}

TEST_F(CircuitBreakerTest, RejectionCallback) {
    CircuitBreakerConfig config;
    config.failure_threshold = 2;

    CircuitBreaker breaker(config);

    uint32_t rejection_count = 0;
    breaker.setRejectionCallback([&]() { rejection_count++; });

    // Open the circuit
    breaker.execute([&]() -> bool { return false; });
    breaker.execute([&]() -> bool { return false; });

    ASSERT_EQ(breaker.getState(), CircuitState::OPEN);

    // Try to execute - should be rejected
    breaker.execute([&]() -> bool { return true; });

    EXPECT_GT(rejection_count, 0);
}

TEST_F(CircuitBreakerTest, ForceOpenAndReset) {
    CircuitBreakerConfig config;
    CircuitBreaker breaker(config);

    // Force open
    breaker.forceOpen();
    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);

    // Reset to closed
    breaker.reset();
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
}

TEST_F(CircuitBreakerTest, ExceptionHandling) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;

    CircuitBreaker breaker(config);

    uint32_t exception_count = 0;

    // Execute with exceptions - should treat as failures
    for (int i = 0; i < 3; i++) {
        bool result = breaker.executeWithExceptions([&]() -> bool {
            exception_count++;
            throw std::runtime_error("Test exception");
            return true;
        });
        EXPECT_FALSE(result);
    }

    EXPECT_EQ(exception_count, 3);
    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);
}

TEST_F(CircuitBreakerTest, RollingWindowFailureRate) {
    CircuitBreakerConfig config;
    config.rolling_window_size = 10;
    config.failure_rate_threshold = 0.5; // 50% failure rate
    config.minimum_request_threshold = 10;

    CircuitBreaker breaker(config);

    // Send 10 requests: 6 failures (60% > 50% threshold)
    for (int i = 0; i < 10; i++) {
        breaker.execute([&]() -> bool { return i >= 6; }); // Fail first 6
    }

    // Circuit should be open due to high failure rate
    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);

    auto stats = breaker.getStatistics();
    EXPECT_GE(stats.current_failure_rate, 0.5);
}

TEST_F(CircuitBreakerTest, MinimumRequestThreshold) {
    CircuitBreakerConfig config;
    config.rolling_window_size = 100;
    config.failure_rate_threshold = 0.5;
    config.minimum_request_threshold = 10;

    CircuitBreaker breaker(config);

    // Send only 5 requests, all failures
    // Should not open circuit due to minimum threshold
    for (int i = 0; i < 5; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    // Circuit should still be closed (not enough requests)
    EXPECT_EQ(breaker.getState(), CircuitState::CLOSED);
}

TEST_F(CircuitBreakerTest, RecordSuccessAndFailure) {
    CircuitBreakerConfig config;
    config.failure_threshold = 3;

    CircuitBreaker breaker(config);

    // Record failures externally
    breaker.recordFailure();
    breaker.recordFailure();
    breaker.recordFailure();

    EXPECT_EQ(breaker.getState(), CircuitState::OPEN);

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_failures, 3);
}

TEST_F(CircuitBreakerTest, BuilderPattern) {
    std::vector<std::pair<CircuitState, CircuitState>> transitions;

    auto breaker = CircuitBreakerBuilder()
        .withFailureThreshold(5)
        .withSuccessThreshold(2)
        .withOpenTimeout(std::chrono::milliseconds(100))
        .onStateChange([&](CircuitState old_state, CircuitState new_state) {
            transitions.emplace_back(old_state, new_state);
        })
        .build();

    ASSERT_NE(breaker, nullptr);

    // Trigger state change
    for (int i = 0; i < 5; i++) {
        breaker->execute([&]() -> bool { return false; });
    }

    EXPECT_EQ(breaker->getState(), CircuitState::OPEN);
    EXPECT_EQ(transitions.size(), 1);
}

TEST_F(CircuitBreakerTest, StatisticsTracking) {
    CircuitBreakerConfig config;
    config.failure_threshold = 5;
    config.success_threshold = 2;
    config.open_timeout = std::chrono::milliseconds(50);

    CircuitBreaker breaker(config);

    // 3 successes
    for (int i = 0; i < 3; i++) {
        breaker.execute([&]() -> bool { return true; });
    }

    // 5 failures (opens circuit)
    for (int i = 0; i < 5; i++) {
        breaker.execute([&]() -> bool { return false; });
    }

    // 2 rejections while open
    breaker.execute([&]() -> bool { return true; });
    breaker.execute([&]() -> bool { return true; });

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_successes, 3);
    EXPECT_EQ(stats.total_failures, 5);
    EXPECT_EQ(stats.total_rejections, 2);
    EXPECT_EQ(stats.open_count, 1);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(ReliabilityIntegrationTest, RetryPolicyWithCircuitBreaker) {
    // Simulate a scenario where retry policy is used with circuit breaker

    CircuitBreakerConfig cb_config;
    cb_config.failure_threshold = 5;
    CircuitBreaker breaker(cb_config);

    RetryConfig retry_config;
    retry_config.max_retries = 3;
    retry_config.initial_delay = std::chrono::milliseconds(10);
    RetryPolicy policy(retry_config);

    uint32_t total_attempts = 0;

    // Execute operation with both retry and circuit breaker
    auto result = policy.execute([&]() -> bool {
        total_attempts++;

        // Use circuit breaker for the actual operation
        return breaker.execute([&]() -> bool {
            // Simulate flaky operation that succeeds on 3rd attempt
            static uint32_t op_count = 0;
            op_count++;
            return op_count >= 3;
        });
    });

    EXPECT_EQ(result, RetryResult::SUCCESS);
}

TEST(ReliabilityIntegrationTest, ConcurrentCircuitBreakerAccess) {
    CircuitBreakerConfig config;
    config.failure_threshold = 100;
    CircuitBreaker breaker(config);

    std::atomic<uint32_t> success_count{0};
    std::atomic<uint32_t> failure_count{0};

    auto worker = [&]() {
        for (int i = 0; i < 100; i++) {
            bool result = breaker.execute([&]() -> bool {
                return (i % 2 == 0); // 50% success rate
            });

            if (result) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    // Run 4 threads concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = breaker.getStatistics();
    EXPECT_EQ(stats.total_successes + stats.total_failures + stats.total_rejections, 400);
}

TEST(ReliabilityIntegrationTest, ConcurrentRetryPolicyAccess) {
    RetryConfig config;
    config.max_retries = 3;
    config.initial_delay = std::chrono::milliseconds(5);
    RetryPolicy policy(config);

    std::atomic<uint32_t> total_operations{0};

    auto worker = [&]() {
        for (int i = 0; i < 50; i++) {
            policy.execute([&]() -> bool {
                total_operations++;
                return (i % 3 == 0); // Some will succeed, some will fail
            });
        }
    };

    // Run 4 threads concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = policy.getStatistics();
    EXPECT_EQ(stats.total_attempts, 200); // 50 * 4
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(ReliabilityPerformanceTest, RetryPolicyLowOverhead) {
    RetryConfig config;
    config.max_retries = 0; // No retries, just measure overhead
    RetryPolicy policy(config);

    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        policy.execute([&]() -> bool { return true; });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average should be less than 20 microseconds per operation (relaxed for Docker/CI)
    double avg_us = static_cast<double>(duration.count()) / iterations;
    EXPECT_LT(avg_us, 20.0);

    std::cout << "RetryPolicy average overhead: " << avg_us << " µs" << std::endl;
}

TEST(ReliabilityPerformanceTest, CircuitBreakerLowOverhead) {
    CircuitBreakerConfig config;
    config.failure_threshold = 1000000; // High threshold to stay closed
    CircuitBreaker breaker(config);

    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        breaker.execute([&]() -> bool { return true; });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average should be less than 20 microseconds per operation (relaxed for Docker/CI)
    double avg_us = static_cast<double>(duration.count()) / iterations;
    EXPECT_LT(avg_us, 20.0);

    std::cout << "CircuitBreaker average overhead: " << avg_us << " µs" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
