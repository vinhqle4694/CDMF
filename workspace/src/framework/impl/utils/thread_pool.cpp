#include "utils/thread_pool.h"
#include <stdexcept>

namespace cdmf {
namespace utils {

ThreadPool::ThreadPool(size_t numThreads)
    : tasks_(0)  // Unlimited queue size
    , shutdown_(false)
{
    if (numThreads == 0) {
        throw std::invalid_argument("Thread pool must have at least one thread");
    }

    // Start worker threads
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::workerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
    wait();
}

void ThreadPool::shutdown() {
    if (shutdown_.exchange(true)) {
        return;  // Already shut down
    }

    // Close the task queue to wake up all waiting workers
    tasks_.close();
}

void ThreadPool::wait() {
    // Join all worker threads
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerThread() {
    while (true) {
        // Wait for a task
        auto task = tasks_.pop();

        if (!task.has_value()) {
            // Queue closed and empty - exit thread
            break;
        }

        // Execute the task
        try {
            task.value()();
        } catch (...) {
            // Task threw an exception
            // Exception is captured by std::packaged_task and propagated through future
            // We just continue processing other tasks
        }
    }
}

} // namespace utils
} // namespace cdmf
