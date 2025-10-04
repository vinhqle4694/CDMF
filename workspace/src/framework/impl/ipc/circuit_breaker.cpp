#include "ipc/circuit_breaker.h"
#include "utils/log.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <stdexcept>

namespace cdmf {
namespace ipc {

/**
 * @brief Private implementation class for CircuitBreaker
 */
class CircuitBreaker::Impl {
public:
    explicit Impl(const CircuitBreakerConfig& config)
        : config_(config)
        , stats_{}
        , current_state_(CircuitState::CLOSED)
        , last_state_change_(std::chrono::steady_clock::now()) {
        LOGI_FMT("Creating CircuitBreaker - failure_threshold: " << config_.failure_threshold
                 << ", success_threshold: " << config_.success_threshold
                 << ", open_timeout: " << config_.open_timeout.count() << "ms");
        validateConfig();
        stats_.current_state = CircuitState::CLOSED;
    }

    bool execute(std::function<bool()> operation) {
        ReliabilityError unused_error;
        std::string unused_msg;
        return executeImpl(operation, &unused_error, &unused_msg, false);
    }

    bool execute(std::function<bool()> operation, ReliabilityError& error) {
        std::string unused_msg;
        return executeImpl(operation, &error, &unused_msg, false);
    }

    bool execute(std::function<bool()> operation,
                 ReliabilityError& error,
                 std::string& error_msg) {
        return executeImpl(operation, &error, &error_msg, false);
    }

    bool executeWithExceptions(std::function<bool()> operation) {
        ReliabilityError unused_error;
        std::string unused_msg;
        return executeImpl(operation, &unused_error, &unused_msg, true);
    }

    bool allowsRequests() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return allowsRequestsImpl();
    }

    CircuitState getState() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return current_state_;
    }

    CircuitBreakerStats getStatistics() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return stats_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        LOGI_FMT("Resetting circuit breaker to CLOSED state");
        transitionTo(CircuitState::CLOSED);
        stats_.consecutive_failures = 0;
        stats_.consecutive_successes = 0;
        rolling_window_.clear();
    }

    void resetStatistics() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        auto current_state = stats_.current_state;
        auto consecutive_failures = stats_.consecutive_failures;
        auto consecutive_successes = stats_.consecutive_successes;

        stats_.reset();
        stats_.current_state = current_state;
        stats_.consecutive_failures = consecutive_failures;
        stats_.consecutive_successes = consecutive_successes;
    }

    void forceOpen() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        LOGI_FMT("Forcing circuit breaker to OPEN state");
        if (current_state_ != CircuitState::OPEN) {
            transitionTo(CircuitState::OPEN);
        }
    }

    void forceHalfOpen() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        LOGI_FMT("Forcing circuit breaker to HALF_OPEN state");
        if (current_state_ != CircuitState::HALF_OPEN) {
            transitionTo(CircuitState::HALF_OPEN);
        }
    }

    CircuitBreakerConfig getConfig() const {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    void updateConfig(const CircuitBreakerConfig& config) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        validateConfig();
    }

    void setStateChangeCallback(StateChangeCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        state_change_callback_ = std::move(callback);
    }

    void setSuccessCallback(SuccessCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        success_callback_ = std::move(callback);
    }

    void setFailureCallback(FailureCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        failure_callback_ = std::move(callback);
    }

    void setRejectionCallback(RejectionCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        rejection_callback_ = std::move(callback);
    }

    void recordSuccess() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        handleSuccess();
    }

    void recordFailure() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        handleFailure("");
    }

private:
    void validateConfig() {
        if (config_.failure_threshold == 0) {
            throw std::invalid_argument("Failure threshold must be > 0");
        }
        if (config_.success_threshold == 0) {
            throw std::invalid_argument("Success threshold must be > 0");
        }
        if (config_.failure_rate_threshold < 0.0 || config_.failure_rate_threshold > 1.0) {
            throw std::invalid_argument("Failure rate threshold must be between 0.0 and 1.0");
        }
    }

    bool executeImpl(std::function<bool()> operation,
                    ReliabilityError* error,
                    std::string* error_msg,
                    bool catch_exceptions) {
        // Check if circuit allows requests
        {
            std::lock_guard<std::mutex> lock(state_mutex_);

            // Check if we need to transition from OPEN to HALF_OPEN
            if (current_state_ == CircuitState::OPEN) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_state_change_);

                if (elapsed >= config_.open_timeout) {
                    transitionTo(CircuitState::HALF_OPEN);
                }
            }

            if (!allowsRequestsImpl()) {
                stats_.total_rejections++;
                LOGW_FMT("Circuit breaker rejected request - state: OPEN, total_rejections: "
                         << stats_.total_rejections);

                if (error) {
                    *error = ReliabilityError::CIRCUIT_OPEN;
                }
                if (error_msg) {
                    *error_msg = "Circuit breaker is OPEN";
                }

                // Call rejection callback
                {
                    std::lock_guard<std::mutex> cb_lock(callback_mutex_);
                    if (rejection_callback_) {
                        rejection_callback_();
                    }
                }

                return false;
            }
        }

        // Execute the operation
        bool success = false;
        std::string local_error_msg;

        LOGD_FMT("Executing operation through circuit breaker");
        try {
            success = operation();
        } catch (const std::exception& e) {
            if (!catch_exceptions) {
                LOGE_FMT("Exception in circuit breaker operation (re-throwing): " << e.what());
                throw;
            }
            local_error_msg = e.what();
            success = false;
            LOGE_FMT("Exception in circuit breaker operation (caught): " << e.what());
        }

        // Update state based on result
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (success) {
            LOGD_FMT("Operation succeeded through circuit breaker");
            handleSuccess();
            if (error) {
                *error = ReliabilityError::NONE;
            }
        } else {
            LOGD_FMT("Operation failed through circuit breaker"
                     << (local_error_msg.empty() ? "" : ": " + local_error_msg));
            handleFailure(local_error_msg);
            if (error_msg && !local_error_msg.empty()) {
                *error_msg = local_error_msg;
            }
        }

        return success;
    }

    bool allowsRequestsImpl() const {
        return current_state_ != CircuitState::OPEN;
    }

    void handleSuccess() {
        stats_.total_successes++;
        stats_.consecutive_successes++;
        stats_.consecutive_failures = 0;

        LOGD_FMT("Circuit breaker success - total: " << stats_.total_successes
                 << ", consecutive: " << stats_.consecutive_successes);

        // Update rolling window if enabled
        if (config_.rolling_window_size > 0) {
            rolling_window_.push_back(true);
            if (rolling_window_.size() > config_.rolling_window_size) {
                rolling_window_.pop_front();
            }
            updateFailureRate();
        }

        // State transitions based on success
        switch (current_state_) {
            case CircuitState::CLOSED:
                // Even on success, check if failure rate is too high (for rolling window mode)
                if (config_.rolling_window_size > 0 && shouldOpenCircuit()) {
                    LOGW_FMT("Opening circuit due to high failure rate: "
                             << stats_.current_failure_rate);
                    transitionTo(CircuitState::OPEN);
                }
                break;

            case CircuitState::HALF_OPEN:
                // Check if we've hit success threshold
                if (stats_.consecutive_successes >= config_.success_threshold) {
                    LOGI_FMT("Closing circuit after " << stats_.consecutive_successes
                             << " consecutive successes");
                    transitionTo(CircuitState::CLOSED);
                }
                break;

            case CircuitState::OPEN:
                // Should not receive successes in OPEN state
                // This can happen during state transition
                break;
        }

        // Call success callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (success_callback_) {
                success_callback_(current_state_);
            }
        }
    }

    void handleFailure(const std::string& error_message) {
        stats_.total_failures++;
        stats_.consecutive_failures++;
        stats_.consecutive_successes = 0;

        LOGD_FMT("Circuit breaker failure - total: " << stats_.total_failures
                 << ", consecutive: " << stats_.consecutive_failures
                 << (error_message.empty() ? "" : ", error: " + error_message));

        // Update rolling window if enabled
        if (config_.rolling_window_size > 0) {
            rolling_window_.push_back(false);
            if (rolling_window_.size() > config_.rolling_window_size) {
                rolling_window_.pop_front();
            }
            updateFailureRate();
        }

        // State transitions based on failure
        switch (current_state_) {
            case CircuitState::CLOSED:
                if (shouldOpenCircuit()) {
                    LOGW_FMT("Opening circuit after " << stats_.consecutive_failures
                             << " consecutive failures (threshold: " << config_.failure_threshold << ")");
                    transitionTo(CircuitState::OPEN);
                }
                break;

            case CircuitState::HALF_OPEN:
                // Any failure in HALF_OPEN immediately opens circuit
                LOGW_FMT("Re-opening circuit from HALF_OPEN after failure");
                transitionTo(CircuitState::OPEN);
                break;

            case CircuitState::OPEN:
                // Stay open
                break;
        }

        // Call failure callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (failure_callback_) {
                failure_callback_(current_state_, error_message);
            }
        }
    }

    bool shouldOpenCircuit() const {
        // Use rolling window if configured
        if (config_.rolling_window_size > 0) {
            // Don't open circuit if we haven't reached minimum request threshold
            if (rolling_window_.size() < config_.minimum_request_threshold) {
                return false;
            }
            return stats_.current_failure_rate >= config_.failure_rate_threshold;
        }

        // Otherwise use consecutive failures
        return stats_.consecutive_failures >= config_.failure_threshold;
    }

    void updateFailureRate() {
        if (rolling_window_.empty()) {
            stats_.current_failure_rate = 0.0;
            return;
        }

        uint32_t failures = 0;
        for (bool success : rolling_window_) {
            if (!success) {
                failures++;
            }
        }

        stats_.current_failure_rate =
            static_cast<double>(failures) / rolling_window_.size();
    }

    void transitionTo(CircuitState new_state) {
        if (current_state_ == new_state) {
            return;
        }

        CircuitState old_state = current_state_;
        current_state_ = new_state;
        stats_.current_state = new_state;
        last_state_change_ = std::chrono::steady_clock::now();

        const char* state_names[] = {"CLOSED", "OPEN", "HALF_OPEN"};
        LOGI_FMT("Circuit breaker state transition: "
                 << state_names[static_cast<int>(old_state)] << " -> "
                 << state_names[static_cast<int>(new_state)]);

        // Reset consecutive counters on transition
        stats_.consecutive_failures = 0;
        stats_.consecutive_successes = 0;

        // Update state-specific counters
        switch (new_state) {
            case CircuitState::OPEN:
                stats_.open_count++;
                stats_.last_open_time = last_state_change_;
                LOGI_FMT("Circuit opened (count: " << stats_.open_count << ")");
                break;

            case CircuitState::HALF_OPEN:
                stats_.half_open_count++;
                LOGI_FMT("Circuit half-open (count: " << stats_.half_open_count << ")");
                break;

            case CircuitState::CLOSED:
                stats_.close_count++;
                stats_.last_close_time = last_state_change_;
                LOGI_FMT("Circuit closed (count: " << stats_.close_count << ")");
                break;
        }

        // Call state change callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (state_change_callback_) {
                state_change_callback_(old_state, new_state);
            }
        }
    }

    CircuitBreakerConfig config_;
    mutable std::mutex config_mutex_;

    CircuitBreakerStats stats_;
    CircuitState current_state_;
    std::chrono::steady_clock::time_point last_state_change_;
    std::deque<bool> rolling_window_; // true = success, false = failure
    mutable std::mutex state_mutex_;

    StateChangeCallback state_change_callback_;
    SuccessCallback success_callback_;
    FailureCallback failure_callback_;
    RejectionCallback rejection_callback_;
    std::mutex callback_mutex_;
};

// CircuitBreaker implementation
CircuitBreaker::CircuitBreaker(const CircuitBreakerConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {
}

CircuitBreaker::~CircuitBreaker() = default;

CircuitBreaker::CircuitBreaker(CircuitBreaker&&) noexcept = default;
CircuitBreaker& CircuitBreaker::operator=(CircuitBreaker&&) noexcept = default;

bool CircuitBreaker::execute(std::function<bool()> operation) {
    return pImpl_->execute(std::move(operation));
}

bool CircuitBreaker::execute(std::function<bool()> operation, ReliabilityError& error) {
    return pImpl_->execute(std::move(operation), error);
}

bool CircuitBreaker::execute(std::function<bool()> operation,
                             ReliabilityError& error,
                             std::string& error_msg) {
    return pImpl_->execute(std::move(operation), error, error_msg);
}

bool CircuitBreaker::executeWithExceptions(std::function<bool()> operation) {
    return pImpl_->executeWithExceptions(std::move(operation));
}

bool CircuitBreaker::allowsRequests() const {
    return pImpl_->allowsRequests();
}

CircuitState CircuitBreaker::getState() const {
    return pImpl_->getState();
}

CircuitBreakerStats CircuitBreaker::getStatistics() const {
    return pImpl_->getStatistics();
}

void CircuitBreaker::reset() {
    pImpl_->reset();
}

void CircuitBreaker::resetStatistics() {
    pImpl_->resetStatistics();
}

void CircuitBreaker::forceOpen() {
    pImpl_->forceOpen();
}

void CircuitBreaker::forceHalfOpen() {
    pImpl_->forceHalfOpen();
}

CircuitBreakerConfig CircuitBreaker::getConfig() const {
    return pImpl_->getConfig();
}

void CircuitBreaker::updateConfig(const CircuitBreakerConfig& config) {
    pImpl_->updateConfig(config);
}

void CircuitBreaker::setStateChangeCallback(StateChangeCallback callback) {
    pImpl_->setStateChangeCallback(std::move(callback));
}

void CircuitBreaker::setSuccessCallback(SuccessCallback callback) {
    pImpl_->setSuccessCallback(std::move(callback));
}

void CircuitBreaker::setFailureCallback(FailureCallback callback) {
    pImpl_->setFailureCallback(std::move(callback));
}

void CircuitBreaker::setRejectionCallback(RejectionCallback callback) {
    pImpl_->setRejectionCallback(std::move(callback));
}

void CircuitBreaker::recordSuccess() {
    pImpl_->recordSuccess();
}

void CircuitBreaker::recordFailure() {
    pImpl_->recordFailure();
}

// CircuitBreakerBuilder implementation
CircuitBreakerBuilder::CircuitBreakerBuilder() {
    // Use default config
}

CircuitBreakerBuilder& CircuitBreakerBuilder::withFailureThreshold(uint32_t threshold) {
    config_.failure_threshold = threshold;
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::withSuccessThreshold(uint32_t threshold) {
    config_.success_threshold = threshold;
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::withOpenTimeout(
    std::chrono::milliseconds timeout) {
    config_.open_timeout = timeout;
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::withHalfOpenTimeout(
    std::chrono::milliseconds timeout) {
    config_.half_open_timeout = timeout;
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::withRollingWindow(
    uint32_t window_size,
    double failure_rate_threshold,
    uint32_t min_requests) {
    config_.rolling_window_size = window_size;
    config_.failure_rate_threshold = failure_rate_threshold;
    config_.minimum_request_threshold = min_requests;
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::onStateChange(
    CircuitBreaker::StateChangeCallback callback) {
    state_change_callback_ = std::move(callback);
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::onSuccess(
    CircuitBreaker::SuccessCallback callback) {
    success_callback_ = std::move(callback);
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::onFailure(
    CircuitBreaker::FailureCallback callback) {
    failure_callback_ = std::move(callback);
    return *this;
}

CircuitBreakerBuilder& CircuitBreakerBuilder::onRejection(
    CircuitBreaker::RejectionCallback callback) {
    rejection_callback_ = std::move(callback);
    return *this;
}

std::unique_ptr<CircuitBreaker> CircuitBreakerBuilder::build() {
    auto breaker = std::make_unique<CircuitBreaker>(config_);

    if (state_change_callback_) {
        breaker->setStateChangeCallback(state_change_callback_);
    }
    if (success_callback_) {
        breaker->setSuccessCallback(success_callback_);
    }
    if (failure_callback_) {
        breaker->setFailureCallback(failure_callback_);
    }
    if (rejection_callback_) {
        breaker->setRejectionCallback(rejection_callback_);
    }

    return breaker;
}

} // namespace ipc
} // namespace cdmf
