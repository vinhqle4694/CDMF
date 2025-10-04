#include <gtest/gtest.h>
#include "utils/blocking_queue.h"
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <algorithm>

using namespace cdmf::utils;
using namespace std::chrono_literals;

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST(BlockingQueueTest, Construction) {
    EXPECT_NO_THROW({
        BlockingQueue<int> queue;
    });

    EXPECT_NO_THROW({
        BlockingQueue<int> queue(100);
    });
}

TEST(BlockingQueueTest, PushAndPop) {
    BlockingQueue<int> queue;

    queue.push(42);
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());

    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 42);
    EXPECT_TRUE(queue.empty());
}

TEST(BlockingQueueTest, PushMove) {
    BlockingQueue<std::string> queue;

    std::string str = "hello";
    queue.push(std::move(str));
    EXPECT_TRUE(str.empty());  // String was moved

    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), "hello");
}

TEST(BlockingQueueTest, MultipleItems) {
    BlockingQueue<int> queue;

    for (int i = 0; i < 10; ++i) {
        queue.push(i);
    }

    EXPECT_EQ(queue.size(), 10);

    for (int i = 0; i < 10; ++i) {
        auto item = queue.pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(item.value(), i);
    }

    EXPECT_TRUE(queue.empty());
}

// ============================================================================
// TryPush/TryPop Tests
// ============================================================================

TEST(BlockingQueueTest, TryPush) {
    BlockingQueue<int> queue(2);  // Max size 2

    EXPECT_TRUE(queue.tryPush(1));
    EXPECT_TRUE(queue.tryPush(2));
    EXPECT_FALSE(queue.tryPush(3));  // Queue full

    EXPECT_EQ(queue.size(), 2);
}

TEST(BlockingQueueTest, TryPop) {
    BlockingQueue<int> queue;

    auto item = queue.tryPop();
    EXPECT_FALSE(item.has_value());  // Queue empty

    queue.push(42);

    item = queue.tryPop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 42);

    item = queue.tryPop();
    EXPECT_FALSE(item.has_value());  // Queue empty again
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST(BlockingQueueTest, PopWithTimeout) {
    BlockingQueue<int> queue;

    auto start = std::chrono::steady_clock::now();
    auto item = queue.pop(100ms);
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(item.has_value());  // Timeout
    EXPECT_GE(duration, 100ms);
    EXPECT_LT(duration, 200ms);  // Should not wait much longer
}

TEST(BlockingQueueTest, PopWithTimeoutSuccess) {
    BlockingQueue<int> queue;

    // Push from another thread after 50ms
    std::thread producer([&queue]() {
        std::this_thread::sleep_for(50ms);
        queue.push(42);
    });

    auto item = queue.pop(200ms);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 42);

    producer.join();
}

// ============================================================================
// Size Limit Tests
// ============================================================================

TEST(BlockingQueueTest, SizeLimit) {
    BlockingQueue<int> queue(3);

    queue.push(1);
    queue.push(2);
    queue.push(3);

    EXPECT_EQ(queue.size(), 3);
    EXPECT_EQ(queue.maxSize(), 3);

    // This would block, so we test it doesn't complete immediately
    std::atomic<bool> pushCompleted(false);
    std::thread producer([&queue, &pushCompleted]() {
        queue.push(4);  // Should block
        pushCompleted = true;
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(pushCompleted);  // Still blocked

    // Pop one item to make space
    queue.pop();
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(pushCompleted);  // Now completed

    producer.join();
    EXPECT_EQ(queue.size(), 3);
}

TEST(BlockingQueueTest, UnlimitedSize) {
    BlockingQueue<int> queue(0);  // Unlimited

    for (int i = 0; i < 1000; ++i) {
        queue.push(i);
    }

    EXPECT_EQ(queue.size(), 1000);
    EXPECT_EQ(queue.maxSize(), 0);
}

// ============================================================================
// Close Tests
// ============================================================================

TEST(BlockingQueueTest, CloseQueue) {
    BlockingQueue<int> queue;

    queue.push(42);
    queue.close();

    EXPECT_TRUE(queue.isClosed());

    // Can still pop existing items
    auto item = queue.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 42);

    // Pop returns nullopt when closed and empty
    item = queue.pop();
    EXPECT_FALSE(item.has_value());
}

TEST(BlockingQueueTest, PushAfterClose) {
    BlockingQueue<int> queue;
    queue.close();

    EXPECT_THROW({
        queue.push(42);
    }, QueueClosedException);
}

TEST(BlockingQueueTest, CloseUnblocksWaiters) {
    BlockingQueue<int> queue;

    std::atomic<int> poppedCount(0);
    std::vector<std::thread> consumers;

    // Start multiple blocking consumers
    for (int i = 0; i < 5; ++i) {
        consumers.emplace_back([&queue, &poppedCount]() {
            auto item = queue.pop();
            if (item.has_value()) {
                ++poppedCount;
            }
        });
    }

    std::this_thread::sleep_for(50ms);

    // Close the queue
    queue.close();

    // All consumers should wake up
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(poppedCount, 0);  // No items were popped
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST(BlockingQueueTest, ClearQueue) {
    BlockingQueue<int> queue;

    for (int i = 0; i < 10; ++i) {
        queue.push(i);
    }

    EXPECT_EQ(queue.size(), 10);

    queue.clear();

    EXPECT_EQ(queue.size(), 0);
    EXPECT_TRUE(queue.empty());
}

// ============================================================================
// Producer-Consumer Tests
// ============================================================================

TEST(BlockingQueueTest, SingleProducerSingleConsumer) {
    BlockingQueue<int> queue;
    const int itemCount = 1000;
    std::atomic<int> sum(0);

    std::thread producer([&queue, itemCount]() {
        for (int i = 0; i < itemCount; ++i) {
            queue.push(i);
        }
        queue.close();
    });

    std::thread consumer([&queue, &sum]() {
        while (true) {
            auto item = queue.pop();
            if (!item.has_value()) break;
            sum += item.value();
        }
    });

    producer.join();
    consumer.join();

    int expectedSum = (itemCount - 1) * itemCount / 2;  // Sum of 0 to itemCount-1
    EXPECT_EQ(sum, expectedSum);
}

TEST(BlockingQueueTest, MultipleProducersMultipleConsumers) {
    BlockingQueue<int> queue;
    const int producerCount = 4;
    const int consumerCount = 4;
    const int itemsPerProducer = 250;
    std::atomic<int> sum(0);

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Start producers
    for (int p = 0; p < producerCount; ++p) {
        producers.emplace_back([&queue, itemsPerProducer]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                queue.push(1);  // Each item contributes 1
            }
        });
    }

    // Start consumers
    for (int c = 0; c < consumerCount; ++c) {
        consumers.emplace_back([&queue, &sum]() {
            while (true) {
                auto item = queue.pop(100ms);
                if (!item.has_value()) break;
                sum += item.value();
            }
        });
    }

    // Wait for producers
    for (auto& t : producers) {
        t.join();
    }

    // Close queue after all producers finish
    std::this_thread::sleep_for(50ms);
    queue.close();

    // Wait for consumers
    for (auto& t : consumers) {
        t.join();
    }

    int expectedSum = producerCount * itemsPerProducer;
    EXPECT_EQ(sum, expectedSum);
}

// ============================================================================
// Thread Safety Stress Tests
// ============================================================================

TEST(BlockingQueueTest, ConcurrentPushPop) {
    BlockingQueue<int> queue(100);
    std::atomic<bool> stopFlag(false);
    std::atomic<int> pushCount(0);
    std::atomic<int> popCount(0);

    auto pushFunc = [&queue, &stopFlag, &pushCount]() {
        for (int i = 0; i < 1000; ++i) {
            queue.push(i);
            ++pushCount;
        }
    };

    auto popFunc = [&queue, &stopFlag, &popCount]() {
        while (!stopFlag) {
            auto item = queue.pop(10ms);
            if (item.has_value()) {
                ++popCount;
            }
        }
    };

    std::vector<std::thread> threads;

    // 4 pushers, 4 poppers
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(pushFunc);
        threads.emplace_back(popFunc);
    }

    // Wait for pushers to finish
    std::this_thread::sleep_for(1s);
    stopFlag = true;

    for (auto& t : threads) {
        t.join();
    }

    // Drain remaining items
    while (auto item = queue.tryPop()) {
        ++popCount;
    }

    EXPECT_EQ(popCount, pushCount);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST(BlockingQueueTest, MoveConstruction) {
    BlockingQueue<int> queue1;
    queue1.push(1);
    queue1.push(2);

    BlockingQueue<int> queue2(std::move(queue1));

    EXPECT_EQ(queue2.size(), 2);

    auto item = queue2.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 1);
}

TEST(BlockingQueueTest, MoveAssignment) {
    BlockingQueue<int> queue1;
    queue1.push(1);
    queue1.push(2);

    BlockingQueue<int> queue2;
    queue2 = std::move(queue1);

    EXPECT_EQ(queue2.size(), 2);

    auto item = queue2.pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item.value(), 1);
}

// ============================================================================
// Custom Type Tests
// ============================================================================

struct CustomType {
    int value;
    std::string data;

    CustomType(int v, std::string d) : value(v), data(std::move(d)) {}
};

TEST(BlockingQueueTest, CustomTypes) {
    BlockingQueue<CustomType> queue;

    queue.push(CustomType(1, "hello"));
    queue.push(CustomType(2, "world"));

    auto item1 = queue.pop();
    ASSERT_TRUE(item1.has_value());
    EXPECT_EQ(item1.value().value, 1);
    EXPECT_EQ(item1.value().data, "hello");

    auto item2 = queue.pop();
    ASSERT_TRUE(item2.has_value());
    EXPECT_EQ(item2.value().value, 2);
    EXPECT_EQ(item2.value().data, "world");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
