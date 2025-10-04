#include <gtest/gtest.h>
#include "utils/properties.h"
#include <thread>
#include <vector>

using namespace cdmf;

TEST(PropertiesTest, DefaultConstructor) {
    Properties props;
    EXPECT_TRUE(props.empty());
    EXPECT_EQ(0u, props.size());
}

TEST(PropertiesTest, SetAndGetString) {
    Properties props;
    props.set("key1", std::string("value1"));

    EXPECT_EQ("value1", props.getString("key1"));
    EXPECT_EQ("", props.getString("nonexistent"));
    EXPECT_EQ("default", props.getString("nonexistent", "default"));
}

TEST(PropertiesTest, SetAndGetInt) {
    Properties props;
    props.set("int_key", 42);

    EXPECT_EQ(42, props.getInt("int_key"));
    EXPECT_EQ(0, props.getInt("nonexistent"));
    EXPECT_EQ(99, props.getInt("nonexistent", 99));
}

TEST(PropertiesTest, SetAndGetBool) {
    Properties props;
    props.set("bool_key", true);

    EXPECT_TRUE(props.getBool("bool_key"));
    EXPECT_FALSE(props.getBool("nonexistent"));
    EXPECT_TRUE(props.getBool("nonexistent", true));
}

TEST(PropertiesTest, SetAndGetDouble) {
    Properties props;
    props.set("double_key", 3.14);

    EXPECT_DOUBLE_EQ(3.14, props.getDouble("double_key"));
    EXPECT_DOUBLE_EQ(0.0, props.getDouble("nonexistent"));
    EXPECT_DOUBLE_EQ(2.71, props.getDouble("nonexistent", 2.71));
}

TEST(PropertiesTest, SetAndGetLong) {
    Properties props;
    props.set("long_key", 1234567890L);

    EXPECT_EQ(1234567890L, props.getLong("long_key"));
    EXPECT_EQ(0L, props.getLong("nonexistent"));
    EXPECT_EQ(999L, props.getLong("nonexistent", 999L));
}

TEST(PropertiesTest, GetAsTemplateMethod) {
    Properties props;
    props.set("string_key", std::string("test"));
    props.set("int_key", 100);

    auto stringValue = props.getAs<std::string>("string_key");
    ASSERT_TRUE(stringValue.has_value());
    EXPECT_EQ("test", stringValue.value());

    auto intValue = props.getAs<int>("int_key");
    ASSERT_TRUE(intValue.has_value());
    EXPECT_EQ(100, intValue.value());

    // Wrong type
    auto wrongType = props.getAs<int>("string_key");
    EXPECT_FALSE(wrongType.has_value());

    // Nonexistent key
    auto nonexistent = props.getAs<std::string>("nonexistent");
    EXPECT_FALSE(nonexistent.has_value());
}

TEST(PropertiesTest, Has) {
    Properties props;
    props.set("key1", std::string("value1"));

    EXPECT_TRUE(props.has("key1"));
    EXPECT_FALSE(props.has("nonexistent"));
}

TEST(PropertiesTest, Remove) {
    Properties props;
    props.set("key1", std::string("value1"));
    props.set("key2", std::string("value2"));

    EXPECT_TRUE(props.has("key1"));
    EXPECT_TRUE(props.remove("key1"));
    EXPECT_FALSE(props.has("key1"));
    EXPECT_FALSE(props.remove("key1")); // Already removed

    EXPECT_TRUE(props.has("key2"));
}

TEST(PropertiesTest, Keys) {
    Properties props;
    props.set("key1", std::string("value1"));
    props.set("key2", 42);
    props.set("key3", true);

    auto keys = props.keys();
    EXPECT_EQ(3u, keys.size());

    // Sort for consistent comparison
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ("key1", keys[0]);
    EXPECT_EQ("key2", keys[1]);
    EXPECT_EQ("key3", keys[2]);
}

TEST(PropertiesTest, Size) {
    Properties props;
    EXPECT_EQ(0u, props.size());

    props.set("key1", std::string("value1"));
    EXPECT_EQ(1u, props.size());

    props.set("key2", 42);
    EXPECT_EQ(2u, props.size());

    props.remove("key1");
    EXPECT_EQ(1u, props.size());
}

TEST(PropertiesTest, Empty) {
    Properties props;
    EXPECT_TRUE(props.empty());

    props.set("key1", std::string("value1"));
    EXPECT_FALSE(props.empty());

    props.clear();
    EXPECT_TRUE(props.empty());
}

TEST(PropertiesTest, Clear) {
    Properties props;
    props.set("key1", std::string("value1"));
    props.set("key2", 42);
    props.set("key3", true);

    EXPECT_EQ(3u, props.size());

    props.clear();

    EXPECT_EQ(0u, props.size());
    EXPECT_TRUE(props.empty());
    EXPECT_FALSE(props.has("key1"));
}

TEST(PropertiesTest, Merge) {
    Properties props1;
    props1.set("key1", std::string("value1"));
    props1.set("key2", 42);

    Properties props2;
    props2.set("key2", 99);  // Will overwrite
    props2.set("key3", true);

    props1.merge(props2);

    EXPECT_EQ(3u, props1.size());
    EXPECT_EQ("value1", props1.getString("key1"));
    EXPECT_EQ(99, props1.getInt("key2"));  // Overwritten
    EXPECT_TRUE(props1.getBool("key3"));
}

TEST(PropertiesTest, CopyConstructor) {
    Properties props1;
    props1.set("key1", std::string("value1"));
    props1.set("key2", 42);

    Properties props2(props1);

    EXPECT_EQ(2u, props2.size());
    EXPECT_EQ("value1", props2.getString("key1"));
    EXPECT_EQ(42, props2.getInt("key2"));

    // Modify props2, props1 should be unchanged
    props2.set("key3", true);
    EXPECT_FALSE(props1.has("key3"));
}

TEST(PropertiesTest, MoveConstructor) {
    Properties props1;
    props1.set("key1", std::string("value1"));
    props1.set("key2", 42);

    Properties props2(std::move(props1));

    EXPECT_EQ(2u, props2.size());
    EXPECT_EQ("value1", props2.getString("key1"));
    EXPECT_EQ(42, props2.getInt("key2"));
}

TEST(PropertiesTest, CopyAssignment) {
    Properties props1;
    props1.set("key1", std::string("value1"));

    Properties props2;
    props2.set("key2", 42);

    props2 = props1;

    EXPECT_EQ(1u, props2.size());
    EXPECT_EQ("value1", props2.getString("key1"));
    EXPECT_FALSE(props2.has("key2"));
}

TEST(PropertiesTest, MoveAssignment) {
    Properties props1;
    props1.set("key1", std::string("value1"));

    Properties props2;
    props2.set("key2", 42);

    props2 = std::move(props1);

    EXPECT_EQ(1u, props2.size());
    EXPECT_EQ("value1", props2.getString("key1"));
}

TEST(PropertiesTest, SelfAssignment) {
    Properties props;
    props.set("key1", std::string("value1"));

    props = props;  // Self-assignment

    EXPECT_EQ(1u, props.size());
    EXPECT_EQ("value1", props.getString("key1"));
}

TEST(PropertiesTest, EqualityOperator) {
    Properties props1;
    props1.set("key1", std::string("value1"));
    props1.set("key2", 42);

    Properties props2;
    props2.set("key1", std::string("value1"));
    props2.set("key2", 42);

    Properties props3;
    props3.set("key1", std::string("value1"));

    // Note: Equality only checks keys, not values (due to std::any limitations)
    EXPECT_TRUE(props1 == props2);
    EXPECT_FALSE(props1 == props3);
    EXPECT_TRUE(props1 != props3);
}

TEST(PropertiesTest, ThreadSafety) {
    Properties props;

    // Write threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&props, i]() {
            for (int j = 0; j < 100; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                props.set(key, i * 100 + j);
            }
        });
    }

    // Read threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&props]() {
            for (int j = 0; j < 100; ++j) {
                auto keys = props.keys();
                for (const auto& key : keys) {
                    props.getInt(key);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All writes should have succeeded
    EXPECT_EQ(1000u, props.size());
}

TEST(PropertiesTest, OverwriteValue) {
    Properties props;
    props.set("key", 42);
    EXPECT_EQ(42, props.getInt("key"));

    props.set("key", std::string("overwritten"));
    EXPECT_EQ("overwritten", props.getString("key"));
    EXPECT_EQ(1u, props.size());  // Still only one key
}

TEST(PropertiesTest, ComplexTypes) {
    Properties props;

    // Store a vector
    std::vector<int> vec = {1, 2, 3, 4, 5};
    props.set("vector", vec);

    auto retrieved = props.getAs<std::vector<int>>("vector");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(vec, retrieved.value());

    // Store a custom struct
    struct Point {
        int x;
        int y;
        bool operator==(const Point& other) const {
            return x == other.x && y == other.y;
        }
    };

    Point p{10, 20};
    props.set("point", p);

    auto retrievedPoint = props.getAs<Point>("point");
    ASSERT_TRUE(retrievedPoint.has_value());
    EXPECT_EQ(p, retrievedPoint.value());
}
