#ifndef CDMF_BLOCKING_QUEUE_H
#define CDMF_BLOCKING_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <atomic>

namespace cdmf {
namespace utils {

/**
 * @brief Exception thrown when queue operations are attempted on a closed queue
 */
class QueueClosedException : public std::runtime_error {
public:
    QueueClosedException() : std::runtime_error("Queue is closed") {}
};

/**
 * @brief Thread-safe blocking queue with optional size limit
 *
 * A producer-consumer queue that supports:
 * - Thread-safe push/pop operations with mutex protection
 * - Blocking pop with optional timeout
 * - Optional maximum size limit
 * - Graceful shutdown with close() method
 * - Move semantics for efficiency
 *
 * @tparam T Element type (must be movable or copyable)
 *
 * Thread-Safety: All methods are thread-safe
 *
 * Example Usage:
 * @code
 * BlockingQueue<Task> taskQueue(100);  // Max 100 tasks
 *
 * // Producer thread
 * taskQueue.push(task);
 *
 * // Consumer thread
 * auto task = taskQueue.pop();  // Blocks until item available
 * if (task.has_value()) {
 *     process(task.value());
 * }
 *
 * // Shutdown
 * taskQueue.close();
 * @endcode
 */
template<typename T>
class BlockingQueue {
public:
    /**
     * @brief Construct a blocking queue
     *
     * @param maxSize Maximum queue size (0 = unlimited)
     */
    explicit BlockingQueue(size_t maxSize = 0)
        : maxSize_(maxSize)
        , closed_(false)
    {}

    /**
     * @brief Destructor - closes the queue
     */
    ~BlockingQueue() {
        close();
    }

    // Prevent copying
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    // Allow moving
    BlockingQueue(BlockingQueue&& other) noexcept
        : queue_(std::move(other.queue_))
        , maxSize_(other.maxSize_)
        , closed_(other.closed_.load())
    {}

    BlockingQueue& operator=(BlockingQueue&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_ = std::move(other.queue_);
            maxSize_ = other.maxSize_;
            closed_ = other.closed_.load();
        }
        return *this;
    }

    /**
     * @brief Push an item onto the queue (copy)
     *
     * Blocks if queue is at maximum size until space becomes available.
     *
     * @param item Item to push
     * @throws QueueClosedException if queue is closed
     */
    void push(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            throw QueueClosedException();
        }

        // Wait if queue is full
        if (maxSize_ > 0) {
            notFull_.wait(lock, [this]() {
                return queue_.size() < maxSize_ || closed_;
            });

            if (closed_) {
                throw QueueClosedException();
            }
        }

        queue_.push(item);
        notEmpty_.notify_one();
    }

    /**
     * @brief Push an item onto the queue (move)
     *
     * Blocks if queue is at maximum size until space becomes available.
     *
     * @param item Item to push (will be moved)
     * @throws QueueClosedException if queue is closed
     */
    void push(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (closed_) {
            throw QueueClosedException();
        }

        // Wait if queue is full
        if (maxSize_ > 0) {
            notFull_.wait(lock, [this]() {
                return queue_.size() < maxSize_ || closed_;
            });

            if (closed_) {
                throw QueueClosedException();
            }
        }

        queue_.push(std::move(item));
        notEmpty_.notify_one();
    }

    /**
     * @brief Try to push an item without blocking
     *
     * @param item Item to push
     * @return true if item was pushed, false if queue is full or closed
     */
    bool tryPush(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_ || (maxSize_ > 0 && queue_.size() >= maxSize_)) {
            return false;
        }

        queue_.push(item);
        notEmpty_.notify_one();
        return true;
    }

    /**
     * @brief Try to push an item without blocking (move version)
     *
     * @param item Item to push (will be moved if successful)
     * @return true if item was pushed, false if queue is full or closed
     */
    bool tryPush(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (closed_ || (maxSize_ > 0 && queue_.size() >= maxSize_)) {
            return false;
        }

        queue_.push(std::move(item));
        notEmpty_.notify_one();
        return true;
    }

    /**
     * @brief Pop an item from the queue (blocking)
     *
     * Blocks until an item is available or queue is closed.
     *
     * @return std::optional<T> Item if available, std::nullopt if queue closed
     */
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait until queue is not empty or closed
        notEmpty_.wait(lock, [this]() {
            return !queue_.empty() || closed_;
        });

        if (queue_.empty()) {
            return std::nullopt;  // Queue was closed
        }

        T item = std::move(queue_.front());
        queue_.pop();

        if (maxSize_ > 0) {
            notFull_.notify_one();
        }

        return item;
    }

    /**
     * @brief Pop an item from the queue with timeout
     *
     * Blocks until an item is available, timeout expires, or queue is closed.
     *
     * @param timeout Maximum time to wait
     * @return std::optional<T> Item if available, std::nullopt if timeout or closed
     */
    template<typename Rep, typename Period>
    std::optional<T> pop(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait with timeout
        bool success = notEmpty_.wait_for(lock, timeout, [this]() {
            return !queue_.empty() || closed_;
        });

        if (!success || queue_.empty()) {
            return std::nullopt;  // Timeout or closed
        }

        T item = std::move(queue_.front());
        queue_.pop();

        if (maxSize_ > 0) {
            notFull_.notify_one();
        }

        return item;
    }

    /**
     * @brief Try to pop an item without blocking
     *
     * @return std::optional<T> Item if available, std::nullopt otherwise
     */
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();

        if (maxSize_ > 0) {
            notFull_.notify_one();
        }

        return item;
    }

    /**
     * @brief Get current queue size
     *
     * @return size_t Number of items in queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief Check if queue is empty
     *
     * @return true if empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief Check if queue is closed
     *
     * @return true if closed
     */
    bool isClosed() const {
        return closed_;
    }

    /**
     * @brief Close the queue
     *
     * After closing:
     * - No new items can be pushed
     * - Existing items can still be popped
     * - Blocked threads are woken up
     */
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    /**
     * @brief Clear all items from the queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        if (maxSize_ > 0) {
            notFull_.notify_all();
        }
    }

    /**
     * @brief Get maximum queue size
     *
     * @return size_t Maximum size (0 = unlimited)
     */
    size_t maxSize() const {
        return maxSize_;
    }

private:
    std::queue<T> queue_;                  ///< Underlying queue
    size_t maxSize_;                       ///< Maximum queue size (0 = unlimited)
    std::atomic<bool> closed_;             ///< Queue closed flag
    mutable std::mutex mutex_;             ///< Mutex for thread safety
    std::condition_variable notEmpty_;     ///< Condition for consumers
    std::condition_variable notFull_;      ///< Condition for producers
};

} // namespace utils
} // namespace cdmf

#endif // CDMF_BLOCKING_QUEUE_H
