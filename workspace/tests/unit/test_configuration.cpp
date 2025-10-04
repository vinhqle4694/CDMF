#include <gtest/gtest.h>
#include "configuration.h"
#include "configuration_admin.h"
#include "configuration_event.h"
#include "configuration_listener.h"
#include "persistence_manager.h"
#include <filesystem>
#include <thread>
#include <chrono>

using namespace cdmf::services;
using namespace cdmf;

// Test fixture for Configuration tests
class ConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up test directory before each test
        if (std::filesystem::exists(testDir_)) {
            std::filesystem::remove_all(testDir_);
        }
    }

    void TearDown() override {
        // Clean up test directory after each test
        if (std::filesystem::exists(testDir_)) {
            std::filesystem::remove_all(testDir_);
        }
    }

    const std::string testDir_ = "./test_config";
};

// Test Configuration basic operations
TEST_F(ConfigurationTest, CreateConfiguration) {
    Configuration config("test.pid");
    EXPECT_EQ("test.pid", config.getPid());
    EXPECT_FALSE(config.isRemoved());
}

TEST_F(ConfigurationTest, ConfigurationWithEmptyPidThrows) {
    EXPECT_THROW(Configuration(""), std::invalid_argument);
}

TEST_F(ConfigurationTest, UpdateConfiguration) {
    Configuration config("test.pid");

    Properties props;
    props.set("key1", "value1");
    props.set("key2", "value2");

    config.update(props);

    const Properties& retrieved = config.getProperties();
    EXPECT_EQ("value1", retrieved.getString("key1"));
    EXPECT_EQ("value2", retrieved.getString("key2"));
}

TEST_F(ConfigurationTest, RemoveConfiguration) {
    Configuration config("test.pid");

    Properties props;
    props.set("key1", "value1");
    config.update(props);

    config.remove();

    EXPECT_TRUE(config.isRemoved());
    EXPECT_THROW(config.update(props), std::runtime_error);
}

// Test PersistenceManager
TEST_F(ConfigurationTest, PersistenceManagerSaveAndLoad) {
    PersistenceManager pm(testDir_);

    Properties props;
    props.set("host", "localhost");
    props.set("port", "8080");
    props.set("timeout", "30");

    pm.save("server.config", props);

    Properties loaded = pm.load("server.config");
    EXPECT_EQ("localhost", loaded.getString("host"));
    EXPECT_EQ("8080", loaded.getString("port"));
    EXPECT_EQ("30", loaded.getString("timeout"));
}

TEST_F(ConfigurationTest, PersistenceManagerLoadNonExistent) {
    PersistenceManager pm(testDir_);

    Properties loaded = pm.load("nonexistent.pid");
    EXPECT_EQ(0, loaded.keys().size());
}

TEST_F(ConfigurationTest, PersistenceManagerRemove) {
    PersistenceManager pm(testDir_);

    Properties props;
    props.set("key", "value");
    pm.save("test.pid", props);

    pm.remove("test.pid");

    Properties loaded = pm.load("test.pid");
    EXPECT_EQ(0, loaded.keys().size());
}

TEST_F(ConfigurationTest, PersistenceManagerListAll) {
    PersistenceManager pm(testDir_);

    Properties props;
    props.set("key", "value");

    pm.save("config1", props);
    pm.save("config2", props);
    pm.save("config3", props);

    auto pids = pm.listAll();
    EXPECT_EQ(3, pids.size());

    EXPECT_TRUE(std::find(pids.begin(), pids.end(), "config1") != pids.end());
    EXPECT_TRUE(std::find(pids.begin(), pids.end(), "config2") != pids.end());
    EXPECT_TRUE(std::find(pids.begin(), pids.end(), "config3") != pids.end());
}

// Test ConfigurationEvent
TEST_F(ConfigurationTest, ConfigurationEventCreation) {
    ConfigurationEvent event1(ConfigurationEventType::CREATED, "test.pid");
    EXPECT_EQ(ConfigurationEventType::CREATED, event1.getType());
    EXPECT_EQ("test.pid", event1.getPid());
    EXPECT_EQ("", event1.getFactoryPid());
    EXPECT_EQ(nullptr, event1.getReference());

    ConfigurationEvent event2(ConfigurationEventType::UPDATED, "test.pid", "factory.pid");
    EXPECT_EQ(ConfigurationEventType::UPDATED, event2.getType());
    EXPECT_EQ("test.pid", event2.getPid());
    EXPECT_EQ("factory.pid", event2.getFactoryPid());

    Configuration config("test.pid");
    ConfigurationEvent event3(ConfigurationEventType::DELETED, "test.pid", &config);
    EXPECT_EQ(ConfigurationEventType::DELETED, event3.getType());
    EXPECT_EQ(&config, event3.getReference());
}

// Mock ConfigurationListener for testing
class MockConfigurationListener : public ConfigurationListener {
public:
    void configurationEvent(const ConfigurationEvent& event) override {
        events_.push_back(event);
    }

    std::vector<ConfigurationEvent> events_;
};

// Test ConfigurationAdmin
TEST_F(ConfigurationTest, ConfigurationAdminCreateConfiguration) {
    ConfigurationAdmin admin(testDir_);

    Configuration* config = admin.createConfiguration("test.pid");
    ASSERT_NE(nullptr, config);
    EXPECT_EQ("test.pid", config->getPid());

    EXPECT_THROW(admin.createConfiguration("test.pid"), std::runtime_error);
}

TEST_F(ConfigurationTest, ConfigurationAdminGetConfiguration) {
    ConfigurationAdmin admin(testDir_);

    Configuration* config1 = admin.getConfiguration("test.pid");
    ASSERT_NE(nullptr, config1);
    EXPECT_EQ("test.pid", config1->getPid());

    // Getting the same PID should return the same configuration
    Configuration* config2 = admin.getConfiguration("test.pid");
    EXPECT_EQ(config1, config2);
}

TEST_F(ConfigurationTest, ConfigurationAdminDeleteConfiguration) {
    ConfigurationAdmin admin(testDir_);

    Configuration* config = admin.createConfiguration("test.pid");
    ASSERT_NE(nullptr, config);

    admin.deleteConfiguration("test.pid");

    EXPECT_TRUE(config->isRemoved());
}

TEST_F(ConfigurationTest, ConfigurationAdminListConfigurations) {
    ConfigurationAdmin admin(testDir_);

    admin.createConfiguration("com.example.service1");
    admin.createConfiguration("com.example.service2");
    admin.createConfiguration("org.test.service");

    auto allConfigs = admin.listConfigurations();
    EXPECT_EQ(3, allConfigs.size());

    auto filtered = admin.listConfigurations("com.example");
    EXPECT_EQ(2, filtered.size());
}

TEST_F(ConfigurationTest, ConfigurationAdminListenerNotifications) {
    ConfigurationAdmin admin(testDir_);
    MockConfigurationListener listener;

    admin.addConfigurationListener(&listener);

    // Create configuration - should fire CREATED event
    Configuration* config = admin.createConfiguration("test.pid");
    EXPECT_EQ(1, listener.events_.size());
    EXPECT_EQ(ConfigurationEventType::CREATED, listener.events_[0].getType());

    // Delete configuration - should fire DELETED event
    admin.deleteConfiguration("test.pid");
    EXPECT_EQ(2, listener.events_.size());
    EXPECT_EQ(ConfigurationEventType::DELETED, listener.events_[1].getType());

    admin.removeConfigurationListener(&listener);
}

TEST_F(ConfigurationTest, ConfigurationAdminMultipleListeners) {
    ConfigurationAdmin admin(testDir_);
    MockConfigurationListener listener1;
    MockConfigurationListener listener2;

    admin.addConfigurationListener(&listener1);
    admin.addConfigurationListener(&listener2);

    admin.createConfiguration("test.pid");

    EXPECT_EQ(1, listener1.events_.size());
    EXPECT_EQ(1, listener2.events_.size());

    admin.removeConfigurationListener(&listener1);

    admin.createConfiguration("test2.pid");

    EXPECT_EQ(1, listener1.events_.size());  // Should not receive new event
    EXPECT_EQ(2, listener2.events_.size());  // Should receive new event
}

TEST_F(ConfigurationTest, ConfigurationAdminPersistence) {
    {
        ConfigurationAdmin admin(testDir_);

        Configuration* config = admin.getConfiguration("persistent.pid");
        Properties props;
        props.set("setting1", "value1");
        props.set("setting2", "value2");
        config->update(props);

        // Destructor should save configurations
    }

    // Create new admin instance - should load persisted configurations
    {
        ConfigurationAdmin admin(testDir_);
        auto configs = admin.listConfigurations();
        EXPECT_EQ(1, configs.size());

        Configuration* config = admin.getConfiguration("persistent.pid");
        const Properties& props = config->getProperties();
        EXPECT_EQ("value1", props.getString("setting1"));
        EXPECT_EQ("value2", props.getString("setting2"));
    }
}

TEST_F(ConfigurationTest, ConfigurationAdminConcurrentAccess) {
    ConfigurationAdmin admin(testDir_);

    const int numThreads = 10;
    const int operationsPerThread = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&admin, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                std::string pid = "config." + std::to_string(i) + "." + std::to_string(j);
                Configuration* config = admin.getConfiguration(pid);

                Properties props;
                props.set("thread", std::to_string(i));
                props.set("iteration", std::to_string(j));
                config->update(props);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto allConfigs = admin.listConfigurations();
    EXPECT_EQ(numThreads * operationsPerThread, allConfigs.size());
}

TEST_F(ConfigurationTest, ConfigurationAdminRemoveNonExistent) {
    ConfigurationAdmin admin(testDir_);

    // Should not throw when deleting non-existent configuration
    EXPECT_NO_THROW(admin.deleteConfiguration("nonexistent.pid"));
}

TEST_F(ConfigurationTest, ConfigurationPropertiesIntegration) {
    ConfigurationAdmin admin(testDir_);

    Configuration* config = admin.getConfiguration("app.config");

    Properties props;
    props.set("string_value", "hello");
    props.set("int_value", "42");
    props.set("bool_value", "true");
    props.set("double_value", "3.14");

    config->update(props);

    const Properties& retrieved = config->getProperties();
    EXPECT_EQ("hello", retrieved.getString("string_value"));
    EXPECT_EQ(42, retrieved.getInt("int_value", 0));
    EXPECT_TRUE(retrieved.getBool("bool_value", false));
    EXPECT_DOUBLE_EQ(3.14, retrieved.getDouble("double_value", 0.0));
}

// Run all tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
