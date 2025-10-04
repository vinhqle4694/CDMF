#include "ipc/retry_policy.h"
#include "utils/log.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace cdmf {
namespace ipc {

/**
 * @brief Private implementation class for RetryPolicy
 */
class RetryPolicy::Impl {
public:
    explicit Impl(const RetryConfig& config)
        : config_(config)
        , stats_{}
        , rng_(std::random_device{}())
        , jitter_dist_(-0.2, 0.2) // ±20% jitter
        , prev_delay_(config.initial_delay) {
        LOGD_FMT("RetryPolicy::Impl constructor called with max_retries=" << config.max_retries);
        validateConfig();
    }

    RetryResult execute(std::function<bool()> operation) {
        LOGD_FMT("RetryPolicy::execute called");
        std::string unused_error;
        return executeImpl(operation, &unused_error, false);
    }

    RetryResult execute(std::function<bool()> operation, std::string& error_msg) {
        LOGD_FMT("RetryPolicy::execute called with error message tracking");
        return executeImpl(operation, &error_msg, false);
    }

    RetryResult executeWithExceptions(std::function<bool()> operation) {
        LOGD_FMT("RetryPolicy::executeWithExceptions called");
        std::string unused_error;
        return executeImpl(operation, &unused_error, true);
    }

    void setSuccessCallback(SuccessCallback callback) {
        LOGD_FMT("RetryPolicy::setSuccessCallback called");
        std::lock_guard<std::mutex> lock(callback_mutex_);
        success_callback_ = std::move(callback);
    }

    void setFailureCallback(FailureCallback callback) {
        LOGD_FMT("RetryPolicy::setFailureCallback called");
        std::lock_guard<std::mutex> lock(callback_mutex_);
        failure_callback_ = std::move(callback);
    }

    void setDelayCallback(DelayCallback callback) {
        LOGD_FMT("RetryPolicy::setDelayCallback called");
        std::lock_guard<std::mutex> lock(callback_mutex_);
        delay_callback_ = std::move(callback);
    }

    RetryStats getStatistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void resetStatistics() {
        LOGI_FMT("RetryPolicy::resetStatistics called");
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.reset();
        LOGD_FMT("RetryPolicy::resetStatistics: statistics reset");
    }

    RetryConfig getConfig() const {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    void updateConfig(const RetryConfig& config) {
        LOGI_FMT("RetryPolicy::updateConfig called");
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        validateConfig();
        LOGD_FMT("RetryPolicy::updateConfig: configuration updated");
    }

    std::chrono::milliseconds calculateDelay(uint32_t attempt_number) const {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return calculateDelayImpl(attempt_number);
    }

    bool isRetryableError(int error_code) const {
        std::lock_guard<std::mutex> lock(config_mutex_);

        switch (error_code) {
            // Temporary network errors (EAGAIN and EWOULDBLOCK are often the same value)
            case EAGAIN:
            #if EAGAIN != EWOULDBLOCK
            case EWOULDBLOCK:
            #endif
            case EINTR:
                return config_.retry_on_temp_error;

            // Connection errors
            case ECONNREFUSED:
            case ECONNRESET:
            case ECONNABORTED:
            case EHOSTUNREACH:
            case ENETUNREACH:
                return config_.retry_on_connection_refused;

            // Timeout errors
            case ETIMEDOUT:
                return config_.retry_on_timeout;

            // Retryable errors
            case EPIPE:
            case ENOTCONN:
                return true;

            // Non-retryable errors
            case EACCES:
            case EPERM:
            case EINVAL:
            case EBADF:
                return false;

            default:
                return false;
        }
    }

private:
    void validateConfig() {
        if (config_.initial_delay > config_.max_delay) {
            throw std::invalid_argument("Initial delay cannot exceed max delay");
        }
        if (config_.backoff_multiplier < 1.0) {
            throw std::invalid_argument("Backoff multiplier must be >= 1.0");
        }
    }

    RetryResult executeImpl(std::function<bool()> operation,
                           std::string* error_msg,
                           bool catch_exceptions) {
        LOGD_FMT("RetryPolicy::executeImpl starting");
        // auto start_time = std::chrono::steady_clock::now();
        uint32_t attempt = 0;
        bool first_try = true;

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_attempts++;
        }

        while (attempt <= config_.max_retries) {
            attempt++;
            LOGD_FMT("RetryPolicy::executeImpl: attempt " << attempt << " of " << (config_.max_retries + 1));

            try {
                bool success = operation();

                if (success) {
                    // Update statistics
                    {
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        if (first_try) {
                            stats_.first_try_successes++;
                        } else {
                            stats_.retry_successes++;
                            // Update average retries on success
                            double total_successful = stats_.first_try_successes + stats_.retry_successes;
                            stats_.avg_retries_on_success =
                                (stats_.avg_retries_on_success * (total_successful - 1) + (attempt - 1)) / total_successful;
                        }
                    }

                    // Call success callback
                    {
                        std::lock_guard<std::mutex> lock(callback_mutex_);
                        if (success_callback_) {
                            success_callback_(attempt);
                        }
                    }

                    LOGI_FMT("RetryPolicy::executeImpl: operation succeeded on attempt " << attempt);
                    return RetryResult::SUCCESS;
                }

                // Operation failed
                first_try = false;

                // Check if we should retry
                bool will_retry = (attempt <= config_.max_retries);

                LOGW_FMT("RetryPolicy::executeImpl: operation failed on attempt " << attempt
                         << ", will_retry=" << will_retry);

                // Call failure callback
                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (failure_callback_) {
                        failure_callback_(attempt, will_retry,
                                        error_msg ? *error_msg : "Operation failed");
                    }
                }

                if (!will_retry) {
                    break;
                }

                // Calculate and apply delay
                auto delay = calculateDelay(attempt);

                LOGD_FMT("RetryPolicy::executeImpl: retrying after " << delay.count() << "ms delay");

                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (delay_callback_) {
                        delay_callback_(attempt, delay);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.total_retry_delay += delay;
                }

                std::this_thread::sleep_for(delay);

            } catch (const std::exception& e) {
                LOGE_FMT("RetryPolicy::executeImpl: exception caught - " << e.what());

                if (!catch_exceptions) {
                    throw;
                }

                if (error_msg) {
                    *error_msg = e.what();
                }

                bool will_retry = (attempt <= config_.max_retries);

                LOGW_FMT("RetryPolicy::executeImpl: will_retry=" << will_retry << " after exception");

                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (failure_callback_) {
                        failure_callback_(attempt, will_retry, e.what());
                    }
                }

                if (!will_retry) {
                    break;
                }

                auto delay = calculateDelay(attempt);

                LOGD_FMT("RetryPolicy::executeImpl: retrying after " << delay.count() << "ms delay (exception)");

                {
                    std::lock_guard<std::mutex> lock(callback_mutex_);
                    if (delay_callback_) {
                        delay_callback_(attempt, delay);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.total_retry_delay += delay;
                }

                std::this_thread::sleep_for(delay);
            }
        }

        // Max retries exceeded
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_failures++;
        }

        LOGE_FMT("RetryPolicy::executeImpl: max retries exceeded, total failures=" << stats_.total_failures);

        if (error_msg && error_msg->empty()) {
            *error_msg = "Maximum retry attempts exceeded";
        }

        return RetryResult::MAX_RETRIES_EXCEEDED;
    }

    std::chrono::milliseconds calculateDelayImpl(uint32_t attempt_number) const {
        if (attempt_number == 0) {
            return std::chrono::milliseconds(0);
        }

        std::chrono::milliseconds delay;

        switch (config_.strategy) {
            case RetryStrategy::CONSTANT:
                delay = config_.initial_delay;
                break;

            case RetryStrategy::LINEAR:
                delay = config_.initial_delay +
                       std::chrono::milliseconds(config_.linear_increment_ms * (attempt_number - 1));
                break;

            case RetryStrategy::EXPONENTIAL: {
                double multiplier = std::pow(config_.backoff_multiplier, attempt_number - 1);
                delay = std::chrono::milliseconds(
                    static_cast<uint64_t>(config_.initial_delay.count() * multiplier));
                break;
            }

            case RetryStrategy::EXPONENTIAL_JITTER: {
                // Decorrelated jitter: random between initial_delay and prev_delay * 3
                std::lock_guard<std::mutex> lock(jitter_mutex_);
                auto min_delay = config_.initial_delay.count();
                auto max_delay_calc = prev_delay_.count() * 3;

                std::uniform_int_distribution<uint64_t> dist(min_delay, max_delay_calc);
                delay = std::chrono::milliseconds(dist(rng_));
                prev_delay_ = delay;
                break;
            }

            default:
                delay = config_.initial_delay;
        }

        // Apply jitter if enabled (except for EXPONENTIAL_JITTER which has built-in jitter)
        if (config_.enable_jitter && config_.strategy != RetryStrategy::EXPONENTIAL_JITTER) {
            std::lock_guard<std::mutex> lock(jitter_mutex_);
            double jitter = jitter_dist_(rng_);
            delay = std::chrono::milliseconds(
                static_cast<uint64_t>(delay.count() * (1.0 + jitter)));
        }

        // Cap at max_delay
        if (delay > config_.max_delay) {
            delay = config_.max_delay;
        }

        return delay;
    }

    RetryConfig config_;
    mutable std::mutex config_mutex_;

    RetryStats stats_;
    mutable std::mutex stats_mutex_;

    SuccessCallback success_callback_;
    FailureCallback failure_callback_;
    DelayCallback delay_callback_;
    std::mutex callback_mutex_;

    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<double> jitter_dist_;
    mutable std::chrono::milliseconds prev_delay_;
    mutable std::mutex jitter_mutex_;
};

// RetryPolicy implementation
RetryPolicy::RetryPolicy(const RetryConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {
}

RetryPolicy::~RetryPolicy() = default;

RetryPolicy::RetryPolicy(RetryPolicy&&) noexcept = default;
RetryPolicy& RetryPolicy::operator=(RetryPolicy&&) noexcept = default;

RetryResult RetryPolicy::execute(std::function<bool()> operation) {
    return pImpl_->execute(std::move(operation));
}

RetryResult RetryPolicy::execute(std::function<bool()> operation, std::string& error_msg) {
    return pImpl_->execute(std::move(operation), error_msg);
}

RetryResult RetryPolicy::executeWithExceptions(std::function<bool()> operation) {
    return pImpl_->executeWithExceptions(std::move(operation));
}

void RetryPolicy::setSuccessCallback(SuccessCallback callback) {
    pImpl_->setSuccessCallback(std::move(callback));
}

void RetryPolicy::setFailureCallback(FailureCallback callback) {
    pImpl_->setFailureCallback(std::move(callback));
}

void RetryPolicy::setDelayCallback(DelayCallback callback) {
    pImpl_->setDelayCallback(std::move(callback));
}

RetryStats RetryPolicy::getStatistics() const {
    return pImpl_->getStatistics();
}

void RetryPolicy::resetStatistics() {
    pImpl_->resetStatistics();
}

RetryConfig RetryPolicy::getConfig() const {
    return pImpl_->getConfig();
}

void RetryPolicy::updateConfig(const RetryConfig& config) {
    pImpl_->updateConfig(config);
}

std::chrono::milliseconds RetryPolicy::calculateDelay(uint32_t attempt_number) const {
    return pImpl_->calculateDelay(attempt_number);
}

bool RetryPolicy::isRetryableError(int error_code) const {
    return pImpl_->isRetryableError(error_code);
}

// RetryPolicyBuilder implementation
RetryPolicyBuilder::RetryPolicyBuilder() {
    // Use default config
}

RetryPolicyBuilder& RetryPolicyBuilder::withMaxRetries(uint32_t max_retries) {
    config_.max_retries = max_retries;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::withConstantDelay(std::chrono::milliseconds delay) {
    config_.strategy = RetryStrategy::CONSTANT;
    config_.initial_delay = delay;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::withLinearBackoff(
    std::chrono::milliseconds initial_delay, uint32_t increment_ms) {
    config_.strategy = RetryStrategy::LINEAR;
    config_.initial_delay = initial_delay;
    config_.linear_increment_ms = increment_ms;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::withExponentialBackoff(
    std::chrono::milliseconds initial_delay,
    std::chrono::milliseconds max_delay,
    double multiplier) {
    config_.strategy = RetryStrategy::EXPONENTIAL;
    config_.initial_delay = initial_delay;
    config_.max_delay = max_delay;
    config_.backoff_multiplier = multiplier;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::withJitter() {
    config_.enable_jitter = true;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::withoutJitter() {
    config_.enable_jitter = false;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::withTimeoutPerAttempt(
    std::chrono::milliseconds timeout) {
    config_.timeout_per_attempt = timeout;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::retryOnTimeout(bool enable) {
    config_.retry_on_timeout = enable;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::retryOnConnectionRefused(bool enable) {
    config_.retry_on_connection_refused = enable;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::retryOnTempError(bool enable) {
    config_.retry_on_temp_error = enable;
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::onSuccess(RetryPolicy::SuccessCallback callback) {
    success_callback_ = std::move(callback);
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::onFailure(RetryPolicy::FailureCallback callback) {
    failure_callback_ = std::move(callback);
    return *this;
}

RetryPolicyBuilder& RetryPolicyBuilder::onDelay(RetryPolicy::DelayCallback callback) {
    delay_callback_ = std::move(callback);
    return *this;
}

std::unique_ptr<RetryPolicy> RetryPolicyBuilder::build() {
    auto policy = std::make_unique<RetryPolicy>(config_);

    if (success_callback_) {
        policy->setSuccessCallback(success_callback_);
    }
    if (failure_callback_) {
        policy->setFailureCallback(failure_callback_);
    }
    if (delay_callback_) {
        policy->setDelayCallback(delay_callback_);
    }

    return policy;
}

} // namespace ipc
} // namespace cdmf
