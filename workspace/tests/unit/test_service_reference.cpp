#include <gtest/gtest.h>
#include "service/service_reference.h"
#include "service/service_entry.h"
#include "utils/properties.h"

using namespace cdmf;

// ============================================================================
// Service Reference Tests
// ============================================================================

TEST(ServiceReferenceTest, DefaultConstructor) {
    ServiceReference ref;

    EXPECT_FALSE(ref.isValid());
    EXPECT_EQ(0u, ref.getServiceId());
    EXPECT_EQ("", ref.getInterface());
    EXPECT_EQ(nullptr, ref.getModule());
}

TEST(ServiceReferenceTest, ConstructFromEntry) {
    Properties props;
    props.set("key", "value");

    auto entry = std::make_shared<ServiceEntry>(
        123, "com.example.ITest", nullptr, props, nullptr
    );

    ServiceReference ref(entry);

    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ(123u, ref.getServiceId());
    EXPECT_EQ("com.example.ITest", ref.getInterface());
    EXPECT_EQ(nullptr, ref.getModule());
}

TEST(ServiceReferenceTest, GetProperties) {
    Properties props;
    props.set("key1", std::any(std::string("value1")));
    props.set("key2", std::any(42));

    auto entry = std::make_shared<ServiceEntry>(
        1, "com.example.ITest", nullptr, props, nullptr
    );

    ServiceReference ref(entry);

    Properties retrievedProps = ref.getProperties();
    EXPECT_EQ("value1", std::any_cast<std::string>(retrievedProps.get("key1")));
    EXPECT_EQ(42, std::any_cast<int>(retrievedProps.get("key2")));
}

TEST(ServiceReferenceTest, GetRanking) {
    Properties props;
    props.set("service.ranking", std::any(100));

    auto entry = std::make_shared<ServiceEntry>(
        1, "com.example.ITest", nullptr, props, nullptr
    );

    ServiceReference ref(entry);

    EXPECT_EQ(100, ref.getRanking());
}

TEST(ServiceReferenceTest, DefaultRanking) {
    Properties props; // No ranking set

    auto entry = std::make_shared<ServiceEntry>(
        1, "com.example.ITest", nullptr, props, nullptr
    );

    ServiceReference ref(entry);

    EXPECT_EQ(0, ref.getRanking()); // Default ranking
}

TEST(ServiceReferenceTest, Comparison) {
    auto entry1 = std::make_shared<ServiceEntry>(
        1, "com.example.ITest", nullptr, Properties(), nullptr
    );
    auto entry2 = std::make_shared<ServiceEntry>(
        2, "com.example.ITest", nullptr, Properties(), nullptr
    );

    ServiceReference ref1(entry1);
    ServiceReference ref2(entry2);
    ServiceReference ref1_copy(entry1);

    EXPECT_EQ(ref1, ref1_copy);
    EXPECT_NE(ref1, ref2);
}

TEST(ServiceReferenceTest, SortByRanking) {
    Properties props1, props2, props3;
    props1.set("service.ranking", 10);
    props2.set("service.ranking", 100);
    props3.set("service.ranking", 50);

    auto entry1 = std::make_shared<ServiceEntry>(1, "com.example.ITest", nullptr, props1, nullptr);
    auto entry2 = std::make_shared<ServiceEntry>(2, "com.example.ITest", nullptr, props2, nullptr);
    auto entry3 = std::make_shared<ServiceEntry>(3, "com.example.ITest", nullptr, props3, nullptr);

    ServiceReference ref1(entry1);
    ServiceReference ref2(entry2);
    ServiceReference ref3(entry3);

    std::vector<ServiceReference> refs = {ref1, ref2, ref3};
    std::sort(refs.begin(), refs.end());

    // Higher ranking comes first
    EXPECT_EQ(100, refs[0].getRanking());
    EXPECT_EQ(50, refs[1].getRanking());
    EXPECT_EQ(10, refs[2].getRanking());
}

TEST(ServiceReferenceTest, SortByServiceId) {
    // Same ranking, different IDs
    Properties props;
    props.set("service.ranking", std::any(50));

    auto entry1 = std::make_shared<ServiceEntry>(1, "com.example.ITest", nullptr, props, nullptr);
    auto entry2 = std::make_shared<ServiceEntry>(2, "com.example.ITest", nullptr, props, nullptr);
    auto entry3 = std::make_shared<ServiceEntry>(3, "com.example.ITest", nullptr, props, nullptr);

    ServiceReference ref1(entry1);
    ServiceReference ref2(entry2);
    ServiceReference ref3(entry3);

    std::vector<ServiceReference> refs = {ref3, ref1, ref2};
    std::sort(refs.begin(), refs.end());

    // Lower ID comes first (when ranking is same)
    EXPECT_EQ(1u, refs[0].getServiceId());
    EXPECT_EQ(2u, refs[1].getServiceId());
    EXPECT_EQ(3u, refs[2].getServiceId());
}
