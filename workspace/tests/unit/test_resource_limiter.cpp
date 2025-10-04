#include "security/resource_limiter.h"
#include <gtest/gtest.h>

using namespace cdmf::security;

TEST(ResourceLimiterTest, SetResourceLimit) {
    auto& limiter = ResourceLimiter::getInstance();
    ResourceLimit limit{ResourceType::MEMORY, 1024, 2048, true};

    EXPECT_TRUE(limiter.setResourceLimit("test.module", limit));
}

TEST(ResourceLimiterTest, GetResourceLimit) {
    auto& limiter = ResourceLimiter::getInstance();
    ResourceLimit limit{ResourceType::CPU_TIME, 1000, 2000, true};
    limiter.setResourceLimit("test.module2", limit);

    auto retrieved = limiter.getResourceLimit("test.module2", ResourceType::CPU_TIME);
    ASSERT_NE(nullptr, retrieved);
    EXPECT_EQ(1000, retrieved->softLimit);
}

TEST(ResourceLimiterTest, RecordUsage) {
    auto& limiter = ResourceLimiter::getInstance();
    ResourceLimit limit{ResourceType::MEMORY, 1024, 2048, true};
    limiter.setResourceLimit("test.module3", limit);

    EXPECT_TRUE(limiter.recordUsage("test.module3", ResourceType::MEMORY, 512));
}

TEST(ResourceLimiterTest, CanAllocate) {
    auto& limiter = ResourceLimiter::getInstance();
    ResourceLimit limit{ResourceType::MEMORY, 1024, 2048, true};
    limiter.setResourceLimit("test.module4", limit);
    limiter.recordUsage("test.module4", ResourceType::MEMORY, 512);

    EXPECT_TRUE(limiter.canAllocate("test.module4", ResourceType::MEMORY, 500));
    EXPECT_FALSE(limiter.canAllocate("test.module4", ResourceType::MEMORY, 2000));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
