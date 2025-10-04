#include <gtest/gtest.h>
#include "module/module_registry.h"
#include "module/module.h"
#include <memory>

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
