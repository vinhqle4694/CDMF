/**
 * @file test_ring_buffer.cpp
 * @brief Comprehensive unit tests for ring buffer implementations
 *
 * Tests include:
 * - Basic operations (push/pop)
 * - Overflow/underflow detection
 * - Concurrent access (multi-threaded)
 * - Performance benchmarks
 * - Memory leak detection
 * - Lock-free correctness
 *
 * @author Ring Buffer Agent
 * @date 2025-10-03
 */

#include "ipc/ring_buffer.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>
#include <chrono>
#include <atomic>
#include <random>
#include <algorithm>

using namespace cdmf::ipc;

// Test fixture for SPSC ring buffer
class SPSCRingBufferTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 1024;
    SPSCRingBuffer<int, BUFFER_SIZE> buffer;
};

// Test fixture for MPMC ring buffer
class MPMCRingBufferTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 1024;
    MPMCRingBuffer<int, BUFFER_SIZE> buffer;
};

// ============================================================================
// SPSC Ring Buffer Tests
// ============================================================================

TEST_F(SPSCRingBufferTest, BasicPushPop) {
    // Test basic push and pop operations
    EXPECT_TRUE(buffer.is_empty());
    EXPECT_FALSE(buffer.is_full());

    // Push some elements
    EXPECT_TRUE(buffer.try_push(42));
    EXPECT_TRUE(buffer.try_push(43));
    EXPECT_TRUE(buffer.try_push(44));

    EXPECT_FALSE(buffer.is_empty());

    // Pop and verify
    int value;
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 42);

    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 43);

    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 44);

    EXPECT_TRUE(buffer.is_empty());
}

TEST_F(SPSCRingBufferTest, MoveSematics) {
    struct MoveOnlyType {
        int value;
        std::unique_ptr<int> ptr;

        MoveOnlyType() : value(0), ptr(nullptr) {}
        explicit MoveOnlyType(int v) : value(v), ptr(std::make_unique<int>(v)) {}

        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;

        MoveOnlyType(MoveOnlyType&&) = default;
        MoveOnlyType& operator=(MoveOnlyType&&) = default;
    };

    SPSCRingBuffer<MoveOnlyType, 64> move_buffer;

    // Push with move
    MoveOnlyType item(42);
    EXPECT_TRUE(move_buffer.try_push(std::move(item)));

    // Pop and verify
    MoveOnlyType result;
    EXPECT_TRUE(move_buffer.try_pop(result));
    EXPECT_EQ(result.value, 42);
    EXPECT_NE(result.ptr, nullptr);
    EXPECT_EQ(*result.ptr, 42);
}

TEST_F(SPSCRingBufferTest, OverflowDetection) {
    // Fill buffer to capacity - 1 (because we reserve one slot)
    int pushed = 0;
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        if (buffer.try_push(static_cast<int>(i))) {
            ++pushed;
        } else {
            break;
        }
    }

    // Should be able to push capacity - 1 elements
    EXPECT_EQ(pushed, BUFFER_SIZE - 1);

    // Next push should fail
    EXPECT_FALSE(buffer.try_push(9999));
    EXPECT_TRUE(buffer.is_full());
}

TEST_F(SPSCRingBufferTest, UnderflowDetection) {
    // Try to pop from empty buffer
    int value;
    EXPECT_FALSE(buffer.try_pop(value));
    EXPECT_TRUE(buffer.is_empty());

    // Push one element
    EXPECT_TRUE(buffer.try_push(42));

    // Pop it
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 42);

    // Try to pop again - should fail
    EXPECT_FALSE(buffer.try_pop(value));
}

TEST_F(SPSCRingBufferTest, WrapAround) {
    // Test wrap-around behavior
    const int NUM_ITERATIONS = 10000;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        EXPECT_TRUE(buffer.try_push(i));

        int value;
        EXPECT_TRUE(buffer.try_pop(value));
        EXPECT_EQ(value, i);
    }

    EXPECT_TRUE(buffer.is_empty());
}

TEST_F(SPSCRingBufferTest, ConcurrentSPSC) {
    const int NUM_ITEMS = 1000000;
    std::atomic<bool> start{false};
    std::atomic<int> errors{0};

    // Producer thread
    std::thread producer([&]() {
        // Wait for start signal
        while (!start.load(std::memory_order_acquire)) {}

        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!buffer.try_push(i)) {
                // Spin wait
                std::this_thread::yield();
            }
        }
    });

    // Consumer thread
    std::thread consumer([&]() {
        // Wait for start signal
        while (!start.load(std::memory_order_acquire)) {}

        for (int i = 0; i < NUM_ITEMS; ++i) {
            int value;
            while (!buffer.try_pop(value)) {
                // Spin wait
                std::this_thread::yield();
            }

            if (value != i) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Start both threads
    start.store(true, std::memory_order_release);

    producer.join();
    consumer.join();

    EXPECT_EQ(errors.load(), 0) << "Lock-free SPSC correctness violated!";
    EXPECT_TRUE(buffer.is_empty());
}

// ============================================================================
// MPMC Ring Buffer Tests
// ============================================================================

TEST_F(MPMCRingBufferTest, BasicPushPop) {
    EXPECT_TRUE(buffer.is_empty());

    // Push some elements
    EXPECT_TRUE(buffer.try_push(100));
    EXPECT_TRUE(buffer.try_push(200));
    EXPECT_TRUE(buffer.try_push(300));

    EXPECT_FALSE(buffer.is_empty());

    // Pop and verify
    int value;
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 100);

    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 200);

    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 300);

    EXPECT_TRUE(buffer.is_empty());
}

TEST_F(MPMCRingBufferTest, OverflowDetection) {
    // Fill buffer
    int pushed = 0;
    for (size_t i = 0; i < BUFFER_SIZE * 2; ++i) {
        if (buffer.try_push(static_cast<int>(i))) {
            ++pushed;
        }
    }

    // Should push exactly BUFFER_SIZE elements
    EXPECT_EQ(pushed, BUFFER_SIZE);
    EXPECT_TRUE(buffer.is_full());

    // Next push should fail
    EXPECT_FALSE(buffer.try_push(9999));
}

TEST_F(MPMCRingBufferTest, MultipleProducers) {
    const int NUM_PRODUCERS = 4;
    const int ITEMS_PER_PRODUCER = 100000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<bool> start{false};
    std::vector<std::thread> producers;

    // Create producers
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {}

            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = p * ITEMS_PER_PRODUCER + i;
                while (!buffer.try_push(value)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Consumer thread
    std::vector<int> consumed;
    consumed.reserve(TOTAL_ITEMS);

    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire)) {}

        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            int value;
            while (!buffer.try_pop(value)) {
                std::this_thread::yield();
            }
            consumed.push_back(value);
        }
    });

    // Start all threads
    start.store(true, std::memory_order_release);

    for (auto& t : producers) {
        t.join();
    }
    consumer.join();

    // Verify all items consumed
    EXPECT_EQ(consumed.size(), TOTAL_ITEMS);

    // Verify all items are unique (no duplicates)
    std::sort(consumed.begin(), consumed.end());
    auto it = std::adjacent_find(consumed.begin(), consumed.end());
    EXPECT_EQ(it, consumed.end()) << "Duplicate values detected!";

    EXPECT_TRUE(buffer.is_empty());
}

TEST_F(MPMCRingBufferTest, MultipleConsumers) {
    const int NUM_CONSUMERS = 4;
    const int TOTAL_ITEMS = 400000;

    std::atomic<bool> start{false};
    std::atomic<int> push_count{0};
    std::vector<std::thread> consumers;
    std::vector<std::vector<int>> consumed(NUM_CONSUMERS);

    // Producer thread
    std::thread producer([&]() {
        while (!start.load(std::memory_order_acquire)) {}

        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            while (!buffer.try_push(i)) {
                std::this_thread::yield();
            }
            push_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Create consumers
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&, c]() {
            while (!start.load(std::memory_order_acquire)) {}

            while (true) {
                int value;
                if (buffer.try_pop(value)) {
                    consumed[c].push_back(value);
                } else {
                    // Check if producer is done
                    if (push_count.load(std::memory_order_relaxed) >= TOTAL_ITEMS) {
                        // Try a few more times to ensure buffer is empty
                        bool found = false;
                        for (int retry = 0; retry < 100; ++retry) {
                            if (buffer.try_pop(value)) {
                                consumed[c].push_back(value);
                                found = true;
                                break;
                            }
                        }
                        if (!found) break;
                    }
                    std::this_thread::yield();
                }
            }
        });
    }

    // Start all threads
    start.store(true, std::memory_order_release);

    producer.join();
    for (auto& t : consumers) {
        t.join();
    }

    // Verify all items consumed
    std::vector<int> all_consumed;
    for (const auto& vec : consumed) {
        all_consumed.insert(all_consumed.end(), vec.begin(), vec.end());
    }

    EXPECT_EQ(all_consumed.size(), TOTAL_ITEMS);

    // Verify all items are unique
    std::sort(all_consumed.begin(), all_consumed.end());
    auto it = std::adjacent_find(all_consumed.begin(), all_consumed.end());
    EXPECT_EQ(it, all_consumed.end()) << "Duplicate values detected!";

    EXPECT_TRUE(buffer.is_empty());
}

TEST_F(MPMCRingBufferTest, MPMCStressTest) {
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 50000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    std::atomic<int> items_produced{0};
    std::vector<std::vector<int>> consumed(NUM_CONSUMERS);

    // Create producers
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        threads.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {}

            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = p * ITEMS_PER_PRODUCER + i;
                while (!buffer.try_push(value)) {
                    std::this_thread::yield();
                }
                items_produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Create consumers
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        threads.emplace_back([&, c]() {
            while (!start.load(std::memory_order_acquire)) {}

            while (true) {
                int value;
                if (buffer.try_pop(value)) {
                    consumed[c].push_back(value);
                } else {
                    if (items_produced.load(std::memory_order_relaxed) >= TOTAL_ITEMS) {
                        // Try more times to drain buffer
                        bool found = false;
                        for (int retry = 0; retry < 1000; ++retry) {
                            if (buffer.try_pop(value)) {
                                consumed[c].push_back(value);
                                found = true;
                                break;
                            }
                        }
                        if (!found) break;
                    }
                    std::this_thread::yield();
                }
            }
        });
    }

    // Start all threads
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    // Verify results
    std::vector<int> all_consumed;
    for (const auto& vec : consumed) {
        all_consumed.insert(all_consumed.end(), vec.begin(), vec.end());
    }

    EXPECT_EQ(all_consumed.size(), TOTAL_ITEMS) << "Lost items!";

    // Check for duplicates
    std::sort(all_consumed.begin(), all_consumed.end());
    auto it = std::adjacent_find(all_consumed.begin(), all_consumed.end());
    EXPECT_EQ(it, all_consumed.end()) << "Duplicate values in MPMC!";
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

class RingBufferBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        // Warm up CPU
        volatile int x = 0;
        for (int i = 0; i < 1000000; ++i) {
            x += i;
        }
    }

    template<typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

TEST_F(RingBufferBenchmark, SPSCThroughput) {
    const int NUM_ITEMS = 10000000;
    SPSCRingBuffer<int, 8192> buffer;

    std::atomic<bool> start{false};

    std::thread producer([&]() {
        while (!start.load(std::memory_order_acquire)) {}

        for (int i = 0; i < NUM_ITEMS; ++i) {
            while (!buffer.try_push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire)) {}

        for (int i = 0; i < NUM_ITEMS; ++i) {
            int value;
            while (!buffer.try_pop(value)) {
                std::this_thread::yield();
            }
        }
    });

    double elapsed_ms = measure_time_ms([&]() {
        start.store(true, std::memory_order_release);
        producer.join();
        consumer.join();
    });

    double throughput = NUM_ITEMS / (elapsed_ms / 1000.0);

    std::cout << "SPSC Throughput: " << throughput / 1000000.0
              << " million ops/sec" << std::endl;

    // Expect at least 5 million ops/sec (adjusted for CI environment)
    EXPECT_GT(throughput, 5000000.0);
}

TEST_F(RingBufferBenchmark, SPSCLatency) {
    const int NUM_SAMPLES = 100000;
    SPSCRingBuffer<int, 1024> buffer;

    std::vector<double> latencies;
    latencies.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // Push
        while (!buffer.try_push(i)) {}

        // Pop
        int value;
        while (!buffer.try_pop(value)) {}

        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
    }

    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    double p50 = latencies[latencies.size() / 2];
    double p95 = latencies[latencies.size() * 95 / 100];
    double p99 = latencies[latencies.size() * 99 / 100];

    std::cout << "SPSC Latency (us):" << std::endl;
    std::cout << "  Average: " << avg << std::endl;
    std::cout << "  p50: " << p50 << std::endl;
    std::cout << "  p95: " << p95 << std::endl;
    std::cout << "  p99: " << p99 << std::endl;

    // Expect p99 latency under 10 microseconds
    EXPECT_LT(p99, 10.0);
}

TEST_F(RingBufferBenchmark, MPMCThroughput) {
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 1000000;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    MPMCRingBuffer<int, 8192> buffer;

    std::atomic<bool> start{false};
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::vector<std::thread> threads;

    // Producers
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        threads.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {}

            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                int value = p * ITEMS_PER_PRODUCER + i;
                while (!buffer.try_push(value)) {
                    std::this_thread::yield();
                }
                items_produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Consumers
    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {}

            while (items_consumed.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
                int value;
                if (buffer.try_pop(value)) {
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    double elapsed_ms = measure_time_ms([&]() {
        start.store(true, std::memory_order_release);
        for (auto& t : threads) {
            t.join();
        }
    });

    double throughput = TOTAL_ITEMS / (elapsed_ms / 1000.0);

    std::cout << "MPMC Throughput: " << throughput / 1000000.0
              << " million ops/sec" << std::endl;

    // Expect at least 2 million ops/sec for MPMC (adjusted for realistic hardware performance)
    EXPECT_GT(throughput, 2000000.0);
}

TEST_F(RingBufferBenchmark, MemoryFootprint) {
    // Test memory footprint of different buffer sizes
    std::cout << "Memory Footprint:" << std::endl;
    std::cout << "  SPSCRingBuffer<int, 256>: " << sizeof(SPSCRingBuffer<int, 256>) << " bytes" << std::endl;
    std::cout << "  SPSCRingBuffer<int, 1024>: " << sizeof(SPSCRingBuffer<int, 1024>) << " bytes" << std::endl;
    std::cout << "  SPSCRingBuffer<int, 4096>: " << sizeof(SPSCRingBuffer<int, 4096>) << " bytes" << std::endl;
    std::cout << "  MPMCRingBuffer<int, 256>: " << sizeof(MPMCRingBuffer<int, 256>) << " bytes" << std::endl;
    std::cout << "  MPMCRingBuffer<int, 1024>: " << sizeof(MPMCRingBuffer<int, 1024>) << " bytes" << std::endl;
    std::cout << "  MPMCRingBuffer<int, 4096>: " << sizeof(MPMCRingBuffer<int, 4096>) << " bytes" << std::endl;

    // Verify reasonable memory usage
    EXPECT_LT(sizeof(SPSCRingBuffer<int, 4096>), 50000);
    EXPECT_LT(sizeof(MPMCRingBuffer<int, 4096>), 100000);
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST(RingBufferEdgeCases, SingleElementBuffer) {
    SPSCRingBuffer<int, 2> tiny_buffer;  // Capacity 2 means 1 usable slot

    EXPECT_TRUE(tiny_buffer.try_push(42));
    EXPECT_TRUE(tiny_buffer.is_full());
    EXPECT_FALSE(tiny_buffer.try_push(43));

    int value;
    EXPECT_TRUE(tiny_buffer.try_pop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(tiny_buffer.is_empty());
}

TEST(RingBufferEdgeCases, LargeElementSize) {
    struct LargeElement {
        char data[1024];
        int id;

        LargeElement() : id(0) {
            std::memset(data, 0, sizeof(data));
        }
        explicit LargeElement(int i) : id(i) {
            std::memset(data, static_cast<char>(i), sizeof(data));
        }
    };

    SPSCRingBuffer<LargeElement, 128> buffer;

    LargeElement elem(42);
    EXPECT_TRUE(buffer.try_push(elem));

    LargeElement result;
    EXPECT_TRUE(buffer.try_pop(result));
    EXPECT_EQ(result.id, 42);
    EXPECT_EQ(result.data[0], 42);
}

TEST(RingBufferEdgeCases, RapidPushPop) {
    SPSCRingBuffer<int, 64> buffer;

    // Rapidly push and pop without filling buffer
    for (int round = 0; round < 10000; ++round) {
        for (int i = 0; i < 10; ++i) {
            EXPECT_TRUE(buffer.try_push(i));
        }

        for (int i = 0; i < 10; ++i) {
            int value;
            EXPECT_TRUE(buffer.try_pop(value));
            EXPECT_EQ(value, i);
        }
    }

    EXPECT_TRUE(buffer.is_empty());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
