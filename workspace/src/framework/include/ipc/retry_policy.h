#ifndef CDMF_IPC_RETRY_POLICY_H
#define CDMF_IPC_RETRY_POLICY_H

#include "reliability_types.h"
#include <functional>
#include <memory>
#include <random>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Retry policy implementation supporting multiple strategies
 *
 * This class implements the retry pattern with configurable strategies including:
 * - Constant delay
 * - Linear backoff
 * - Exponential backoff
 * - Exponential backoff with jitter
 *
 * The policy is thread-safe and can be shared across multiple operations.
 *
 * Example usage:
 * @code
 * RetryConfig config;
 * config.max_retries = 5;
 * config.strategy = RetryStrategy::EXPONENTIAL;
 * config.initial_delay = std::chrono::milliseconds(100);
 *
 * RetryPolicy policy(config);
 *
 * auto result = policy.execute([&]() -> bool {
 *     return attempt_connection();
 * });
 *
 * if (result == RetryResult::SUCCESS) {
 *     // Operation succeeded
 * }
 * @endcode
 */
class RetryPolicy {
public:
    /**
     * @brief Callback type for operation success
     * @param attempt_number The attempt number (1-indexed)
     */
    using SuccessCallback = std::function<void(uint32_t attempt_number)>;

    /**
     * @brief Callback type for operation failure
     * @param attempt_number The attempt number (1-indexed)
     * @param will_retry Whether another retry will be attempted
     * @param error_message Optional error message
     */
    using FailureCallback = std::function<void(uint32_t attempt_number,
                                                bool will_retry,
                                                const std::string& error_message)>;

    /**
     * @brief Callback type for retry delay
     * @param attempt_number The attempt number (1-indexed)
     * @param delay The computed delay before next retry
     */
    using DelayCallback = std::function<void(uint32_t attempt_number,
                                             std::chrono::milliseconds delay)>;

    /**
     * @brief Construct retry policy with configuration
     * @param config Retry configuration parameters
     */
    explicit RetryPolicy(const RetryConfig& config);

    /**
     * @brief Destructor
     */
    ~RetryPolicy();

    // Prevent copying, allow moving
    RetryPolicy(const RetryPolicy&) = delete;
    RetryPolicy& operator=(const RetryPolicy&) = delete;
    RetryPolicy(RetryPolicy&&) noexcept;
    RetryPolicy& operator=(RetryPolicy&&) noexcept;

    /**
     * @brief Execute an operation with retry logic
     * @param operation Function that returns true on success, false on failure
     * @return RetryResult indicating outcome
     */
    RetryResult execute(std::function<bool()> operation);

    /**
     * @brief Execute an operation with retry logic and error information
     * @param operation Function that returns true on success, false on failure
     * @param error_msg Output parameter for error message (if failed)
     * @return RetryResult indicating outcome
     */
    RetryResult execute(std::function<bool()> operation, std::string& error_msg);

    /**
     * @brief Execute an operation with retry logic and exception handling
     *
     * Catches exceptions thrown by the operation and treats them as failures.
     * The exception message is logged and available via the failure callback.
     *
     * @param operation Function that returns true on success, throws on failure
     * @return RetryResult indicating outcome
     */
    RetryResult executeWithExceptions(std::function<bool()> operation);

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
     * @brief Set callback for retry delays
     * @param callback Function to call before delay
     */
    void setDelayCallback(DelayCallback callback);

    /**
     * @brief Get current retry statistics
     * @return Copy of current statistics
     */
    RetryStats getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Get the current configuration
     * @return Copy of current configuration
     */
    RetryConfig getConfig() const;

    /**
     * @brief Update the retry configuration
     * @param config New configuration parameters
     */
    void updateConfig(const RetryConfig& config);

    /**
     * @brief Calculate the delay for a given attempt number
     *
     * This method is exposed for testing and introspection.
     *
     * @param attempt_number The attempt number (1-indexed)
     * @return Computed delay duration
     */
    std::chrono::milliseconds calculateDelay(uint32_t attempt_number) const;

    /**
     * @brief Check if an error should trigger a retry
     * @param error_code System error code (errno)
     * @return true if error is retryable
     */
    bool isRetryableError(int error_code) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Builder pattern for constructing RetryPolicy with fluent API
 *
 * Example usage:
 * @code
 * auto policy = RetryPolicyBuilder()
 *     .withMaxRetries(5)
 *     .withExponentialBackoff(100, 10000)
 *     .withJitter()
 *     .onSuccess([](uint32_t attempt) {
 *         std::cout << "Success on attempt " << attempt << std::endl;
 *     })
 *     .build();
 * @endcode
 */
class RetryPolicyBuilder {
public:
    /**
     * @brief Construct builder with default configuration
     */
    RetryPolicyBuilder();

    /**
     * @brief Set maximum number of retries
     */
    RetryPolicyBuilder& withMaxRetries(uint32_t max_retries);

    /**
     * @brief Set constant delay strategy
     */
    RetryPolicyBuilder& withConstantDelay(std::chrono::milliseconds delay);

    /**
     * @brief Set linear backoff strategy
     */
    RetryPolicyBuilder& withLinearBackoff(std::chrono::milliseconds initial_delay,
                                          uint32_t increment_ms);

    /**
     * @brief Set exponential backoff strategy
     */
    RetryPolicyBuilder& withExponentialBackoff(std::chrono::milliseconds initial_delay,
                                                std::chrono::milliseconds max_delay,
                                                double multiplier = 2.0);

    /**
     * @brief Enable jitter for delay randomization
     */
    RetryPolicyBuilder& withJitter();

    /**
     * @brief Disable jitter
     */
    RetryPolicyBuilder& withoutJitter();

    /**
     * @brief Set timeout per attempt
     */
    RetryPolicyBuilder& withTimeoutPerAttempt(std::chrono::milliseconds timeout);

    /**
     * @brief Enable retry on timeout errors
     */
    RetryPolicyBuilder& retryOnTimeout(bool enable = true);

    /**
     * @brief Enable retry on connection refused errors
     */
    RetryPolicyBuilder& retryOnConnectionRefused(bool enable = true);

    /**
     * @brief Enable retry on temporary network errors
     */
    RetryPolicyBuilder& retryOnTempError(bool enable = true);

    /**
     * @brief Set success callback
     */
    RetryPolicyBuilder& onSuccess(RetryPolicy::SuccessCallback callback);

    /**
     * @brief Set failure callback
     */
    RetryPolicyBuilder& onFailure(RetryPolicy::FailureCallback callback);

    /**
     * @brief Set delay callback
     */
    RetryPolicyBuilder& onDelay(RetryPolicy::DelayCallback callback);

    /**
     * @brief Build the RetryPolicy instance
     */
    std::unique_ptr<RetryPolicy> build();

private:
    RetryConfig config_;
    RetryPolicy::SuccessCallback success_callback_;
    RetryPolicy::FailureCallback failure_callback_;
    RetryPolicy::DelayCallback delay_callback_;
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_RETRY_POLICY_H
