/**
 * @file ring_buffer.cpp
 * @brief Ring buffer utility implementations
 *
 * Provides helper functions and utilities for ring buffer operations.
 *
 * @author Ring Buffer Agent
 * @date 2025-10-03
 */

#include "ipc/ring_buffer.h"
#include "utils/log.h"
#include <cmath>
#include <stdexcept>

namespace cdmf {
namespace ipc {

/**
 * @brief Calculate next power of 2 for buffer sizing
 *
 * @param n Input size
 * @return Next power of 2 >= n
 */
size_t next_power_of_2(size_t n) {
    LOGD_FMT("next_power_of_2 - input: " << n);

    if (n == 0) {
        LOGD_FMT("next_power_of_2 - input is 0, returning 1");
        return 1;
    }

    // Check if already power of 2
    if ((n & (n - 1)) == 0) {
        LOGD_FMT("next_power_of_2 - already power of 2, returning: " << n);
        return n;
    }

    // Find next power of 2
    size_t power = 1;
    while (power < n) {
        power <<= 1;
        if (power == 0) {
            // Overflow - return maximum power of 2 for size_t
            size_t max_power = size_t(1) << (sizeof(size_t) * 8 - 1);
            LOGW_FMT("next_power_of_2 - overflow detected, returning max: " << max_power);
            return max_power;
        }
    }

    LOGD_FMT("next_power_of_2 - calculated result: " << power);
    return power;
}

/**
 * @brief Validate if a number is power of 2
 *
 * @param n Number to check
 * @return true if power of 2, false otherwise
 */
bool is_power_of_2(size_t n) {
    LOGD_FMT("is_power_of_2 - checking: " << n);
    bool result = n > 0 && (n & (n - 1)) == 0;
    LOGD_FMT("is_power_of_2 - result: " << (result ? "true" : "false"));
    return result;
}

/**
 * @brief Calculate optimal buffer size based on expected throughput
 *
 * @param messages_per_second Expected message rate
 * @param message_size_bytes Average message size
 * @param latency_target_us Target latency in microseconds
 * @return Recommended buffer capacity (power of 2)
 */
size_t calculate_optimal_buffer_size(
    uint64_t messages_per_second,
    [[maybe_unused]] size_t message_size_bytes,
    uint64_t latency_target_us) {

    LOGD_FMT("calculate_optimal_buffer_size - messages_per_second: " << messages_per_second
             << ", latency_target_us: " << latency_target_us);

    // Calculate messages produced during target latency window
    double latency_seconds = latency_target_us / 1000000.0;
    size_t messages_in_window = static_cast<size_t>(
        messages_per_second * latency_seconds
    );

    LOGD_FMT("calculate_optimal_buffer_size - messages_in_window: " << messages_in_window);

    // Add safety margin (2x)
    size_t required_capacity = messages_in_window * 2;

    // Ensure minimum size
    if (required_capacity < 64) {
        LOGD_FMT("calculate_optimal_buffer_size - applying minimum size of 64");
        required_capacity = 64;
    }

    // Round up to next power of 2
    size_t result = next_power_of_2(required_capacity);
    LOGD_FMT("calculate_optimal_buffer_size - final result: " << result);
    return result;
}

/**
 * @brief Get recommended buffer size for common scenarios
 *
 * @param scenario Scenario name ("low_latency", "high_throughput", "balanced")
 * @return Recommended buffer capacity
 */
size_t get_recommended_buffer_size(const char* scenario) {
    LOGD_FMT("get_recommended_buffer_size - scenario: " << (scenario ? scenario : "null"));

    if (std::string(scenario) == "low_latency") {
        // Small buffer for minimal latency
        LOGD_FMT("get_recommended_buffer_size - returning 256 for low_latency");
        return 256;
    } else if (std::string(scenario) == "high_throughput") {
        // Large buffer for maximum throughput
        LOGD_FMT("get_recommended_buffer_size - returning 16384 for high_throughput");
        return 16384;
    } else if (std::string(scenario) == "balanced") {
        // Balanced configuration
        LOGD_FMT("get_recommended_buffer_size - returning 4096 for balanced");
        return 4096;
    } else {
        // Default
        LOGD_FMT("get_recommended_buffer_size - returning 1024 for default/unknown scenario");
        return 1024;
    }
}

namespace detail {

/**
 * @brief Memory barrier helper for compiler optimization control
 */
inline void memory_barrier() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

/**
 * @brief Spin wait with exponential backoff
 *
 * Used internally for contention handling in busy-wait scenarios
 *
 * @param iteration Current iteration number
 */
inline void spin_wait(int iteration) {
    if (iteration < 10) {
        // Pause CPU for a few cycles
        #if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
        #elif defined(__aarch64__) || defined(__arm__)
        __asm__ __volatile__("yield" ::: "memory");
        #endif
    } else if (iteration < 20) {
        // Short sleep
        for (volatile int i = 0; i < 50; ++i) {}
    } else {
        // Yield to scheduler
        #ifdef _WIN32
        SwitchToThread();
        #else
        sched_yield();
        #endif
    }
}

} // namespace detail

/**
 * @brief Ring buffer configuration validator
 */
class RingBufferConfig {
public:
    static bool validate_capacity(size_t capacity) {
        LOGD_FMT("RingBufferConfig::validate_capacity - capacity: " << capacity);

        // Must be power of 2
        if (!is_power_of_2(capacity)) {
            LOGW_FMT("RingBufferConfig::validate_capacity - not a power of 2");
            return false;
        }

        // Must be reasonable size (between 2 and 2^30)
        if (capacity < 2 || capacity > (1ULL << 30)) {
            LOGW_FMT("RingBufferConfig::validate_capacity - out of range (must be 2 to 2^30)");
            return false;
        }

        LOGD_FMT("RingBufferConfig::validate_capacity - validation passed");
        return true;
    }

    static bool validate_element_size(size_t element_size) {
        LOGD_FMT("RingBufferConfig::validate_element_size - element_size: " << element_size);

        // Element size should be reasonable
        if (element_size == 0 || element_size > (1024 * 1024)) {
            LOGW_FMT("RingBufferConfig::validate_element_size - invalid size (must be 1 to 1MB)");
            return false;
        }

        LOGD_FMT("RingBufferConfig::validate_element_size - validation passed");
        return true;
    }

    static bool validate_alignment(size_t alignment) {
        LOGD_FMT("RingBufferConfig::validate_alignment - alignment: " << alignment);
        // Alignment must be power of 2
        bool result = is_power_of_2(alignment);
        LOGD_FMT("RingBufferConfig::validate_alignment - result: " << (result ? "valid" : "invalid"));
        return result;
    }
};

/**
 * @brief Performance hint for ring buffer usage patterns
 */
enum class UsagePattern {
    SPSC_LOW_LATENCY,    // Single producer/consumer, minimize latency
    SPSC_HIGH_THROUGHPUT, // Single producer/consumer, maximize throughput
    MPMC_BALANCED,        // Multiple producers/consumers, balanced
    MPMC_PRODUCER_HEAVY,  // More producers than consumers
    MPMC_CONSUMER_HEAVY   // More consumers than producers
};

/**
 * @brief Get performance recommendations based on usage pattern
 */
struct PerformanceRecommendation {
    size_t buffer_size;
    bool use_spsc;  // true = SPSC, false = MPMC
    const char* memory_order_advice;

    static PerformanceRecommendation get(UsagePattern pattern) {
        LOGD_FMT("PerformanceRecommendation::get - pattern: " << static_cast<int>(pattern));

        PerformanceRecommendation rec;

        switch (pattern) {
            case UsagePattern::SPSC_LOW_LATENCY:
                rec.buffer_size = 256;
                rec.use_spsc = true;
                rec.memory_order_advice = "Use relaxed ordering where safe";
                LOGD_FMT("PerformanceRecommendation::get - SPSC_LOW_LATENCY: buffer_size=256");
                break;

            case UsagePattern::SPSC_HIGH_THROUGHPUT:
                rec.buffer_size = 16384;
                rec.use_spsc = true;
                rec.memory_order_advice = "Batch operations for better throughput";
                LOGD_FMT("PerformanceRecommendation::get - SPSC_HIGH_THROUGHPUT: buffer_size=16384");
                break;

            case UsagePattern::MPMC_BALANCED:
                rec.buffer_size = 4096;
                rec.use_spsc = false;
                rec.memory_order_advice = "Use standard acquire/release semantics";
                LOGD_FMT("PerformanceRecommendation::get - MPMC_BALANCED: buffer_size=4096");
                break;

            case UsagePattern::MPMC_PRODUCER_HEAVY:
                rec.buffer_size = 8192;
                rec.use_spsc = false;
                rec.memory_order_advice = "Consider larger buffer for producer contention";
                LOGD_FMT("PerformanceRecommendation::get - MPMC_PRODUCER_HEAVY: buffer_size=8192");
                break;

            case UsagePattern::MPMC_CONSUMER_HEAVY:
                rec.buffer_size = 2048;
                rec.use_spsc = false;
                rec.memory_order_advice = "Smaller buffer OK with fast consumers";
                LOGD_FMT("PerformanceRecommendation::get - MPMC_CONSUMER_HEAVY: buffer_size=2048");
                break;

            default:
                rec.buffer_size = 1024;
                rec.use_spsc = true;
                rec.memory_order_advice = "Default configuration";
                LOGD_FMT("PerformanceRecommendation::get - DEFAULT: buffer_size=1024");
                break;
        }

        return rec;
    }
};

} // namespace ipc
} // namespace cdmf
