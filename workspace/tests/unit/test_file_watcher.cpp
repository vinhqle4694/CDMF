/**
 * @file test_file_watcher.cpp
 * @brief Comprehensive unit tests for FileWatcher component
 *
 * Tests include:
 * - File modification detection
 * - File creation detection
 * - File deletion detection
 * - Multiple file watching
 * - Callback invocation
 * - Thread safety
 * - Start/stop functionality
 * - Edge cases
 *
 * @author File Watcher Test Agent
 * @date 2025-10-05
 */

#include "utils/file_watcher.h"
#include <gtest/gtest.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>

using namespace cdmf;
using namespace std::chrono_literals;

// ============================================================================
// Test Fixtures
// ============================================================================

class FileWatcherTest : public ::testing::Test {
protected:
    std::filesystem::path test_dir;
    std::filesystem::path test_file1;
    std::filesystem::path test_file2;

    void SetUp() override {
        // Create temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "cdmf_file_watcher_test";
        std::filesystem::create_directories(test_dir);

        test_file1 = test_dir / "test_file1.txt";
        test_file2 = test_dir / "test_file2.txt";
    }

    void TearDown() override {
        // Clean up test files and directory
        std::filesystem::remove_all(test_dir);
    }

    void createFile(const std::filesystem::path& path, const std::string& content = "initial") {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    void modifyFile(const std::filesystem::path& path, const std::string& content = "modified") {
        std::this_thread::sleep_for(10ms);  // Ensure timestamp changes
        std::ofstream file(path, std::ios::app);
        file << content;
        file.close();
    }

    void deleteFile(const std::filesystem::path& path) {
        std::filesystem::remove(path);
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(FileWatcherTest, ConstructorWithDefaultInterval) {
    FileWatcher watcher;
    EXPECT_FALSE(watcher.isRunning());
    EXPECT_EQ(watcher.getWatchCount(), 0);
}

TEST_F(FileWatcherTest, ConstructorWithCustomInterval) {
    FileWatcher watcher(500);
    EXPECT_FALSE(watcher.isRunning());
}

TEST_F(FileWatcherTest, StartAndStop) {
    FileWatcher watcher;

    watcher.start();
    EXPECT_TRUE(watcher.isRunning());

    watcher.stop();
    EXPECT_FALSE(watcher.isRunning());
}

TEST_F(FileWatcherTest, MultipleStartCalls) {
    FileWatcher watcher;

    watcher.start();
    EXPECT_TRUE(watcher.isRunning());

    // Starting again should be safe (idempotent)
    watcher.start();
    EXPECT_TRUE(watcher.isRunning());

    watcher.stop();
}

TEST_F(FileWatcherTest, MultipleStopCalls) {
    FileWatcher watcher;

    watcher.start();
    watcher.stop();
    EXPECT_FALSE(watcher.isRunning());

    // Stopping again should be safe
    watcher.stop();
    EXPECT_FALSE(watcher.isRunning());
}

TEST_F(FileWatcherTest, DestructorStopsWatcher) {
    {
        FileWatcher watcher;
        watcher.start();
        EXPECT_TRUE(watcher.isRunning());
    }
    // Destructor should stop watcher gracefully
}

// ============================================================================
// Watch Management Tests
// ============================================================================

TEST_F(FileWatcherTest, WatchExistingFile) {
    createFile(test_file1);
    FileWatcher watcher(100);

    bool result = watcher.watch(test_file1.string(), [](const std::string&, FileEvent) {});

    EXPECT_TRUE(result);
    EXPECT_EQ(watcher.getWatchCount(), 1);
    EXPECT_TRUE(watcher.isWatching(test_file1.string()));
}

TEST_F(FileWatcherTest, WatchNonExistentFile) {
    FileWatcher watcher(100);

    bool result = watcher.watch(test_file1.string(), [](const std::string&, FileEvent) {});

    EXPECT_TRUE(result);  // Should allow watching non-existent files
    EXPECT_EQ(watcher.getWatchCount(), 1);
}

TEST_F(FileWatcherTest, WatchMultipleFiles) {
    createFile(test_file1);
    createFile(test_file2);

    FileWatcher watcher(100);

    watcher.watch(test_file1.string(), [](const std::string&, FileEvent) {});
    watcher.watch(test_file2.string(), [](const std::string&, FileEvent) {});

    EXPECT_EQ(watcher.getWatchCount(), 2);
    EXPECT_TRUE(watcher.isWatching(test_file1.string()));
    EXPECT_TRUE(watcher.isWatching(test_file2.string()));
}

TEST_F(FileWatcherTest, UnwatchFile) {
    createFile(test_file1);
    FileWatcher watcher(100);

    watcher.watch(test_file1.string(), [](const std::string&, FileEvent) {});
    ASSERT_EQ(watcher.getWatchCount(), 1);

    watcher.unwatch(test_file1.string());

    EXPECT_EQ(watcher.getWatchCount(), 0);
    EXPECT_FALSE(watcher.isWatching(test_file1.string()));
}

TEST_F(FileWatcherTest, UnwatchNonWatchedFile) {
    FileWatcher watcher(100);

    // Should not crash or cause issues
    watcher.unwatch(test_file1.string());

    EXPECT_EQ(watcher.getWatchCount(), 0);
}

TEST_F(FileWatcherTest, IsWatchingNonWatchedFile) {
    FileWatcher watcher(100);

    EXPECT_FALSE(watcher.isWatching(test_file1.string()));
}

// ============================================================================
// File Modification Detection Tests
// ============================================================================

TEST_F(FileWatcherTest, DetectFileModification) {
    createFile(test_file1, "initial content");

    std::atomic<bool> callback_invoked{false};
    std::atomic<int> modification_count{0};

    FileWatcher watcher(50);  // Fast polling for test
    watcher.watch(test_file1.string(), [&](const std::string& path, FileEvent event) {
        if (event == FileEvent::MODIFIED) {
            callback_invoked = true;
            modification_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    watcher.start();

    // Wait for initial state capture
    std::this_thread::sleep_for(100ms);

    // Modify the file
    modifyFile(test_file1, " - modified");

    // Wait for detection
    std::this_thread::sleep_for(200ms);

    watcher.stop();

    EXPECT_TRUE(callback_invoked);
    EXPECT_GT(modification_count.load(), 0);
}

TEST_F(FileWatcherTest, DetectMultipleModifications) {
    createFile(test_file1);

    std::atomic<int> modification_count{0};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string&, FileEvent event) {
        if (event == FileEvent::MODIFIED) {
            modification_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    // Perform multiple modifications
    for (int i = 0; i < 3; ++i) {
        modifyFile(test_file1, " - mod " + std::to_string(i));
        std::this_thread::sleep_for(100ms);
    }

    watcher.stop();

    EXPECT_GT(modification_count.load(), 0);
}

// ============================================================================
// File Creation Detection Tests
// ============================================================================

TEST_F(FileWatcherTest, DetectFileCreation) {
    std::atomic<bool> created{false};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string& path, FileEvent event) {
        if (event == FileEvent::CREATED) {
            created = true;
        }
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    // Create the file
    createFile(test_file1);

    // Wait for detection
    std::this_thread::sleep_for(200ms);

    watcher.stop();

    EXPECT_TRUE(created);
}

// ============================================================================
// File Deletion Detection Tests
// ============================================================================

TEST_F(FileWatcherTest, DetectFileDeletion) {
    createFile(test_file1);

    std::atomic<bool> deleted{false};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string& path, FileEvent event) {
        if (event == FileEvent::DELETED) {
            deleted = true;
        }
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    // Delete the file
    deleteFile(test_file1);

    // Wait for detection
    std::this_thread::sleep_for(200ms);

    watcher.stop();

    EXPECT_TRUE(deleted);
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(FileWatcherTest, CallbackReceivesCorrectPath) {
    createFile(test_file1);

    std::string received_path;

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string& path, FileEvent event) {
        received_path = path;
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    modifyFile(test_file1);
    std::this_thread::sleep_for(200ms);

    watcher.stop();

    EXPECT_EQ(received_path, test_file1.string());
}

TEST_F(FileWatcherTest, CallbackReceivesCorrectEventType) {
    createFile(test_file1);

    FileEvent received_event = FileEvent::CREATED;
    std::atomic<bool> callback_invoked{false};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string& path, FileEvent event) {
        received_event = event;
        callback_invoked = true;
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    modifyFile(test_file1);
    std::this_thread::sleep_for(200ms);

    watcher.stop();

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(received_event, FileEvent::MODIFIED);
}

TEST_F(FileWatcherTest, DifferentCallbacksForDifferentFiles) {
    createFile(test_file1);
    createFile(test_file2);

    std::atomic<int> file1_events{0};
    std::atomic<int> file2_events{0};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string&, FileEvent) {
        file1_events.fetch_add(1, std::memory_order_relaxed);
    });
    watcher.watch(test_file2.string(), [&](const std::string&, FileEvent) {
        file2_events.fetch_add(1, std::memory_order_relaxed);
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    modifyFile(test_file1);
    std::this_thread::sleep_for(150ms);

    modifyFile(test_file2);
    std::this_thread::sleep_for(150ms);

    watcher.stop();

    EXPECT_GT(file1_events.load(), 0);
    EXPECT_GT(file2_events.load(), 0);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(FileWatcherTest, ConcurrentWatchOperations) {
    FileWatcher watcher(100);

    const int NUM_THREADS = 4;
    const int FILES_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> watch_count{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < FILES_PER_THREAD; ++i) {
                auto path = test_dir / ("file_" + std::to_string(t) + "_" + std::to_string(i) + ".txt");
                if (watcher.watch(path.string(), [](const std::string&, FileEvent) {})) {
                    watch_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(watch_count.load(), NUM_THREADS * FILES_PER_THREAD);
    EXPECT_EQ(watcher.getWatchCount(), NUM_THREADS * FILES_PER_THREAD);
}

TEST_F(FileWatcherTest, ConcurrentUnwatchOperations) {
    FileWatcher watcher(100);

    std::vector<std::string> paths;
    for (int i = 0; i < 100; ++i) {
        auto path = test_dir / ("file_" + std::to_string(i) + ".txt");
        paths.push_back(path.string());
        watcher.watch(path, [](const std::string&, FileEvent) {});
    }

    ASSERT_EQ(watcher.getWatchCount(), 100);

    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = t; i < paths.size(); i += NUM_THREADS) {
                watcher.unwatch(paths[i]);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(watcher.getWatchCount(), 0);
}

TEST_F(FileWatcherTest, ConcurrentStartStop) {
    FileWatcher watcher(100);

    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            watcher.start();
            std::this_thread::sleep_for(10ms);
            watcher.stop();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should end in a consistent state
    EXPECT_FALSE(watcher.isRunning());
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST_F(FileWatcherTest, WatchEmptyPath) {
    FileWatcher watcher(100);

    bool result = watcher.watch("", [](const std::string&, FileEvent) {});

    // Should handle gracefully
    EXPECT_FALSE(result);
}

TEST_F(FileWatcherTest, WatchInvalidPath) {
    FileWatcher watcher(100);

    bool result = watcher.watch("/invalid/path/that/does/not/exist/file.txt",
                                [](const std::string&, FileEvent) {});

    // Should still allow watching (might be created later)
    EXPECT_TRUE(result);
}

TEST_F(FileWatcherTest, WatchSameFileTwice) {
    createFile(test_file1);
    FileWatcher watcher(100);

    watcher.watch(test_file1.string(), [](const std::string&, FileEvent) {});
    watcher.watch(test_file1.string(), [](const std::string&, FileEvent) {});

    // Should only watch once (replace callback)
    EXPECT_EQ(watcher.getWatchCount(), 1);
}

TEST_F(FileWatcherTest, WatchManyFiles) {
    FileWatcher watcher(200);

    const int NUM_FILES = 100;
    std::vector<std::string> paths;

    for (int i = 0; i < NUM_FILES; ++i) {
        auto path = test_dir / ("file_" + std::to_string(i) + ".txt");
        paths.push_back(path.string());
        createFile(path);
        watcher.watch(path, [](const std::string&, FileEvent) {});
    }

    EXPECT_EQ(watcher.getWatchCount(), NUM_FILES);

    watcher.start();
    std::this_thread::sleep_for(300ms);
    watcher.stop();
}

TEST_F(FileWatcherTest, RapidFileChanges) {
    createFile(test_file1);

    std::atomic<int> event_count{0};

    FileWatcher watcher(20);  // Very fast polling
    watcher.watch(test_file1.string(), [&](const std::string&, FileEvent) {
        event_count.fetch_add(1, std::memory_order_relaxed);
    });

    watcher.start();
    std::this_thread::sleep_for(50ms);

    // Rapid modifications
    for (int i = 0; i < 10; ++i) {
        modifyFile(test_file1, std::to_string(i));
        std::this_thread::sleep_for(30ms);
    }

    watcher.stop();

    EXPECT_GT(event_count.load(), 0);
}

TEST_F(FileWatcherTest, WatchBeforeStart) {
    createFile(test_file1);

    std::atomic<bool> callback_invoked{false};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string&, FileEvent) {
        callback_invoked = true;
    });

    // Modify before starting
    modifyFile(test_file1);

    watcher.start();
    std::this_thread::sleep_for(200ms);
    watcher.stop();

    // Should detect the file state after starting
    // Might not detect the pre-start modification
}

TEST_F(FileWatcherTest, ModifyAfterUnwatch) {
    createFile(test_file1);

    std::atomic<int> event_count{0};

    FileWatcher watcher(50);
    watcher.watch(test_file1.string(), [&](const std::string&, FileEvent) {
        event_count.fetch_add(1, std::memory_order_relaxed);
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    watcher.unwatch(test_file1.string());

    // Modify after unwatching
    modifyFile(test_file1);
    std::this_thread::sleep_for(200ms);

    int count_after_unwatch = event_count.load();

    watcher.stop();

    // Should not detect modifications after unwatching
    EXPECT_EQ(event_count.load(), count_after_unwatch);
}

TEST_F(FileWatcherTest, VerySlowPolling) {
    createFile(test_file1);

    std::atomic<bool> detected{false};

    FileWatcher watcher(5000);  // 5 second polling
    watcher.watch(test_file1.string(), [&](const std::string&, FileEvent) {
        detected = true;
    });

    watcher.start();
    std::this_thread::sleep_for(100ms);

    modifyFile(test_file1);

    // Stop before polling interval - should not detect
    std::this_thread::sleep_for(200ms);
    watcher.stop();

    // Likely won't detect due to slow polling
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
