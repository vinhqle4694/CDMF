#include "security/permission_manager.h"
#include <gtest/gtest.h>
#include <fstream>
#include <thread>

using namespace cdmf::security;

class PermissionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset the permission manager before each test
        PermissionManager::getInstance().reset();
    }

    void TearDown() override {
        // Clean up test files
        std::remove("test_permissions.conf");
    }

    PermissionManager& manager = PermissionManager::getInstance();
};

// ========== Basic Permission Management Tests ==========

TEST_F(PermissionManagerTest, GrantPermission) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    EXPECT_TRUE(manager.grantPermission("test.module", perm));
}

TEST_F(PermissionManagerTest, GrantPermissionInvalidModule) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    EXPECT_FALSE(manager.grantPermission("", perm));
}

TEST_F(PermissionManagerTest, GrantPermissionNull) {
    EXPECT_FALSE(manager.grantPermission("test.module", nullptr));
}

TEST_F(PermissionManagerTest, RevokePermission) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    manager.grantPermission("test.module", perm);

    EXPECT_TRUE(manager.revokePermission("test.module", perm));
}

TEST_F(PermissionManagerTest, RevokePermissionNonExistent) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    EXPECT_FALSE(manager.revokePermission("nonexistent.module", perm));
}

// ========== Permission Checking Tests ==========

TEST_F(PermissionManagerTest, HasPermissionExact) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.service");
    manager.grantPermission("test.module", perm);

    Permission check(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(manager.hasPermission("test.module", check));
}

TEST_F(PermissionManagerTest, HasPermissionWildcard) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    manager.grantPermission("test.module", perm);

    Permission check(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(manager.hasPermission("test.module", check));
}

TEST_F(PermissionManagerTest, HasPermissionNoMatch) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    manager.grantPermission("test.module", perm);

    Permission check(PermissionType::SERVICE_GET, "org.other.service");
    EXPECT_FALSE(manager.hasPermission("test.module", check));
}

TEST_F(PermissionManagerTest, CheckPermission) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    manager.grantPermission("test.module", perm);

    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::SERVICE_GET, "com.example.service"));
    EXPECT_FALSE(manager.checkPermission("test.module", PermissionType::MODULE_LOAD, "com.example.module"));
}

TEST_F(PermissionManagerTest, CheckPermissionDefaultTarget) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET, "*");
    manager.grantPermission("test.module", perm);

    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::SERVICE_GET));
}

TEST_F(PermissionManagerTest, AdminPermissionImpliesAll) {
    auto perm = std::make_shared<Permission>(PermissionType::ADMIN);
    manager.grantPermission("test.module", perm);

    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::SERVICE_GET, "any.service"));
    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::MODULE_LOAD, "any.module"));
    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::FILE_WRITE, "/any/path"));
}

// ========== Permission Retrieval Tests ==========

TEST_F(PermissionManagerTest, GetPermissions) {
    auto perm1 = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    auto perm2 = std::make_shared<Permission>(PermissionType::MODULE_LOAD);

    manager.grantPermission("test.module", perm1);
    manager.grantPermission("test.module", perm2);

    auto perms = manager.getPermissions("test.module");
    EXPECT_EQ(2, perms.size());
}

TEST_F(PermissionManagerTest, GetPermissionsEmpty) {
    auto perms = manager.getPermissions("nonexistent.module");
    EXPECT_TRUE(perms.empty());
}

TEST_F(PermissionManagerTest, GetPermissionsByType) {
    auto perm1 = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    auto perm2 = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    auto perm3 = std::make_shared<Permission>(PermissionType::MODULE_LOAD);

    manager.grantPermission("test.module", perm1);
    manager.grantPermission("test.module", perm2);
    manager.grantPermission("test.module", perm3);

    auto servicePerms = manager.getPermissionsByType("test.module", PermissionType::SERVICE_GET);
    EXPECT_EQ(2, servicePerms.size());

    auto modulePerms = manager.getPermissionsByType("test.module", PermissionType::MODULE_LOAD);
    EXPECT_EQ(1, modulePerms.size());
}

TEST_F(PermissionManagerTest, ClearPermissions) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    manager.grantPermission("test.module", perm);

    manager.clearPermissions("test.module");

    auto perms = manager.getPermissions("test.module");
    EXPECT_TRUE(perms.empty());
}

// ========== Default Permissions Tests ==========

TEST_F(PermissionManagerTest, SetDefaultPermissions) {
    std::vector<std::shared_ptr<Permission>> defaults = {
        std::make_shared<Permission>(PermissionType::SERVICE_GET),
        std::make_shared<Permission>(PermissionType::EVENT_SUBSCRIBE)
    };

    manager.setDefaultPermissions(defaults);

    auto retrieved = manager.getDefaultPermissions();
    EXPECT_EQ(2, retrieved.size());
}

TEST_F(PermissionManagerTest, GetDefaultPermissions) {
    auto defaults = manager.getDefaultPermissions();
    EXPECT_FALSE(defaults.empty()); // Should have some default permissions
}

TEST_F(PermissionManagerTest, ApplyDefaultPermissions) {
    std::vector<std::shared_ptr<Permission>> defaults = {
        std::make_shared<Permission>(PermissionType::SERVICE_GET),
        std::make_shared<Permission>(PermissionType::EVENT_SUBSCRIBE)
    };

    manager.setDefaultPermissions(defaults);
    manager.applyDefaultPermissions("new.module");

    auto perms = manager.getPermissions("new.module");
    EXPECT_EQ(2, perms.size());
}

// ========== Configuration Persistence Tests ==========

TEST_F(PermissionManagerTest, SavePermissionsToConfig) {
    auto perm1 = std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*");
    auto perm2 = std::make_shared<Permission>(PermissionType::MODULE_LOAD, "*");

    manager.grantPermission("test.module1", perm1);
    manager.grantPermission("test.module2", perm2);

    EXPECT_TRUE(manager.savePermissionsToConfig("test_permissions.conf"));

    // Verify file exists
    std::ifstream file("test_permissions.conf");
    EXPECT_TRUE(file.good());
    file.close();
}

TEST_F(PermissionManagerTest, LoadPermissionsFromConfig) {
    // Create a test config file
    std::ofstream file("test_permissions.conf");
    file << "# Test config\n";
    file << "[test.module]\n";
    file << "SERVICE_GET:com.example.*:GRANT\n";
    file << "MODULE_LOAD:*:GRANT\n";
    file.close();

    EXPECT_TRUE(manager.loadPermissionsFromConfig("test_permissions.conf"));

    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::SERVICE_GET, "com.example.service"));
    EXPECT_TRUE(manager.checkPermission("test.module", PermissionType::MODULE_LOAD, "any.module"));
}

TEST_F(PermissionManagerTest, LoadPermissionsFromConfigNonExistent) {
    EXPECT_FALSE(manager.loadPermissionsFromConfig("nonexistent.conf"));
}

TEST_F(PermissionManagerTest, LoadPermissionsIgnoresComments) {
    std::ofstream file("test_permissions.conf");
    file << "# This is a comment\n";
    file << "[test.module]\n";
    file << "# Another comment\n";
    file << "SERVICE_GET:*:GRANT\n";
    file << "\n"; // Empty line
    file << "MODULE_LOAD:*:GRANT\n";
    file.close();

    EXPECT_TRUE(manager.loadPermissionsFromConfig("test_permissions.conf"));

    auto perms = manager.getPermissions("test.module");
    EXPECT_EQ(2, perms.size());
}

// ========== Module Management Tests ==========

TEST_F(PermissionManagerTest, GetAllModuleIds) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);

    manager.grantPermission("module1", perm);
    manager.grantPermission("module2", perm);
    manager.grantPermission("module3", perm);

    auto ids = manager.getAllModuleIds();
    EXPECT_EQ(3, ids.size());
}

TEST_F(PermissionManagerTest, GetAllModuleIdsEmpty) {
    auto ids = manager.getAllModuleIds();
    EXPECT_TRUE(ids.empty());
}

TEST_F(PermissionManagerTest, HasModule) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    manager.grantPermission("test.module", perm);

    EXPECT_TRUE(manager.hasModule("test.module"));
    EXPECT_FALSE(manager.hasModule("nonexistent.module"));
}

TEST_F(PermissionManagerTest, Reset) {
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    manager.grantPermission("module1", perm);
    manager.grantPermission("module2", perm);

    manager.reset();

    auto ids = manager.getAllModuleIds();
    EXPECT_TRUE(ids.empty());
}

// ========== Thread Safety Tests ==========

TEST_F(PermissionManagerTest, ConcurrentGrant) {
    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i]() {
            auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);
            manager.grantPermission("module" + std::to_string(i), perm);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto ids = manager.getAllModuleIds();
    EXPECT_EQ(numThreads, ids.size());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
