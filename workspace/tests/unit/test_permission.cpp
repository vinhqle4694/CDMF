#include "security/permission.h"
#include <gtest/gtest.h>

using namespace cdmf::security;

class PermissionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }

    void TearDown() override {
        // Cleanup
    }
};

// ========== PermissionType Tests ==========

TEST_F(PermissionTest, PermissionTypeToString) {
    EXPECT_EQ("SERVICE_GET", permissionTypeToString(PermissionType::SERVICE_GET));
    EXPECT_EQ("MODULE_LOAD", permissionTypeToString(PermissionType::MODULE_LOAD));
    EXPECT_EQ("ADMIN", permissionTypeToString(PermissionType::ADMIN));
}

TEST_F(PermissionTest, StringToPermissionType) {
    EXPECT_EQ(PermissionType::SERVICE_GET, stringToPermissionType("SERVICE_GET"));
    EXPECT_EQ(PermissionType::MODULE_LOAD, stringToPermissionType("MODULE_LOAD"));
    EXPECT_EQ(PermissionType::ADMIN, stringToPermissionType("ADMIN"));
}

TEST_F(PermissionTest, StringToPermissionTypeInvalid) {
    EXPECT_THROW(stringToPermissionType("INVALID"), std::invalid_argument);
}

// ========== PermissionAction Tests ==========

TEST_F(PermissionTest, PermissionActionToString) {
    EXPECT_EQ("GRANT", permissionActionToString(PermissionAction::GRANT));
    EXPECT_EQ("DENY", permissionActionToString(PermissionAction::DENY));
    EXPECT_EQ("REVOKE", permissionActionToString(PermissionAction::REVOKE));
}

TEST_F(PermissionTest, StringToPermissionAction) {
    EXPECT_EQ(PermissionAction::GRANT, stringToPermissionAction("GRANT"));
    EXPECT_EQ(PermissionAction::DENY, stringToPermissionAction("DENY"));
    EXPECT_EQ(PermissionAction::REVOKE, stringToPermissionAction("REVOKE"));
}

TEST_F(PermissionTest, StringToPermissionActionInvalid) {
    EXPECT_THROW(stringToPermissionAction("INVALID"), std::invalid_argument);
}

// ========== Permission Class Tests ==========

TEST_F(PermissionTest, PermissionConstruction) {
    Permission perm(PermissionType::SERVICE_GET, "com.example.*", PermissionAction::GRANT);
    EXPECT_EQ(PermissionType::SERVICE_GET, perm.getType());
    EXPECT_EQ("com.example.*", perm.getTarget());
    EXPECT_EQ(PermissionAction::GRANT, perm.getAction());
}

TEST_F(PermissionTest, PermissionDefaultTarget) {
    Permission perm(PermissionType::SERVICE_GET);
    EXPECT_EQ("*", perm.getTarget());
    EXPECT_EQ(PermissionAction::GRANT, perm.getAction());
}

TEST_F(PermissionTest, PermissionToString) {
    Permission perm(PermissionType::SERVICE_GET, "com.example.*", PermissionAction::GRANT);
    EXPECT_EQ("SERVICE_GET:com.example.*:GRANT", perm.toString());
}

TEST_F(PermissionTest, PermissionFromString) {
    auto perm = Permission::fromString("SERVICE_GET:com.example.*:GRANT");
    EXPECT_EQ(PermissionType::SERVICE_GET, perm->getType());
    EXPECT_EQ("com.example.*", perm->getTarget());
    EXPECT_EQ(PermissionAction::GRANT, perm->getAction());
}

TEST_F(PermissionTest, PermissionFromStringDefaults) {
    auto perm = Permission::fromString("SERVICE_GET");
    EXPECT_EQ(PermissionType::SERVICE_GET, perm->getType());
    EXPECT_EQ("*", perm->getTarget());
    EXPECT_EQ(PermissionAction::GRANT, perm->getAction());
}

TEST_F(PermissionTest, PermissionEquals) {
    Permission perm1(PermissionType::SERVICE_GET, "com.example.*", PermissionAction::GRANT);
    Permission perm2(PermissionType::SERVICE_GET, "com.example.*", PermissionAction::GRANT);
    Permission perm3(PermissionType::SERVICE_GET, "com.other.*", PermissionAction::GRANT);

    EXPECT_TRUE(perm1.equals(perm2));
    EXPECT_FALSE(perm1.equals(perm3));
}

// ========== Permission Implication Tests ==========

TEST_F(PermissionTest, PermissionImpliesExactMatch) {
    Permission perm1(PermissionType::SERVICE_GET, "com.example.service");
    Permission perm2(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(perm1.implies(perm2));
}

TEST_F(PermissionTest, PermissionImpliesWildcard) {
    Permission perm1(PermissionType::SERVICE_GET, "com.example.*");
    Permission perm2(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(perm1.implies(perm2));
}

TEST_F(PermissionTest, PermissionImpliesAllWildcard) {
    Permission perm1(PermissionType::SERVICE_GET, "*");
    Permission perm2(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(perm1.implies(perm2));
}

TEST_F(PermissionTest, PermissionImpliesAdmin) {
    Permission admin(PermissionType::ADMIN);
    Permission other(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(admin.implies(other));
}

TEST_F(PermissionTest, PermissionImpliesDifferentTypes) {
    Permission perm1(PermissionType::SERVICE_GET, "*");
    Permission perm2(PermissionType::MODULE_LOAD, "com.example.module");
    EXPECT_FALSE(perm1.implies(perm2));
}

TEST_F(PermissionTest, PermissionDenyDoesNotImply) {
    Permission deny(PermissionType::SERVICE_GET, "*", PermissionAction::DENY);
    Permission other(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_FALSE(deny.implies(other));
}

// ========== Wildcard Matching Tests ==========

TEST_F(PermissionTest, WildcardMatchExact) {
    Permission perm(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(perm.matchesTarget("com.example.service"));
    EXPECT_FALSE(perm.matchesTarget("com.example.other"));
}

TEST_F(PermissionTest, WildcardMatchStar) {
    Permission perm(PermissionType::SERVICE_GET, "com.example.*");
    EXPECT_TRUE(perm.matchesTarget("com.example.service"));
    EXPECT_TRUE(perm.matchesTarget("com.example.other"));
    EXPECT_FALSE(perm.matchesTarget("org.example.service"));
}

TEST_F(PermissionTest, WildcardMatchAll) {
    Permission perm(PermissionType::SERVICE_GET, "*");
    EXPECT_TRUE(perm.matchesTarget("anything"));
    EXPECT_TRUE(perm.matchesTarget("com.example.service"));
}

// ========== PermissionCollection Tests ==========

TEST_F(PermissionTest, PermissionCollectionAdd) {
    PermissionCollection collection;
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);

    collection.add(perm);
    EXPECT_EQ(1, collection.size());
    EXPECT_FALSE(collection.empty());
}

TEST_F(PermissionTest, PermissionCollectionRemove) {
    PermissionCollection collection;
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);

    collection.add(perm);
    EXPECT_TRUE(collection.remove(perm));
    EXPECT_EQ(0, collection.size());
    EXPECT_TRUE(collection.empty());
}

TEST_F(PermissionTest, PermissionCollectionRemoveNonExistent) {
    PermissionCollection collection;
    auto perm = std::make_shared<Permission>(PermissionType::SERVICE_GET);

    EXPECT_FALSE(collection.remove(perm));
}

TEST_F(PermissionTest, PermissionCollectionImplies) {
    PermissionCollection collection;
    collection.add(std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*"));

    Permission check(PermissionType::SERVICE_GET, "com.example.service");
    EXPECT_TRUE(collection.implies(check));
}

TEST_F(PermissionTest, PermissionCollectionImpliesDeny) {
    PermissionCollection collection;
    collection.add(std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.*", PermissionAction::GRANT));
    collection.add(std::make_shared<Permission>(PermissionType::SERVICE_GET, "com.example.denied", PermissionAction::DENY));

    Permission allowed(PermissionType::SERVICE_GET, "com.example.allowed");
    Permission denied(PermissionType::SERVICE_GET, "com.example.denied");

    EXPECT_TRUE(collection.implies(allowed));
    EXPECT_FALSE(collection.implies(denied));
}

TEST_F(PermissionTest, PermissionCollectionGetByType) {
    PermissionCollection collection;
    collection.add(std::make_shared<Permission>(PermissionType::SERVICE_GET));
    collection.add(std::make_shared<Permission>(PermissionType::MODULE_LOAD));
    collection.add(std::make_shared<Permission>(PermissionType::SERVICE_GET));

    auto servicePerms = collection.getPermissionsByType(PermissionType::SERVICE_GET);
    EXPECT_EQ(2, servicePerms.size());

    auto modulePerms = collection.getPermissionsByType(PermissionType::MODULE_LOAD);
    EXPECT_EQ(1, modulePerms.size());
}

TEST_F(PermissionTest, PermissionCollectionClear) {
    PermissionCollection collection;
    collection.add(std::make_shared<Permission>(PermissionType::SERVICE_GET));
    collection.add(std::make_shared<Permission>(PermissionType::MODULE_LOAD));

    collection.clear();
    EXPECT_EQ(0, collection.size());
    EXPECT_TRUE(collection.empty());
}

TEST_F(PermissionTest, PermissionCollectionGetAll) {
    PermissionCollection collection;
    auto perm1 = std::make_shared<Permission>(PermissionType::SERVICE_GET);
    auto perm2 = std::make_shared<Permission>(PermissionType::MODULE_LOAD);

    collection.add(perm1);
    collection.add(perm2);

    auto all = collection.getPermissions();
    EXPECT_EQ(2, all.size());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
