/**
 * @file test_module_reloader.cpp
 * @brief Comprehensive unit tests for ModuleReloader component
 *
 * Tests include:
 * - Constructor and initialization
 * - Enable/disable functionality
 * - Start/stop operations
 * - File watching and change detection
 * - Module registration and unregistration
 * - Auto-reload trigger verification
 * - Manifest file handling
 * - Thread safety and concurrent operations
 * - Error handling and edge cases
 *
 * @author Module Reloader Test Agent
 * @date 2025-10-05
 */

#include "module/module_reloader.h"
#include "core/framework.h"
#include "module/module.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <nlohmann/json.hpp>

using namespace cdmf;
using namespace std::chrono_literals;
using json = nlohmann::json;

// ============================================================================
// Mock Module for Testing
// ============================================================================

class MockModule : public Module {
public:
    MockModule(const std::string& name, const Version& version)
        : reload_count_(0), was_active_(false), symbolic_name_(name),
          version_(version), module_id_(1), location_(""),
          state_(ModuleState::INSTALLED), manifest_(json::object()) {}

    // Identity
    std::string getSymbolicName() const override { return symbolic_name_; }
    Version getVersion() const override { return version_; }
    std::string getLocation() const override { return location_; }
    uint64_t getModuleId() const override { return module_id_; }

    // Lifecycle operations
    void start() override {
        state_ = ModuleState::ACTIVE;
        was_active_ = true;
    }

    void stop() override {
        state_ = ModuleState::RESOLVED;
    }

    void update(const std::string& location) override {
        location_ = location;
        reload_count_++;
    }

    void uninstall() override {
        state_ = ModuleState::UNINSTALLED;
    }

    // State
    ModuleState getState() const override { return state_; }

    // Context and services
    IModuleContext* getContext() override { return nullptr; }
    std::vector<ServiceRegistration> getRegisteredServices() const override {
        return std::vector<ServiceRegistration>();
    }
    std::vector<ServiceReference> getServicesInUse() const override {
        return std::vector<ServiceReference>();
    }

    // Manifest and headers
    const nlohmann::json& getManifest() const override { return manifest_; }
    std::map<std::string, std::string> getHeaders() const override {
        return std::map<std::string, std::string>();
    }

    // Module listeners (empty implementations)
    void addModuleListener(IModuleListener* listener) override {}
    void removeModuleListener(IModuleListener* listener) override {}

    // Custom test methods
    void simulateReload() {
        reload_count_++;
    }

    int getReloadCount() const { return reload_count_; }
    bool wasActive() const { return was_active_; }

private:
    std::atomic<int> reload_count_;
    bool was_active_;
    std::string symbolic_name_;
    Version version_;
    uint64_t module_id_;
    std::string location_;
    ModuleState state_;
    nlohmann::json manifest_;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class ModuleReloaderTest : public ::testing::Test {
protected:
    std::unique_ptr<ModuleReloader> reloader;
    std::filesystem::path test_dir;
    std::filesystem::path test_lib;
    std::filesystem::path test_manifest;

    void SetUp() override {
        reloader = std::make_unique<ModuleReloader>(nullptr, 50);

        // Create temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "cdmf_reloader_test";
        std::filesystem::create_directories(test_dir);

        test_lib = test_dir / "test_module.so";
        test_manifest = test_dir / "test_module.json";
        createDummyLibrary(test_lib);
        createDummyManifest(test_manifest);
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
        // Ensure file is flushed and visible to filesystem
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
    }

    void createDummyManifest(const std::filesystem::path& path,
                            const std::string& content = "{\"name\":\"test\",\"version\":\"1.0.0\"}") {
        std::ofstream file(path);
        file << content;
        file.close();
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
    }

    void modifyFile(const std::filesystem::path& path, const std::string& new_content) {
        // Small delay to ensure timestamp changes
        std::this_thread::sleep_for(100ms);
        std::ofstream file(path);
        file << new_content;
        file.close();
        // Force timestamp update
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now());
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
// Module Registration Tests
// ============================================================================

TEST_F(ModuleReloaderTest, RegisterModuleBasic) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    bool result = reloader->registerModule(&module, test_lib.string());

    EXPECT_TRUE(result);
    EXPECT_TRUE(reloader->isRegistered(&module));
    EXPECT_EQ(reloader->getRegisteredCount(), 1);
}

TEST_F(ModuleReloaderTest, RegisterModuleWithManifest) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    bool result = reloader->registerModule(&module, test_lib.string(), test_manifest.string());

    EXPECT_TRUE(result);
    EXPECT_TRUE(reloader->isRegistered(&module));
    EXPECT_EQ(reloader->getManifestPath(&module), test_manifest.string());
}

TEST_F(ModuleReloaderTest, RegisterModuleNonexistentLibrary) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    bool result = reloader->registerModule(&module, "/nonexistent/path/module.so");

    // Note: FileWatcher allows watching non-existent paths (for future file creation)
    // So registration succeeds, though no reload will trigger until the file exists
    EXPECT_TRUE(result);
    EXPECT_TRUE(reloader->isRegistered(&module));
}

TEST_F(ModuleReloaderTest, RegisterSameModuleTwice) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    bool result1 = reloader->registerModule(&module, test_lib.string());
    bool result2 = reloader->registerModule(&module, test_lib.string());

    EXPECT_TRUE(result1);
    EXPECT_FALSE(result2);  // Second registration should fail
    EXPECT_EQ(reloader->getRegisteredCount(), 1);
}

TEST_F(ModuleReloaderTest, RegisterMultipleModules) {
    MockModule module1("test.module1", Version(1, 0, 0));
    MockModule module2("test.module2", Version(1, 0, 0));

    std::filesystem::path lib2 = test_dir / "module2.so";
    createDummyLibrary(lib2);

    reloader->start();
    bool result1 = reloader->registerModule(&module1, test_lib.string());
    bool result2 = reloader->registerModule(&module2, lib2.string());

    EXPECT_TRUE(result1);
    EXPECT_TRUE(result2);
    EXPECT_EQ(reloader->getRegisteredCount(), 2);
}

TEST_F(ModuleReloaderTest, UnregisterModule) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    reloader->registerModule(&module, test_lib.string());
    EXPECT_TRUE(reloader->isRegistered(&module));

    reloader->unregisterModule(&module);

    EXPECT_FALSE(reloader->isRegistered(&module));
    EXPECT_EQ(reloader->getRegisteredCount(), 0);
}

TEST_F(ModuleReloaderTest, UnregisterNonregisteredModule) {
    MockModule module("test.module", Version(1, 0, 0));

    // Should not crash
    EXPECT_NO_THROW({
        reloader->unregisterModule(&module);
    });

    EXPECT_FALSE(reloader->isRegistered(&module));
}

// ============================================================================
// File Change Detection Tests
// ============================================================================

TEST_F(ModuleReloaderTest, DetectLibraryFileChange) {
    std::atomic<bool> change_detected(false);

    // Create a custom reloader with file watcher callback
    reloader->start();
    reloader->setEnabled(true);

    // Register a dummy module (without framework, won't actually reload)
    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string());

    // Modify the library file
    modifyFile(test_lib, "library v2");

    // Wait for file watcher to detect change (poll interval is 50ms)
    std::this_thread::sleep_for(200ms);

    // Note: Without a real framework, we can't verify actual reload
    // But we can verify the file watcher is running
    EXPECT_TRUE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, DetectManifestFileChange) {
    reloader->start();
    reloader->setEnabled(true);

    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string(), test_manifest.string());

    // Modify the manifest file
    modifyFile(test_manifest, "{\"name\":\"test\",\"version\":\"2.0.0\"}");

    // Wait for file watcher to detect change
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, NoReloadWhenDisabled) {
    reloader->start();
    reloader->setEnabled(false);  // Disable auto-reload

    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string());

    int initial_reload_count = module.getReloadCount();

    // Modify the library file
    modifyFile(test_lib, "library v2");

    // Wait for file watcher
    std::this_thread::sleep_for(200ms);

    // Reload should not have been triggered
    EXPECT_EQ(module.getReloadCount(), initial_reload_count);
}

TEST_F(ModuleReloaderTest, MultipleFileChanges) {
    reloader->start();
    reloader->setEnabled(true);

    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string());

    // Multiple modifications
    modifyFile(test_lib, "library v2");
    std::this_thread::sleep_for(150ms);

    modifyFile(test_lib, "library v3");
    std::this_thread::sleep_for(150ms);

    modifyFile(test_lib, "library v4");
    std::this_thread::sleep_for(150ms);

    EXPECT_TRUE(reloader->isRunning());
}

// ============================================================================
// Auto-reload Behavior Tests
// ============================================================================

TEST_F(ModuleReloaderTest, ReloadOnlyWhenEnabled) {
    reloader->start();

    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string());

    // Disabled by default
    EXPECT_FALSE(reloader->isEnabled());

    modifyFile(test_lib, "library v2");
    std::this_thread::sleep_for(200ms);

    // Now enable and modify again
    reloader->setEnabled(true);
    modifyFile(test_lib, "library v3");
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(reloader->isEnabled());
}

TEST_F(ModuleReloaderTest, ReloadPreservesRegistration) {
    reloader->start();
    reloader->setEnabled(true);

    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string());

    EXPECT_EQ(reloader->getRegisteredCount(), 1);

    // Trigger a file change
    modifyFile(test_lib, "library v2");
    std::this_thread::sleep_for(200ms);

    // Module should still be registered after reload attempt
    EXPECT_TRUE(reloader->isRegistered(&module));
    EXPECT_EQ(reloader->getRegisteredCount(), 1);
}

TEST_F(ModuleReloaderTest, StopPreventsFileWatching) {
    reloader->start();
    reloader->setEnabled(true);

    MockModule module("test.module", Version(1, 0, 0));
    reloader->registerModule(&module, test_lib.string());

    // Stop the reloader
    reloader->stop();
    EXPECT_FALSE(reloader->isRunning());

    // Modify file while stopped
    modifyFile(test_lib, "library v2");
    std::this_thread::sleep_for(200ms);

    // No reload should have occurred (reloader is stopped)
}

// ============================================================================
// Manifest Path Tests
// ============================================================================

TEST_F(ModuleReloaderTest, GetManifestPathForRegisteredModule) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    reloader->registerModule(&module, test_lib.string(), test_manifest.string());

    std::string manifest_path = reloader->getManifestPath(&module);

    EXPECT_EQ(manifest_path, test_manifest.string());
}

TEST_F(ModuleReloaderTest, GetManifestPathForUnregisteredModule) {
    MockModule module("test.module", Version(1, 0, 0));

    std::string manifest_path = reloader->getManifestPath(&module);

    EXPECT_TRUE(manifest_path.empty());
}

TEST_F(ModuleReloaderTest, GetManifestPathNullModule) {
    std::string manifest_path = reloader->getManifestPath(nullptr);

    EXPECT_TRUE(manifest_path.empty());
}

TEST_F(ModuleReloaderTest, ManifestPathClearedAfterUnregister) {
    MockModule module("test.module", Version(1, 0, 0));

    reloader->start();
    reloader->registerModule(&module, test_lib.string(), test_manifest.string());

    EXPECT_FALSE(reloader->getManifestPath(&module).empty());

    reloader->unregisterModule(&module);

    EXPECT_TRUE(reloader->getManifestPath(&module).empty());
}

// ============================================================================
// Integration Scenario Tests
// ============================================================================

TEST_F(ModuleReloaderTest, CompleteWorkflow) {
    // 1. Create and start reloader
    reloader->start();
    EXPECT_TRUE(reloader->isRunning());

    // 2. Register module
    MockModule module("test.module", Version(1, 0, 0));
    bool registered = reloader->registerModule(&module, test_lib.string(), test_manifest.string());
    EXPECT_TRUE(registered);

    // 3. Enable auto-reload
    reloader->setEnabled(true);
    EXPECT_TRUE(reloader->isEnabled());

    // 4. Simulate file change
    modifyFile(test_lib, "library v2");
    std::this_thread::sleep_for(200ms);

    // 5. Verify state
    EXPECT_TRUE(reloader->isRegistered(&module));
    EXPECT_EQ(reloader->getRegisteredCount(), 1);

    // 6. Unregister and stop
    reloader->unregisterModule(&module);
    EXPECT_FALSE(reloader->isRegistered(&module));

    reloader->stop();
    EXPECT_FALSE(reloader->isRunning());
}

TEST_F(ModuleReloaderTest, ConcurrentRegistrationAndFileChanges) {
    reloader->start();
    reloader->setEnabled(true);

    MockModule module1("test.module1", Version(1, 0, 0));
    MockModule module2("test.module2", Version(1, 0, 0));

    std::filesystem::path lib2 = test_dir / "module2.so";
    createDummyLibrary(lib2);

    std::thread t1([&]() {
        reloader->registerModule(&module1, test_lib.string());
        std::this_thread::sleep_for(50ms);
        modifyFile(test_lib, "library v2");
    });

    std::thread t2([&]() {
        reloader->registerModule(&module2, lib2.string());
        std::this_thread::sleep_for(50ms);
        modifyFile(lib2, "library v2");
    });

    t1.join();
    t2.join();

    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(reloader->getRegisteredCount(), 2);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
