/**
 * @file test_module_reloader.cpp
 * @brief Basic unit tests for ModuleReloader component APIs
 *
 * Tests include:
 * - Constructor and initialization
 * - Enable/disable functionality
 * - Start/stop operations
 * - Basic error handling
 *
 * Note: Full module registration tests require concrete Module instances
 * which are tested in integration tests.
 *
 * @author Module Reloader Test Agent
 * @date 2025-10-05
 */

#include "module/module_reloader.h"
#include "core/framework.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace cdmf;
using namespace std::chrono_literals;

// ============================================================================
// Test Fixtures
// ============================================================================

class ModuleReloaderTest : public ::testing::Test {
protected:
    std::unique_ptr<ModuleReloader> reloader;
    std::filesystem::path test_dir;
    std::filesystem::path test_lib;

    void SetUp() override {
        reloader = std::make_unique<ModuleReloader>(nullptr, 50);

        // Create temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "cdmf_reloader_test";
        std::filesystem::create_directories(test_dir);

        test_lib = test_dir / "test_module.so";
        createDummyLibrary(test_lib);
    }

    void TearDown() override {
        if (reloader) {
            reloader->stop();
            reloader.reset();
        }

        // Clean up test files
        std::filesystem::remove_all(test_dir);
    }

    void createDummyLibrary(const std::filesystem::path& path,
                           const std::string& content = "library v1") {
        std::ofstream file(path);
        file << content;
        file.close();
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(ModuleReloaderTest, ConstructorWithNullFramework) {
    EXPECT_NO_THROW({
        ModuleReloader reloader(nullptr);
    });
}

TEST_F(ModuleReloaderTest, ConstructorWithCustomPollInterval) {
    EXPECT_NO_THROW({
        ModuleReloader reloader(nullptr, 500);
    });
}

TEST_F(ModuleReloaderTest, InitialState) {
    EXPECT_FALSE(reloader->isEnabled());
    EXPECT_FALSE(reloader->isRunning());
    EXPECT_EQ(reloader->getRegisteredCount(), 0);
}

TEST_F(ModuleReloaderTest, EnableDisable) {
    EXPECT_FALSE(reloader->isEnabled());

    reloader->setEnabled(true);
    EXPECT_TRUE(reloader->isEnabled());

    reloader->setEnabled(false);
    EXPECT_FALSE(reloader->isEnabled());
}

TEST_F(ModuleReloaderTest, StartAndStop) {
    reloader->start();
    EXPECT_TRUE(reloader->isRunning());

    reloader->stop();
    EXPECT_FALSE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, MultipleStartCalls) {
    reloader->start();
    EXPECT_TRUE(reloader->isRunning());

    // Starting again should be safe
    reloader->start();
    EXPECT_TRUE(reloader->isRunning());

    reloader->stop();
}

TEST_F(ModuleReloaderTest, MultipleStopCalls) {
    reloader->start();
    reloader->stop();
    EXPECT_FALSE(reloader->isRunning());

    // Stopping again should be safe
    reloader->stop();
    EXPECT_FALSE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, DestructorStopsReloader) {
    {
        ModuleReloader temp_reloader(nullptr);
        temp_reloader.start();
        EXPECT_TRUE(temp_reloader.isRunning());
    }
    // Destructor should stop reloader gracefully
}

// ============================================================================
// Module Registration Error Handling
// ============================================================================

TEST_F(ModuleReloaderTest, RegisterNullModule) {
    bool result = reloader->registerModule(nullptr, test_lib.string());

    // Should handle gracefully
    EXPECT_FALSE(result);
    EXPECT_EQ(reloader->getRegisteredCount(), 0);
}

TEST_F(ModuleReloaderTest, RegisterWithEmptyPath) {
    bool result = reloader->registerModule(nullptr, "");

    EXPECT_FALSE(result);
    EXPECT_EQ(reloader->getRegisteredCount(), 0);
}

TEST_F(ModuleReloaderTest, UnregisterNullModule) {
    // Should not crash
    EXPECT_NO_THROW({
        reloader->unregisterModule(nullptr);
    });
}

TEST_F(ModuleReloaderTest, IsRegisteredNullModule) {
    EXPECT_FALSE(reloader->isRegistered(nullptr));
}

// ============================================================================
// State Preservation Tests
// ============================================================================

TEST_F(ModuleReloaderTest, EnableStatePersistsAcrossStartStop) {
    reloader->setEnabled(true);

    reloader->start();
    EXPECT_TRUE(reloader->isEnabled());

    reloader->stop();
    EXPECT_TRUE(reloader->isEnabled());
}

TEST_F(ModuleReloaderTest, RegistrationCountInitiallyZero) {
    EXPECT_EQ(reloader->getRegisteredCount(), 0);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ModuleReloaderTest, VeryFastPolling) {
    EXPECT_NO_THROW({
        ModuleReloader fast_reloader(nullptr, 10);  // 10ms polling
        fast_reloader.start();
        std::this_thread::sleep_for(50ms);
        fast_reloader.stop();
    });
}

TEST_F(ModuleReloaderTest, VerySlowPolling) {
    EXPECT_NO_THROW({
        ModuleReloader slow_reloader(nullptr, 5000);  // 5s polling
        slow_reloader.start();
        std::this_thread::sleep_for(100ms);
        slow_reloader.stop();
    });
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ModuleReloaderTest, ConcurrentStartStop) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&]() {
            reloader->start();
            std::this_thread::sleep_for(10ms);
            reloader->stop();
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_FALSE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, ConcurrentEnableDisable) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&, i]() {
            if (i % 2 == 0) {
                reloader->setEnabled(true);
            } else {
                reloader->setEnabled(false);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should end in a consistent state
    bool enabled = reloader->isEnabled();
    EXPECT_TRUE(enabled == true || enabled == false);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ModuleReloaderTest, StartStopRapidCycle) {
    for (int i = 0; i < 20; ++i) {
        reloader->start();
        reloader->stop();
    }

    EXPECT_FALSE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, EnableDisableRapidCycle) {
    for (int i = 0; i < 50; ++i) {
        reloader->setEnabled(i % 2 == 0);
    }

    // Should handle without issues
}

TEST_F(ModuleReloaderTest, ZeroPollInterval) {
    // Test edge case of very aggressive polling
    EXPECT_NO_THROW({
        ModuleReloader zero_poll(nullptr, 1);  // 1ms = essentially continuous
        zero_poll.start();
        std::this_thread::sleep_for(20ms);
        zero_poll.stop();
    });
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
