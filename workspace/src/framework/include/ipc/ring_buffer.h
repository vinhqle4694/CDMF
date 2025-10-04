/**
 * @file ring_buffer.h
 * @brief Lock-free ring buffer implementation for high-performance IPC
 *
 * Provides both SPSC (Single Producer Single Consumer) and MPMC
 * (Multi Producer Multi Consumer) variants using atomic operations.
 *
 * Features:
 * - Lock-free implementation using std::atomic
 * - Cache-line aligned to prevent false sharing
 * - Zero-copy operations where possible
 * - Configurable buffer size (must be power of 2)
 * - Overflow/underflow detection
 * - Memory barriers for proper ordering
 *
 * @author Ring Buffer Agent
 * @date 2025-10-03
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <new>

namespace cdmf {
namespace ipc {

// Cache line size for alignment
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Lock-free Single Producer Single Consumer Ring Buffer
 *
 * Optimized for SPSC scenarios using relaxed memory ordering where safe.
 * Uses Lamport's algorithm for lock-free SPSC queue.
 *
 * @tparam T Element type (must be trivially copyable for best performance)
 * @tparam Capacity Buffer capacity (must be power of 2)
 */
template<typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be greater than 0");

public:
    /**
     * @brief Construct a new SPSC Ring Buffer
     */
    SPSCRingBuffer()
        : write_pos_(0)
        , read_pos_(0) {
        // Initialize buffer with default constructed elements
        for (size_t i = 0; i < Capacity; ++i) {
            new (&buffer_[i]) T();
        }
    }

    ~SPSCRingBuffer() {
        // Destroy remaining elements if T is not trivially destructible
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < Capacity; ++i) {
                reinterpret_cast<T*>(&buffer_[i])->~T();
            }
        }
    }

    // Non-copyable, non-movable
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer(SPSCRingBuffer&&) = delete;
    SPSCRingBuffer& operator=(SPSCRingBuffer&&) = delete;

    /**
     * @brief Try to push an element (producer only)
     *
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    bool try_push(const T& item) {
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & (Capacity - 1);

        // Check if buffer is full
        // Use acquire to synchronize with consumer's release
        if (next_write == read_pos_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }

        // Write data
        *reinterpret_cast<T*>(&buffer_[current_write]) = item;

        // Publish write position with release semantics
        // Ensures data write happens before position update
        write_pos_.store(next_write, std::memory_order_release);

        return true;
    }

    /**
     * @brief Try to push an element (move semantics)
     *
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    bool try_push(T&& item) {
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & (Capacity - 1);

        if (next_write == read_pos_.load(std::memory_order_acquire)) {
            return false;
        }

        *reinterpret_cast<T*>(&buffer_[current_write]) = std::move(item);
        write_pos_.store(next_write, std::memory_order_release);

        return true;
    }

    /**
     * @brief Try to pop an element (consumer only)
     *
     * @param item Output parameter for popped item
     * @return true if successful, false if buffer is empty
     */
    bool try_pop(T& item) {
        const size_t current_read = read_pos_.load(std::memory_order_relaxed);

        // Check if buffer is empty
        // Use acquire to synchronize with producer's release
        if (current_read == write_pos_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }

        // Read data (use move semantics for move-only types)
        item = std::move(*reinterpret_cast<T*>(&buffer_[current_read]));

        const size_t next_read = (current_read + 1) & (Capacity - 1);

        // Publish read position with release semantics
        read_pos_.store(next_read, std::memory_order_release);

        return true;
    }

    /**
     * @brief Check if buffer is empty (approximate)
     *
     * Note: This is an approximate check and may race with concurrent operations
     */
    bool is_empty() const {
        return read_pos_.load(std::memory_order_relaxed) ==
               write_pos_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Check if buffer is full (approximate)
     */
    bool is_full() const {
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & (Capacity - 1);
        return next_write == read_pos_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get approximate size of buffer
     */
    size_t size() const {
        const size_t w = write_pos_.load(std::memory_order_relaxed);
        const size_t r = read_pos_.load(std::memory_order_relaxed);
        return (w - r) & (Capacity - 1);
    }

    /**
     * @brief Get buffer capacity
     */
    static constexpr size_t capacity() {
        return Capacity;
    }

private:
    // Cache-line aligned positions to prevent false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_pos_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_pos_;

    // Buffer storage using aligned_storage for proper alignment
    alignas(CACHE_LINE_SIZE) typename std::aligned_storage<sizeof(T), alignof(T)>::type buffer_[Capacity];
};

/**
 * @brief Lock-free Multi Producer Multi Consumer Ring Buffer
 *
 * Uses CAS (Compare-And-Swap) operations for thread-safe MPMC access.
 * Based on bounded MPMC queue algorithm.
 *
 * @tparam T Element type
 * @tparam Capacity Buffer capacity (must be power of 2)
 */
template<typename T, size_t Capacity>
class MPMCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be greater than 0");

    struct Cell {
        std::atomic<size_t> sequence;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type data;
    };

public:
    MPMCRingBuffer()
        : enqueue_pos_(0)
        , dequeue_pos_(0) {
        // Initialize sequence numbers
        for (size_t i = 0; i < Capacity; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~MPMCRingBuffer() {
        // Destroy remaining elements if T is not trivially destructible
        if constexpr (!std::is_trivially_destructible_v<T>) {
            // Note: In a proper implementation, we'd track which cells are occupied
            // For now, assuming all cleanup is handled by users
        }
    }

    // Non-copyable, non-movable
    MPMCRingBuffer(const MPMCRingBuffer&) = delete;
    MPMCRingBuffer& operator=(const MPMCRingBuffer&) = delete;
    MPMCRingBuffer(MPMCRingBuffer&&) = delete;
    MPMCRingBuffer& operator=(MPMCRingBuffer&&) = delete;

    /**
     * @brief Try to enqueue an element (thread-safe for multiple producers)
     *
     * @param item Item to enqueue
     * @return true if successful, false if buffer is full
     */
    bool try_push(const T& item) {
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

        for (;;) {
            cell = &buffer_[pos & (Capacity - 1)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;

            if (dif == 0) {
                // Cell is available, try to claim it
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                // Buffer is full
                return false;
            } else {
                // Another producer claimed this position, update and retry
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        // Write data
        new (&cell->data) T(item);

        // Publish the item
        cell->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Try to enqueue an element (move semantics)
     */
    bool try_push(T&& item) {
        Cell* cell;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

        for (;;) {
            cell = &buffer_[pos & (Capacity - 1)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;

            if (dif == 0) {
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return false;
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }

        new (&cell->data) T(std::move(item));
        cell->sequence.store(pos + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Try to dequeue an element (thread-safe for multiple consumers)
     *
     * @param item Output parameter for dequeued item
     * @return true if successful, false if buffer is empty
     */
    bool try_pop(T& item) {
        Cell* cell;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

        for (;;) {
            cell = &buffer_[pos & (Capacity - 1)];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);

            if (dif == 0) {
                // Cell has data, try to claim it
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1,
                    std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                // Buffer is empty
                return false;
            } else {
                // Another consumer claimed this position, update and retry
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }

        // Read data
        item = *reinterpret_cast<T*>(&cell->data);

        // Destroy the object if not trivially destructible
        if constexpr (!std::is_trivially_destructible_v<T>) {
            reinterpret_cast<T*>(&cell->data)->~T();
        }

        // Mark cell as available for producers
        cell->sequence.store(pos + Capacity, std::memory_order_release);

        return true;
    }

    /**
     * @brief Check if buffer is empty (approximate)
     */
    bool is_empty() const {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        const Cell* cell = &buffer_[pos & (Capacity - 1)];
        size_t seq = cell->sequence.load(std::memory_order_acquire);
        return (intptr_t)seq - (intptr_t)(pos + 1) < 0;
    }

    /**
     * @brief Check if buffer is full (approximate)
     */
    bool is_full() const {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        const Cell* cell = &buffer_[pos & (Capacity - 1)];
        size_t seq = cell->sequence.load(std::memory_order_acquire);
        return (intptr_t)seq - (intptr_t)pos < 0;
    }

    /**
     * @brief Get buffer capacity
     */
    static constexpr size_t capacity() {
        return Capacity;
    }

private:
    // Cache-line aligned positions
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> enqueue_pos_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> dequeue_pos_;

    // Buffer cells
    alignas(CACHE_LINE_SIZE) Cell buffer_[Capacity];
};

/**
 * @brief Ring buffer statistics for monitoring
 */
struct RingBufferStats {
    uint64_t total_pushes;
    uint64_t total_pops;
    uint64_t failed_pushes;  // Buffer full
    uint64_t failed_pops;    // Buffer empty
    uint64_t max_size;

    RingBufferStats()
        : total_pushes(0)
        , total_pops(0)
        , failed_pushes(0)
        , failed_pops(0)
        , max_size(0) {}
};

} // namespace ipc
} // namespace cdmf

#endif // RING_BUFFER_H
