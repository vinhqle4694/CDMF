#include <gtest/gtest.h>
#include "module/module_registry.h"
#include "module/module.h"
#include <memory>
#include <thread>
#include <vector>

using namespace cdmf;

// Mock module for testing
class MockModule : public Module {
public:
    MockModule(uint64_t id, const std::string& name, const Version& version)
        : id_(id), name_(name), version_(version), state_(ModuleState::INSTALLED) {}

    std::string getSymbolicName() const override { return name_; }
    Version getVersion() const override { return version_; }
    std::string getLocation() const override { return ""; }
    uint64_t getModuleId() const override { return id_; }

    void start() override { state_ = ModuleState::ACTIVE; }
    void stop() override { state_ = ModuleState::RESOLVED; }
    void update(const std::string&) override {}
    void uninstall() override { state_ = ModuleState::UNINSTALLED; }

    ModuleState getState() const override { return state_; }
    IModuleContext* getContext() override { return nullptr; }

    std::vector<ServiceRegistration> getRegisteredServices() const override { return {}; }
    std::vector<ServiceReference> getServicesInUse() const override { return {}; }

    const nlohmann::json& getManifest() const override {
        static nlohmann::json empty;
        return empty;
    }

    std::map<std::string, std::string> getHeaders() const override { return {}; }

    void addModuleListener(IModuleListener*) override {}
    void removeModuleListener(IModuleListener*) override {}

private:
    uint64_t id_;
    std::string name_;
    Version version_;
    ModuleState state_;
};

// ============================================================================
// Module Registry Tests
// ============================================================================

TEST(ModuleRegistryTest, Construction) {
    ModuleRegistry registry;
    EXPECT_EQ(0u, registry.getModuleCount());
}

TEST(ModuleRegistryTest, RegisterModule) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    registry.registerModule(&module);

    EXPECT_EQ(1u, registry.getModuleCount());
    EXPECT_TRUE(registry.contains(1));
}

TEST(ModuleRegistryTest, RegisterNullModule) {
    ModuleRegistry registry;

    EXPECT_THROW(registry.registerModule(nullptr), std::invalid_argument);
}

TEST(ModuleRegistryTest, RegisterDuplicateId) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test1", Version(1, 0, 0));
    MockModule module2(1, "com.example.test2", Version(1, 0, 0));

    registry.registerModule(&module1);

    EXPECT_THROW(registry.registerModule(&module2), std::runtime_error);
}

TEST(ModuleRegistryTest, UnregisterModule) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    registry.registerModule(&module);
    EXPECT_EQ(1u, registry.getModuleCount());

    bool result = registry.unregisterModule(1);
    EXPECT_TRUE(result);
    EXPECT_EQ(0u, registry.getModuleCount());
    EXPECT_FALSE(registry.contains(1));
}

TEST(ModuleRegistryTest, UnregisterNonexistent) {
    ModuleRegistry registry;

    bool result = registry.unregisterModule(999);
    EXPECT_FALSE(result);
}

TEST(ModuleRegistryTest, GetModuleById) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    registry.registerModule(&module);

    Module* found = registry.getModule(1);
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(1u, found->getModuleId());
    EXPECT_EQ("com.example.test", found->getSymbolicName());
}

TEST(ModuleRegistryTest, GetModuleByName) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    registry.registerModule(&module);

    Module* found = registry.getModule("com.example.test");
    ASSERT_NE(nullptr, found);
    EXPECT_EQ("com.example.test", found->getSymbolicName());
}

TEST(ModuleRegistryTest, GetModuleByNameMultipleVersions) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(2, 0, 0));
    MockModule module3(3, "com.example.test", Version(1, 5, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);
    registry.registerModule(&module3);

    // Should return highest version
    Module* found = registry.getModule("com.example.test");
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(Version(2, 0, 0), found->getVersion());
}

TEST(ModuleRegistryTest, GetModuleByNameAndVersion) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(2, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);

    Module* found = registry.getModule("com.example.test", Version(1, 0, 0));
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(Version(1, 0, 0), found->getVersion());
}

TEST(ModuleRegistryTest, GetModules) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(2, 0, 0));
    MockModule module3(3, "com.example.other", Version(1, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);
    registry.registerModule(&module3);

    std::vector<Module*> modules = registry.getModules("com.example.test");
    EXPECT_EQ(2u, modules.size());

    // Should be sorted by version (highest first)
    EXPECT_EQ(Version(2, 0, 0), modules[0]->getVersion());
    EXPECT_EQ(Version(1, 0, 0), modules[1]->getVersion());
}

TEST(ModuleRegistryTest, GetAllModules) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test1", Version(1, 0, 0));
    MockModule module2(2, "com.example.test2", Version(1, 0, 0));
    MockModule module3(3, "com.example.test3", Version(1, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);
    registry.registerModule(&module3);

    std::vector<Module*> modules = registry.getAllModules();
    EXPECT_EQ(3u, modules.size());
}

TEST(ModuleRegistryTest, FindCompatibleModule) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(1, 5, 0));
    MockModule module3(3, "com.example.test", Version(2, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);
    registry.registerModule(&module3);

    VersionRange range = VersionRange::parse("[1.0.0,2.0.0)");

    Module* found = registry.findCompatibleModule("com.example.test", range);
    ASSERT_NE(nullptr, found);

    // Should return highest matching version
    EXPECT_EQ(Version(1, 5, 0), found->getVersion());
}

TEST(ModuleRegistryTest, FindCompatibleModules) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(1, 5, 0));
    MockModule module3(3, "com.example.test", Version(2, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);
    registry.registerModule(&module3);

    VersionRange range = VersionRange::parse("[1.0.0,2.0.0)");

    std::vector<Module*> modules = registry.findCompatibleModules("com.example.test", range);
    EXPECT_EQ(2u, modules.size());

    // Should be sorted by version (highest first)
    EXPECT_EQ(Version(1, 5, 0), modules[0]->getVersion());
    EXPECT_EQ(Version(1, 0, 0), modules[1]->getVersion());
}

TEST(ModuleRegistryTest, GetModulesByState) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test1", Version(1, 0, 0));
    MockModule module2(2, "com.example.test2", Version(1, 0, 0));
    MockModule module3(3, "com.example.test3", Version(1, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);
    registry.registerModule(&module3);

    module1.start(); // ACTIVE
    module2.start(); // ACTIVE
    // module3 stays INSTALLED

    std::vector<Module*> activeModules = registry.getModulesByState(ModuleState::ACTIVE);
    EXPECT_EQ(2u, activeModules.size());

    std::vector<Module*> installedModules = registry.getModulesByState(ModuleState::INSTALLED);
    EXPECT_EQ(1u, installedModules.size());
}

TEST(ModuleRegistryTest, GenerateModuleId) {
    ModuleRegistry registry;

    uint64_t id1 = registry.generateModuleId();
    uint64_t id2 = registry.generateModuleId();
    uint64_t id3 = registry.generateModuleId();

    EXPECT_EQ(1u, id1);
    EXPECT_EQ(2u, id2);
    EXPECT_EQ(3u, id3);
}

// ============================================================================
// Module Registry Boundary and Edge Case Tests
// ============================================================================

TEST(ModuleRegistryTest, RegisterManyModules) {
    ModuleRegistry registry;
    std::vector<std::unique_ptr<MockModule>> modules;

    const int MODULE_COUNT = 1000;

    for (int i = 0; i < MODULE_COUNT; ++i) {
        auto module = std::make_unique<MockModule>(
            i + 1,
            "com.example.module" + std::to_string(i),
            Version(1, 0, 0)
        );
        registry.registerModule(module.get());
        modules.push_back(std::move(module));
    }

    EXPECT_EQ(static_cast<size_t>(MODULE_COUNT), registry.getModuleCount());
}

TEST(ModuleRegistryTest, UnregisterAndReregister) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    registry.registerModule(&module);
    EXPECT_EQ(1u, registry.getModuleCount());

    registry.unregisterModule(1);
    EXPECT_EQ(0u, registry.getModuleCount());

    // Re-register same module
    registry.registerModule(&module);
    EXPECT_EQ(1u, registry.getModuleCount());
}

TEST(ModuleRegistryTest, ManyVersionsSameModule) {
    ModuleRegistry registry;

    const int VERSION_COUNT = 100;
    std::vector<std::unique_ptr<MockModule>> modules;

    for (int i = 0; i < VERSION_COUNT; ++i) {
        auto module = std::make_unique<MockModule>(
            i + 1,
            "com.example.test",
            Version(i, 0, 0)
        );
        registry.registerModule(module.get());
        modules.push_back(std::move(module));
    }

    // Should return highest version
    Module* highest = registry.getModule("com.example.test");
    ASSERT_NE(nullptr, highest);
    EXPECT_EQ(Version(VERSION_COUNT - 1, 0, 0), highest->getVersion());
}

TEST(ModuleRegistryTest, VeryLongModuleName) {
    ModuleRegistry registry;
    std::string longName(10000, 'a');
    MockModule module(1, longName, Version(1, 0, 0));

    registry.registerModule(&module);

    Module* found = registry.getModule(longName);
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(longName, found->getSymbolicName());
}

TEST(ModuleRegistryTest, SpecialCharactersInModuleName) {
    ModuleRegistry registry;
    std::string specialName = "com.example@#$%^&*().test";
    MockModule module(1, specialName, Version(1, 0, 0));

    registry.registerModule(&module);

    Module* found = registry.getModule(specialName);
    ASSERT_NE(nullptr, found);
    EXPECT_EQ(specialName, found->getSymbolicName());
}

TEST(ModuleRegistryTest, GetNonexistentModule) {
    ModuleRegistry registry;

    Module* found = registry.getModule(999);
    EXPECT_EQ(nullptr, found);

    found = registry.getModule("com.example.nonexistent");
    EXPECT_EQ(nullptr, found);

    found = registry.getModule("com.example.nonexistent", Version(1, 0, 0));
    EXPECT_EQ(nullptr, found);
}

TEST(ModuleRegistryTest, EmptyRegistry) {
    ModuleRegistry registry;

    EXPECT_EQ(0u, registry.getModuleCount());

    std::vector<Module*> modules = registry.getAllModules();
    EXPECT_TRUE(modules.empty());
}

TEST(ModuleRegistryTest, ContainsCheck) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    EXPECT_FALSE(registry.contains(1));

    registry.registerModule(&module);
    EXPECT_TRUE(registry.contains(1));

    registry.unregisterModule(1);
    EXPECT_FALSE(registry.contains(1));
}

TEST(ModuleRegistryTest, ConcurrentModuleRegistration) {
    ModuleRegistry registry;

    const int THREADS = 10;
    const int MODULES_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<std::unique_ptr<MockModule>>> allModules(THREADS);

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < MODULES_PER_THREAD; ++i) {
                int moduleId = t * MODULES_PER_THREAD + i + 1;
                auto module = std::make_unique<MockModule>(
                    moduleId,
                    "com.example.module" + std::to_string(moduleId),
                    Version(1, 0, 0)
                );
                registry.registerModule(module.get());
                allModules[t].push_back(std::move(module));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(static_cast<size_t>(THREADS * MODULES_PER_THREAD), registry.getModuleCount());
}

TEST(ModuleRegistryTest, ConcurrentModuleUnregistration) {
    ModuleRegistry registry;

    const int MODULES = 1000;
    std::vector<std::unique_ptr<MockModule>> modules;

    // Register modules
    for (int i = 0; i < MODULES; ++i) {
        auto module = std::make_unique<MockModule>(
            i + 1,
            "com.example.module" + std::to_string(i),
            Version(1, 0, 0)
        );
        registry.registerModule(module.get());
        modules.push_back(std::move(module));
    }

    EXPECT_EQ(static_cast<size_t>(MODULES), registry.getModuleCount());

    // Unregister concurrently
    const int THREADS = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = t; i < MODULES; i += THREADS) {
                registry.unregisterModule(i + 1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0u, registry.getModuleCount());
}

TEST(ModuleRegistryTest, FindCompatibleModuleWithNoMatches) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(2, 0, 0));

    registry.registerModule(&module1);
    registry.registerModule(&module2);

    // Range that doesn't match any version
    VersionRange range = VersionRange::parse("[3.0.0,4.0.0)");

    Module* found = registry.findCompatibleModule("com.example.test", range);
    EXPECT_EQ(nullptr, found);
}

TEST(ModuleRegistryTest, GetModulesByStateEmpty) {
    ModuleRegistry registry;

    std::vector<Module*> modules = registry.getModulesByState(ModuleState::ACTIVE);
    EXPECT_TRUE(modules.empty());
}

TEST(ModuleRegistryTest, GetModulesByMultipleStates) {
    ModuleRegistry registry;

    const int MODULE_COUNT = 30;
    std::vector<std::unique_ptr<MockModule>> modules;

    for (int i = 0; i < MODULE_COUNT; ++i) {
        auto module = std::make_unique<MockModule>(
            i + 1,
            "com.example.module" + std::to_string(i),
            Version(1, 0, 0)
        );
        registry.registerModule(module.get());

        // Set different states
        if (i % 3 == 0) {
            module->start();  // ACTIVE
        } else if (i % 3 == 1) {
            module->start();
            module->stop();   // RESOLVED
        }
        // else stay INSTALLED

        modules.push_back(std::move(module));
    }

    std::vector<Module*> activeModules = registry.getModulesByState(ModuleState::ACTIVE);
    std::vector<Module*> resolvedModules = registry.getModulesByState(ModuleState::RESOLVED);
    std::vector<Module*> installedModules = registry.getModulesByState(ModuleState::INSTALLED);

    EXPECT_EQ(10u, activeModules.size());
    EXPECT_EQ(10u, resolvedModules.size());
    EXPECT_EQ(10u, installedModules.size());
}

TEST(ModuleRegistryTest, VersionSortingComplex) {
    ModuleRegistry registry;
    MockModule module1(1, "com.example.test", Version(1, 0, 0));
    MockModule module2(2, "com.example.test", Version(1, 0, 1));
    MockModule module3(3, "com.example.test", Version(1, 1, 0));
    MockModule module4(4, "com.example.test", Version(2, 0, 0));
    MockModule module5(5, "com.example.test", Version(1, 5, 3));

    // Register in random order
    registry.registerModule(&module3);
    registry.registerModule(&module1);
    registry.registerModule(&module5);
    registry.registerModule(&module4);
    registry.registerModule(&module2);

    std::vector<Module*> modules = registry.getModules("com.example.test");
    ASSERT_EQ(5u, modules.size());

    // Should be sorted by version (highest first)
    EXPECT_EQ(Version(2, 0, 0), modules[0]->getVersion());
    EXPECT_EQ(Version(1, 5, 3), modules[1]->getVersion());
    EXPECT_EQ(Version(1, 1, 0), modules[2]->getVersion());
    EXPECT_EQ(Version(1, 0, 1), modules[3]->getVersion());
    EXPECT_EQ(Version(1, 0, 0), modules[4]->getVersion());
}

TEST(ModuleRegistryTest, GetModulesEmptyResult) {
    ModuleRegistry registry;

    std::vector<Module*> modules = registry.getModules("com.example.nonexistent");
    EXPECT_TRUE(modules.empty());
}

TEST(ModuleRegistryTest, FindCompatibleModulesEmptyResult) {
    ModuleRegistry registry;
    MockModule module(1, "com.example.test", Version(1, 0, 0));

    registry.registerModule(&module);

    VersionRange range = VersionRange::parse("[2.0.0,3.0.0)");

    std::vector<Module*> modules = registry.findCompatibleModules("com.example.test", range);
    EXPECT_TRUE(modules.empty());
}
