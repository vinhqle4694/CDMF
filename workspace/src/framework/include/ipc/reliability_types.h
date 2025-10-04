#ifndef CDMF_IPC_RELIABILITY_TYPES_H
#define CDMF_IPC_RELIABILITY_TYPES_H

#include <chrono>
#include <cstdint>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Retry strategy types for failure recovery
 */
enum class RetryStrategy {
    /**
     * @brief Constant delay between retry attempts
     * delay = initial_delay
     */
    CONSTANT,

    /**
     * @brief Linear increase in delay between retry attempts
     * delay = initial_delay + (attempt * increment)
     */
    LINEAR,

    /**
     * @brief Exponential backoff delay between retry attempts
     * delay = initial_delay * (backoff_multiplier ^ attempt)
     */
    EXPONENTIAL,

    /**
     * @brief Exponential backoff with decorrelated jitter
     * Prevents thundering herd problem
     * delay = random(initial_delay, prev_delay * 3)
     */
    EXPONENTIAL_JITTER
};

/**
 * @brief Circuit breaker states following the state machine pattern
 */
enum class CircuitState {
    /**
     * @brief Normal operation, all requests allowed
     * Transitions to OPEN when failure threshold is exceeded
     */
    CLOSED,

    /**
     * @brief Circuit is open, all requests fail fast without attempting
     * Transitions to HALF_OPEN after timeout period
     */
    OPEN,

    /**
     * @brief Testing if the service has recovered
     * Limited number of requests allowed through
     * Transitions to CLOSED if success threshold met, or OPEN if failures continue
     */
    HALF_OPEN
};

/**
 * @brief Configuration for retry policy behavior
 */
struct RetryConfig {
    /**
     * @brief Maximum number of retry attempts (0 = no retries)
     */
    uint32_t max_retries = 3;

    /**
     * @brief Initial delay before first retry attempt
     */
    std::chrono::milliseconds initial_delay{100};

    /**
     * @brief Maximum delay between retry attempts (cap for exponential growth)
     */
    std::chrono::milliseconds max_delay{10000};

    /**
     * @brief Timeout for each individual retry attempt
     */
    std::chrono::milliseconds timeout_per_attempt{5000};

    /**
     * @brief Retry strategy to use
     */
    RetryStrategy strategy = RetryStrategy::EXPONENTIAL;

    /**
     * @brief Multiplier for exponential backoff (default 2.0 = double each time)
     */
    double backoff_multiplier = 2.0;

    /**
     * @brief Linear increment for LINEAR strategy (in milliseconds)
     */
    uint32_t linear_increment_ms = 100;

    /**
     * @brief Whether to add random jitter to prevent thundering herd
     * Jitter is ±20% of computed delay
     */
    bool enable_jitter = true;

    /**
     * @brief Whether to retry on timeout errors
     */
    bool retry_on_timeout = true;

    /**
     * @brief Whether to retry on connection refused errors
     */
    bool retry_on_connection_refused = true;

    /**
     * @brief Whether to retry on temporary network errors (EAGAIN, EWOULDBLOCK)
     */
    bool retry_on_temp_error = true;
};

/**
 * @brief Configuration for circuit breaker behavior
 */
struct CircuitBreakerConfig {
    /**
     * @brief Number of consecutive failures before opening circuit
     */
    uint32_t failure_threshold = 5;

    /**
     * @brief Number of consecutive successes in HALF_OPEN to close circuit
     */
    uint32_t success_threshold = 2;

    /**
     * @brief Time to wait before attempting recovery (OPEN -> HALF_OPEN)
     */
    std::chrono::milliseconds open_timeout{30000};

    /**
     * @brief Timeout for operations in HALF_OPEN state
     */
    std::chrono::milliseconds half_open_timeout{5000};

    /**
     * @brief Rolling window size for failure rate calculation (in requests)
     * If 0, uses consecutive failure counting instead
     */
    uint32_t rolling_window_size = 0;

    /**
     * @brief Failure rate threshold (0.0 - 1.0) when using rolling window
     * Circuit opens if (failures / total_requests) >= threshold
     */
    double failure_rate_threshold = 0.5;

    /**
     * @brief Minimum number of requests before calculating failure rate
     * Prevents opening circuit due to small sample sizes
     */
    uint32_t minimum_request_threshold = 10;
};

/**
 * @brief Result of a retry attempt
 */
enum class RetryResult {
    /**
     * @brief Operation succeeded
     */
    SUCCESS,

    /**
     * @brief Operation failed but can be retried
     */
    RETRY,

    /**
     * @brief Operation failed and should not be retried
     */
    FATAL_ERROR,

    /**
     * @brief Operation exceeded maximum retry attempts
     */
    MAX_RETRIES_EXCEEDED,

    /**
     * @brief Operation timed out
     */
    TIMEOUT
};

/**
 * @brief Statistics for retry operations
 */
struct RetryStats {
    /**
     * @brief Total number of operations attempted
     */
    uint64_t total_attempts = 0;

    /**
     * @brief Number of operations that succeeded on first try
     */
    uint64_t first_try_successes = 0;

    /**
     * @brief Number of operations that succeeded after retries
     */
    uint64_t retry_successes = 0;

    /**
     * @brief Number of operations that failed after all retries
     */
    uint64_t total_failures = 0;

    /**
     * @brief Total time spent in retry delays
     */
    std::chrono::milliseconds total_retry_delay{0};

    /**
     * @brief Average number of retries for successful operations
     */
    double avg_retries_on_success = 0.0;

    /**
     * @brief Reset all statistics to zero
     */
    void reset() {
        total_attempts = 0;
        first_try_successes = 0;
        retry_successes = 0;
        total_failures = 0;
        total_retry_delay = std::chrono::milliseconds{0};
        avg_retries_on_success = 0.0;
    }
};

/**
 * @brief Statistics for circuit breaker operations
 */
struct CircuitBreakerStats {
    /**
     * @brief Current circuit state
     */
    CircuitState current_state = CircuitState::CLOSED;

    /**
     * @brief Total number of successful requests
     */
    uint64_t total_successes = 0;

    /**
     * @brief Total number of failed requests
     */
    uint64_t total_failures = 0;

    /**
     * @brief Total number of rejected requests (while circuit OPEN)
     */
    uint64_t total_rejections = 0;

    /**
     * @brief Number of times circuit has opened
     */
    uint64_t open_count = 0;

    /**
     * @brief Number of times circuit has transitioned to half-open
     */
    uint64_t half_open_count = 0;

    /**
     * @brief Number of times circuit has closed after recovery
     */
    uint64_t close_count = 0;

    /**
     * @brief Time when circuit was last opened
     */
    std::chrono::steady_clock::time_point last_open_time;

    /**
     * @brief Time when circuit was last closed
     */
    std::chrono::steady_clock::time_point last_close_time;

    /**
     * @brief Consecutive failures in current state
     */
    uint32_t consecutive_failures = 0;

    /**
     * @brief Consecutive successes in current state
     */
    uint32_t consecutive_successes = 0;

    /**
     * @brief Current failure rate (only if rolling window enabled)
     */
    double current_failure_rate = 0.0;

    /**
     * @brief Reset all statistics to zero
     */
    void reset() {
        current_state = CircuitState::CLOSED;
        total_successes = 0;
        total_failures = 0;
        total_rejections = 0;
        open_count = 0;
        half_open_count = 0;
        close_count = 0;
        consecutive_failures = 0;
        consecutive_successes = 0;
        current_failure_rate = 0.0;
    }
};

/**
 * @brief Error codes specific to reliability mechanisms
 */
enum class ReliabilityError {
    /**
     * @brief No error
     */
    NONE = 0,

    /**
     * @brief Circuit breaker is open, request rejected
     */
    CIRCUIT_OPEN,

    /**
     * @brief Maximum retry attempts exceeded
     */
    MAX_RETRIES_EXCEEDED,

    /**
     * @brief Operation timed out
     */
    TIMEOUT,

    /**
     * @brief Invalid configuration
     */
    INVALID_CONFIG,

    /**
     * @brief Operation cancelled
     */
    CANCELLED
};

/**
 * @brief Convert ReliabilityError to string description
 */
inline const char* to_string(ReliabilityError error) {
    switch (error) {
        case ReliabilityError::NONE: return "No error";
        case ReliabilityError::CIRCUIT_OPEN: return "Circuit breaker is open";
        case ReliabilityError::MAX_RETRIES_EXCEEDED: return "Maximum retry attempts exceeded";
        case ReliabilityError::TIMEOUT: return "Operation timed out";
        case ReliabilityError::INVALID_CONFIG: return "Invalid configuration";
        case ReliabilityError::CANCELLED: return "Operation cancelled";
        default: return "Unknown error";
    }
}

/**
 * @brief Convert CircuitState to string
 */
inline const char* to_string(CircuitState state) {
    switch (state) {
        case CircuitState::CLOSED: return "CLOSED";
        case CircuitState::OPEN: return "OPEN";
        case CircuitState::HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert RetryStrategy to string
 */
inline const char* to_string(RetryStrategy strategy) {
    switch (strategy) {
        case RetryStrategy::CONSTANT: return "CONSTANT";
        case RetryStrategy::LINEAR: return "LINEAR";
        case RetryStrategy::EXPONENTIAL: return "EXPONENTIAL";
        case RetryStrategy::EXPONENTIAL_JITTER: return "EXPONENTIAL_JITTER";
        default: return "UNKNOWN";
    }
}

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_RELIABILITY_TYPES_H
