#ifndef CDMF_IPC_CIRCUIT_BREAKER_H
#define CDMF_IPC_CIRCUIT_BREAKER_H

#include "reliability_types.h"
#include <functional>
#include <memory>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Circuit breaker implementation following the state machine pattern
 *
 * The circuit breaker prevents cascading failures by failing fast when a
 * service is unavailable. It implements three states:
 *
 * - CLOSED: Normal operation, all requests are allowed through. Transitions
 *           to OPEN when failure threshold is exceeded.
 *
 * - OPEN: All requests fail immediately without attempting the operation.
 *         Transitions to HALF_OPEN after a timeout period.
 *
 * - HALF_OPEN: Testing phase. A limited number of requests are allowed through
 *              to test if the service has recovered. Transitions to CLOSED if
 *              success threshold is met, or back to OPEN if failures continue.
 *
 * Thread-safe for concurrent use across multiple threads.
 *
 * Example usage:
 * @code
 * CircuitBreakerConfig config;
 * config.failure_threshold = 5;
 * config.success_threshold = 2;
 * config.open_timeout = std::chrono::seconds(30);
 *
 * CircuitBreaker breaker(config);
 *
 * auto result = breaker.execute([&]() -> bool {
 *     return attempt_operation();
 * });
 *
 * if (result) {
 *     // Operation succeeded
 * }
 * @endcode
 */
class CircuitBreaker {
public:
    /**
     * @brief Callback type for state change events
     * @param old_state Previous circuit state
     * @param new_state New circuit state
     */
    using StateChangeCallback = std::function<void(CircuitState old_state,
                                                    CircuitState new_state)>;

    /**
     * @brief Callback type for operation success events
     * @param state Current circuit state
     */
    using SuccessCallback = std::function<void(CircuitState state)>;

    /**
     * @brief Callback type for operation failure events
     * @param state Current circuit state
     * @param error_message Optional error message
     */
    using FailureCallback = std::function<void(CircuitState state,
                                                const std::string& error_message)>;

    /**
     * @brief Callback type for rejection events (when circuit is OPEN)
     */
    using RejectionCallback = std::function<void()>;

    /**
     * @brief Construct circuit breaker with configuration
     * @param config Circuit breaker configuration parameters
     */
    explicit CircuitBreaker(const CircuitBreakerConfig& config);

    /**
     * @brief Destructor
     */
    ~CircuitBreaker();

    // Prevent copying, allow moving
    CircuitBreaker(const CircuitBreaker&) = delete;
    CircuitBreaker& operator=(const CircuitBreaker&) = delete;
    CircuitBreaker(CircuitBreaker&&) noexcept;
    CircuitBreaker& operator=(CircuitBreaker&&) noexcept;

    /**
     * @brief Execute an operation through the circuit breaker
     *
     * If the circuit is OPEN, the operation is not executed and false is returned.
     * Otherwise, the operation is executed and the result affects the circuit state.
     *
     * @param operation Function that returns true on success, false on failure
     * @return true if operation succeeded, false if it failed or was rejected
     */
    bool execute(std::function<bool()> operation);

    /**
     * @brief Execute an operation with error information
     * @param operation Function that returns true on success, false on failure
     * @param error Output parameter for error information
     * @return true if operation succeeded, false if it failed or was rejected
     */
    bool execute(std::function<bool()> operation, ReliabilityError& error);

    /**
     * @brief Execute an operation with error information and message
     * @param operation Function that returns true on success, false on failure
     * @param error Output parameter for error code
     * @param error_msg Output parameter for error message
     * @return true if operation succeeded, false if it failed or was rejected
     */
    bool execute(std::function<bool()> operation,
                 ReliabilityError& error,
                 std::string& error_msg);

    /**
     * @brief Execute an operation with exception handling
     *
     * Catches exceptions thrown by the operation and treats them as failures.
     *
     * @param operation Function that returns true on success, throws on failure
     * @return true if operation succeeded, false if it failed or was rejected
     */
    bool executeWithExceptions(std::function<bool()> operation);

    /**
     * @brief Check if the circuit allows requests (not OPEN)
     * @return true if circuit is CLOSED or HALF_OPEN
     */
    bool allowsRequests() const;

    /**
     * @brief Get the current circuit state
     * @return Current circuit state
     */
    CircuitState getState() const;

    /**
     * @brief Get current circuit breaker statistics
     * @return Copy of current statistics
     */
    CircuitBreakerStats getStatistics() const;

    /**
     * @brief Reset the circuit breaker to CLOSED state
     *
     * This manually closes the circuit and resets all counters.
     * Use with caution as it overrides the automatic state management.
     */
    void reset();

    /**
     * @brief Reset statistics counters but keep current state
     */
    void resetStatistics();

    /**
     * @brief Force circuit to OPEN state
     *
     * This manually opens the circuit. Useful for maintenance or
     * when external systems detect a problem.
     */
    void forceOpen();

    /**
     * @brief Force circuit to HALF_OPEN state
     *
     * This manually transitions to testing state.
     */
    void forceHalfOpen();

    /**
     * @brief Get the current configuration
     * @return Copy of current configuration
     */
    CircuitBreakerConfig getConfig() const;

    /**
     * @brief Update the circuit breaker configuration
     *
     * Note: State and statistics are preserved. Only thresholds and
     * timeouts are updated.
     *
     * @param config New configuration parameters
     */
    void updateConfig(const CircuitBreakerConfig& config);

    /**
     * @brief Set callback for state changes
     * @param callback Function to call on state transitions
     */
    void setStateChangeCallback(StateChangeCallback callback);

    /**
     * @brief Set callback for successful operations
     * @param callback Function to call on success
     */
    void setSuccessCallback(SuccessCallback callback);

    /**
     * @brief Set callback for failed operations
     * @param callback Function to call on failure
     */
    void setFailureCallback(FailureCallback callback);

    /**
     * @brief Set callback for rejected operations
     * @param callback Function to call on rejection
     */
    void setRejectionCallback(RejectionCallback callback);

    /**
     * @brief Record a success externally
     *
     * This allows recording successes without executing an operation.
     * Useful when the circuit breaker is used as a health tracker.
     */
    void recordSuccess();

    /**
     * @brief Record a failure externally
     *
     * This allows recording failures without executing an operation.
     * Useful when the circuit breaker is used as a health tracker.
     */
    void recordFailure();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Builder pattern for constructing CircuitBreaker with fluent API
 *
 * Example usage:
 * @code
 * auto breaker = CircuitBreakerBuilder()
 *     .withFailureThreshold(5)
 *     .withSuccessThreshold(2)
 *     .withOpenTimeout(std::chrono::seconds(30))
 *     .onStateChange([](CircuitState old_state, CircuitState new_state) {
 *         std::cout << "Circuit changed from " << to_string(old_state)
 *                   << " to " << to_string(new_state) << std::endl;
 *     })
 *     .build();
 * @endcode
 */
class CircuitBreakerBuilder {
public:
    /**
     * @brief Construct builder with default configuration
     */
    CircuitBreakerBuilder();

    /**
     * @brief Set failure threshold
     */
    CircuitBreakerBuilder& withFailureThreshold(uint32_t threshold);

    /**
     * @brief Set success threshold for recovery
     */
    CircuitBreakerBuilder& withSuccessThreshold(uint32_t threshold);

    /**
     * @brief Set timeout before attempting recovery
     */
    CircuitBreakerBuilder& withOpenTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Set timeout for operations in HALF_OPEN state
     */
    CircuitBreakerBuilder& withHalfOpenTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Enable rolling window for failure rate calculation
     * @param window_size Number of requests in the rolling window
     * @param failure_rate_threshold Failure rate (0.0-1.0) to trigger OPEN
     * @param min_requests Minimum requests before calculating rate
     */
    CircuitBreakerBuilder& withRollingWindow(uint32_t window_size,
                                             double failure_rate_threshold,
                                             uint32_t min_requests);

    /**
     * @brief Set state change callback
     */
    CircuitBreakerBuilder& onStateChange(CircuitBreaker::StateChangeCallback callback);

    /**
     * @brief Set success callback
     */
    CircuitBreakerBuilder& onSuccess(CircuitBreaker::SuccessCallback callback);

    /**
     * @brief Set failure callback
     */
    CircuitBreakerBuilder& onFailure(CircuitBreaker::FailureCallback callback);

    /**
     * @brief Set rejection callback
     */
    CircuitBreakerBuilder& onRejection(CircuitBreaker::RejectionCallback callback);

    /**
     * @brief Build the CircuitBreaker instance
     */
    std::unique_ptr<CircuitBreaker> build();

private:
    CircuitBreakerConfig config_;
    CircuitBreaker::StateChangeCallback state_change_callback_;
    CircuitBreaker::SuccessCallback success_callback_;
    CircuitBreaker::FailureCallback failure_callback_;
    CircuitBreaker::RejectionCallback rejection_callback_;
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_CIRCUIT_BREAKER_H
