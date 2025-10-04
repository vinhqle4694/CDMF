#include "security/sandbox_manager.h"
#include <gtest/gtest.h>

using namespace cdmf::security;

TEST(SandboxManagerTest, CreateSandbox) {
    auto& manager = SandboxManager::getInstance();
    SandboxConfig config;
    config.type = SandboxType::PROCESS;

    std::string sandboxId = manager.createSandbox("test.module", config);
    EXPECT_FALSE(sandboxId.empty());
}

TEST(SandboxManagerTest, DestroySandbox) {
    auto& manager = SandboxManager::getInstance();
    SandboxConfig config;
    std::string sandboxId = manager.createSandbox("test.module2", config);

    EXPECT_TRUE(manager.destroySandbox(sandboxId));
}

TEST(SandboxManagerTest, GetSandboxInfo) {
    auto& manager = SandboxManager::getInstance();
    SandboxConfig config;
    std::string sandboxId = manager.createSandbox("test.module3", config);

    auto info = manager.getSandboxInfo(sandboxId);
    ASSERT_NE(nullptr, info);
    EXPECT_EQ("test.module3", info->moduleId);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
