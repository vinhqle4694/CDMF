#ifndef CDMF_THREAD_POOL_H
#define CDMF_THREAD_POOL_H

#include "blocking_queue.h"
#include <vector>
#include <thread>
#include <functional>
#include <future>
#include <memory>
#include <atomic>

namespace cdmf {
namespace utils {

/**
 * @brief High-performance thread pool for parallel task execution
 *
 * Features:
 * - Configurable number of worker threads
 * - Task queue with blocking operations
 * - Future-based result retrieval
 * - Graceful shutdown with pending task completion
 * - Exception propagation through futures
 * - Move-only semantics
 *
 * Design:
 * - Worker threads continuously poll the task queue
 * - Tasks are std::function<void()> wrapped with std::packaged_task
 * - Shutdown waits for all queued tasks to complete
 * - Thread-safe enqueueing and shutdown
 *
 * Example Usage:
 * @code
 * ThreadPool pool(4);  // 4 worker threads
 *
 * auto future = pool.enqueue([]() {
 *     return compute_result();
 * });
 *
 * int result = future.get();
 *
 * pool.shutdown();
 * pool.wait();
 * @endcode
 */
class ThreadPool {
public:
    /**
     * @brief Construct a thread pool with specified number of threads
     *
     * @param numThreads Number of worker threads (default: hardware concurrency)
     * @throws std::invalid_argument if numThreads is 0
     */
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());

    /**
     * @brief Destructor - automatically calls shutdown() and wait()
     */
    ~ThreadPool();

    // Prevent copying
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Prevent moving (due to running threads)
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Enqueue a task for execution
     *
     * @tparam F Callable type (function, lambda, functor)
     * @tparam Args Argument types
     * @param f Callable object
     * @param args Arguments to pass to f
     * @return std::future<ReturnType> Future for retrieving result
     *
     * @throws std::runtime_error if pool is shut down
     *
     * Example:
     * @code
     * auto future = pool.enqueue([](int x, int y) { return x + y; }, 5, 3);
     * int result = future.get();  // result == 8
     * @endcode
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using ReturnType = typename std::invoke_result<F, Args...>::type;

        if (shutdown_) {
            throw std::runtime_error("Cannot enqueue task: thread pool is shut down");
        }

        // Create a packaged task with bound arguments
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        // Wrap the packaged task in a void() function
        tasks_.push([task]() {
            (*task)();
        });

        return result;
    }

    /**
     * @brief Initiate graceful shutdown
     *
     * After calling shutdown():
     * - No new tasks can be enqueued
     * - Existing queued tasks will complete
     * - Worker threads will exit after queue is empty
     *
     * @note This is non-blocking; use wait() to wait for completion
     */
    void shutdown();

    /**
     * @brief Wait for all worker threads to finish
     *
     * Blocks until all worker threads have exited.
     * Should be called after shutdown().
     */
    void wait();

    /**
     * @brief Check if pool is shut down
     *
     * @return true if shutdown() has been called
     */
    bool isShutdown() const {
        return shutdown_;
    }

    /**
     * @brief Get number of worker threads
     *
     * @return size_t Thread count
     */
    size_t getThreadCount() const {
        return workers_.size();
    }

    /**
     * @brief Get number of pending tasks in queue
     *
     * @return size_t Number of queued tasks
     */
    size_t getPendingTaskCount() const {
        return tasks_.size();
    }

private:
    /**
     * @brief Worker thread function
     *
     * Continuously polls the task queue and executes tasks
     * until shutdown and queue is empty.
     */
    void workerThread();

    /**
     * @brief Task type: void function
     */
    using Task = std::function<void()>;

    /**
     * @brief Worker threads
     */
    std::vector<std::thread> workers_;

    /**
     * @brief Task queue
     */
    BlockingQueue<Task> tasks_;

    /**
     * @brief Shutdown flag
     */
    std::atomic<bool> shutdown_;
};

} // namespace utils
} // namespace cdmf

#endif // CDMF_THREAD_POOL_H
