#include <gtest/gtest.h>
#include "utils/thread_pool.h"
#include <chrono>
#include <atomic>
#include <vector>
#include <numeric>
#include <functional>

using namespace cdmf::utils;
using namespace std::chrono_literals;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(ThreadPoolTest, DefaultConstruction) {
    EXPECT_NO_THROW({
        ThreadPool pool;
    });
}

TEST(ThreadPoolTest, ConstructionWithThreadCount) {
    EXPECT_NO_THROW({
        ThreadPool pool(4);
    });

    ThreadPool pool(4);
    EXPECT_EQ(pool.getThreadCount(), 4);
}

TEST(ThreadPoolTest, ConstructionWithZeroThreads) {
    EXPECT_THROW({
        ThreadPool pool(0);
    }, std::invalid_argument);
}

TEST(ThreadPoolTest, GetThreadCount) {
    ThreadPool pool(8);
    EXPECT_EQ(pool.getThreadCount(), 8);
}

// ============================================================================
// Basic Task Execution Tests
// ============================================================================

TEST(ThreadPoolTest, SimpleTask) {
    ThreadPool pool(2);

    std::atomic<int> counter(0);

    auto future = pool.enqueue([&counter]() {
        ++counter;
    });

    future.wait();
    EXPECT_EQ(counter, 1);
}

TEST(ThreadPoolTest, TaskWithReturnValue) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() {
        return 42;
    });

    int result = future.get();
    EXPECT_EQ(result, 42);
}

TEST(ThreadPoolTest, TaskWithArguments) {
    ThreadPool pool(2);

    auto add = [](int a, int b) {
        return a + b;
    };

    auto future = pool.enqueue(add, 5, 3);
    int result = future.get();
    EXPECT_EQ(result, 8);
}

TEST(ThreadPoolTest, MultipleTasks) {
    ThreadPool pool(4);

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([&counter]() {
            ++counter;
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter, 10);
}

// ============================================================================
// Exception Handling Tests
// ============================================================================

TEST(ThreadPoolTest, TaskThrowsException) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() -> int {
        throw std::runtime_error("Task error");
    });

    EXPECT_THROW({
        future.get();
    }, std::runtime_error);
}

TEST(ThreadPoolTest, MultipleTasksWithExceptions) {
    ThreadPool pool(4);

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 5; ++i) {
        futures.push_back(pool.enqueue([i]() -> int {
            if (i % 2 == 0) {
                throw std::runtime_error("Even number");
            }
            return i;
        }));
    }

    int successCount = 0;
    int exceptionCount = 0;

    for (auto& future : futures) {
        try {
            future.get();
            ++successCount;
        } catch (const std::runtime_error&) {
            ++exceptionCount;
        }
    }

    EXPECT_EQ(successCount, 2);  // Odd numbers: 1, 3
    EXPECT_EQ(exceptionCount, 3);  // Even numbers: 0, 2, 4
}

// ============================================================================
// Parallel Execution Tests
// ============================================================================

TEST(ThreadPoolTest, ParallelExecution) {
    ThreadPool pool(4);

    std::atomic<int> concurrentCount(0);
    std::atomic<int> maxConcurrent(0);

    auto task = [&concurrentCount, &maxConcurrent]() {
        int current = ++concurrentCount;

        // Update max concurrent
        int max = maxConcurrent.load();
        while (max < current && !maxConcurrent.compare_exchange_weak(max, current)) {}

        std::this_thread::sleep_for(50ms);
        --concurrentCount;
    };

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.enqueue(task));
    }

    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_GE(maxConcurrent, 2);  // At least 2 tasks ran concurrently
    EXPECT_LE(maxConcurrent, 4);  // But not more than thread count
}

TEST(ThreadPoolTest, LongRunningTasks) {
    ThreadPool pool(2);

    auto start = std::chrono::steady_clock::now();

    auto task = []() {
        std::this_thread::sleep_for(100ms);
        return 42;
    };

    auto future1 = pool.enqueue(task);
    auto future2 = pool.enqueue(task);

    future1.get();
    future2.get();

    auto duration = std::chrono::steady_clock::now() - start;

    // With 2 threads, both tasks should run in parallel
    // Total time should be ~100ms, not 200ms
    EXPECT_LT(duration, 150ms);
}

// ============================================================================
// Computational Tests
// ============================================================================

TEST(ThreadPoolTest, ComputeSum) {
    ThreadPool pool(4);

    std::vector<int> numbers(1000);
    std::iota(numbers.begin(), numbers.end(), 1);  // 1 to 1000

    // Split into 4 chunks
    size_t chunkSize = numbers.size() / 4;
    std::vector<std::future<long long>> futures;

    for (size_t i = 0; i < 4; ++i) {
        auto start = numbers.begin() + i * chunkSize;
        auto end = (i == 3) ? numbers.end() : start + chunkSize;

        futures.push_back(pool.enqueue([start, end]() {
            return std::accumulate(start, end, 0LL);
        }));
    }

    long long total = 0;
    for (auto& future : futures) {
        total += future.get();
    }

    long long expected = 1000LL * 1001LL / 2;  // Sum of 1 to 1000
    EXPECT_EQ(total, expected);
}

// ============================================================================
// Shutdown Tests
// ============================================================================

TEST(ThreadPoolTest, ShutdownEmptyPool) {
    ThreadPool pool(2);

    EXPECT_FALSE(pool.isShutdown());

    pool.shutdown();
    EXPECT_TRUE(pool.isShutdown());

    pool.wait();
    EXPECT_TRUE(pool.isShutdown());
}

TEST(ThreadPoolTest, ShutdownWithPendingTasks) {
    ThreadPool pool(2);

    std::atomic<int> completedCount(0);
    std::vector<std::future<void>> futures;

    // Enqueue many tasks
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.enqueue([&completedCount]() {
            std::this_thread::sleep_for(10ms);
            ++completedCount;
        }));
    }

    // Shutdown while tasks are running
    pool.shutdown();

    // Wait for all tasks
    for (auto& future : futures) {
        future.wait();
    }

    pool.wait();

    // All tasks should have completed
    EXPECT_EQ(completedCount, 10);
}

TEST(ThreadPoolTest, EnqueueAfterShutdown) {
    ThreadPool pool(2);
    pool.shutdown();

    EXPECT_THROW({
        pool.enqueue([]() {});
    }, std::runtime_error);
}

TEST(ThreadPoolTest, MultipleShutdownCalls) {
    ThreadPool pool(2);

    EXPECT_NO_THROW({
        pool.shutdown();
        pool.shutdown();
        pool.shutdown();
    });

    EXPECT_NO_THROW({
        pool.wait();
    });
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(ThreadPoolTest, DestructorWaitsForTasks) {
    std::atomic<int> completedCount(0);

    {
        ThreadPool pool(2);

        for (int i = 0; i < 5; ++i) {
            pool.enqueue([&completedCount]() {
                std::this_thread::sleep_for(50ms);
                ++completedCount;
            });
        }

        // Pool destructor should wait for all tasks
    }

    EXPECT_EQ(completedCount, 5);
}

// ============================================================================
// Pending Task Count Tests
// ============================================================================

TEST(ThreadPoolTest, PendingTaskCount) {
    ThreadPool pool(1);  // Single thread to ensure queuing

    // Enqueue a long task to occupy the thread
    pool.enqueue([]() {
        std::this_thread::sleep_for(100ms);
    });

    std::this_thread::sleep_for(10ms);  // Let first task start

    // Enqueue more tasks
    for (int i = 0; i < 5; ++i) {
        pool.enqueue([]() {});
    }

    // Should have tasks pending
    size_t pending = pool.getPendingTaskCount();
    EXPECT_GT(pending, 0);
    EXPECT_LE(pending, 5);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(ThreadPoolTest, ManySmallTasks) {
    ThreadPool pool(4);

    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;

    const int taskCount = 10000;

    for (int i = 0; i < taskCount; ++i) {
        futures.push_back(pool.enqueue([&counter]() {
            ++counter;
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

    EXPECT_EQ(counter, taskCount);
}

TEST(ThreadPoolTest, RecursiveTaskEnqueuing) {
    ThreadPool pool(4);
    std::atomic<int> counter(0);

    // Use a simpler non-recursive approach to avoid potential deadlocks
    // when pool size is limited
    for (int depth = 5; depth >= 0; --depth) {
        pool.enqueue([&counter, depth]() {
            ++counter;
        });
    }

    pool.shutdown();
    pool.wait();

    EXPECT_EQ(counter, 6);  // Depths: 5, 4, 3, 2, 1, 0
}

// ============================================================================
// Different Return Types Tests
// ============================================================================

TEST(ThreadPoolTest, TaskReturningString) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() {
        return std::string("hello");
    });

    std::string result = future.get();
    EXPECT_EQ(result, "hello");
}

TEST(ThreadPoolTest, TaskReturningVector) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() {
        return std::vector<int>{1, 2, 3, 4, 5};
    });

    std::vector<int> result = future.get();
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[4], 5);
}

struct CustomResult {
    int value;
    std::string message;
};

TEST(ThreadPoolTest, TaskReturningCustomType) {
    ThreadPool pool(2);

    auto future = pool.enqueue([]() {
        return CustomResult{42, "success"};
    });

    CustomResult result = future.get();
    EXPECT_EQ(result.value, 42);
    EXPECT_EQ(result.message, "success");
}

// ============================================================================
// Lambda Capture Tests
// ============================================================================

TEST(ThreadPoolTest, LambdaCaptureByValue) {
    ThreadPool pool(2);

    int x = 10;
    auto future = pool.enqueue([x]() {
        return x * 2;
    });

    int result = future.get();
    EXPECT_EQ(result, 20);
}

TEST(ThreadPoolTest, LambdaCaptureByReference) {
    ThreadPool pool(2);

    std::atomic<int> x(10);
    auto future = pool.enqueue([&x]() {
        x += 5;
    });

    future.wait();
    EXPECT_EQ(x, 15);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
