#include <gtest/gtest.h>
#include "core/framework_properties.h"

using namespace cdmf;

TEST(FrameworkPropertiesTest, DefaultConstructor) {
    FrameworkProperties props;

    // Check defaults are loaded
    EXPECT_EQ("CDMF", props.getFrameworkName());
    EXPECT_EQ("1.0.0", props.getFrameworkVersion());
    EXPECT_EQ("CDMF Project", props.getFrameworkVendor());
    EXPECT_FALSE(props.isSecurityEnabled());
    EXPECT_FALSE(props.isIpcEnabled());
    EXPECT_TRUE(props.isAutoStartModulesEnabled());
    EXPECT_EQ(4u, props.getEventThreadPoolSize());
    EXPECT_EQ(100u, props.getServiceCacheSize());
}

TEST(FrameworkPropertiesTest, SetAndGetFrameworkName) {
    FrameworkProperties props;

    props.setFrameworkName("TestFramework");
    EXPECT_EQ("TestFramework", props.getFrameworkName());
}

TEST(FrameworkPropertiesTest, SetAndGetFrameworkVersion) {
    FrameworkProperties props;

    props.setFrameworkVersion("2.0.0");
    EXPECT_EQ("2.0.0", props.getFrameworkVersion());
}

TEST(FrameworkPropertiesTest, SetAndGetFrameworkVendor) {
    FrameworkProperties props;

    props.setFrameworkVendor("Test Vendor");
    EXPECT_EQ("Test Vendor", props.getFrameworkVendor());
}

TEST(FrameworkPropertiesTest, SetAndGetSecurityEnabled) {
    FrameworkProperties props;

    EXPECT_FALSE(props.isSecurityEnabled());

    props.setSecurityEnabled(true);
    EXPECT_TRUE(props.isSecurityEnabled());

    props.setSecurityEnabled(false);
    EXPECT_FALSE(props.isSecurityEnabled());
}

TEST(FrameworkPropertiesTest, SetAndGetIpcEnabled) {
    FrameworkProperties props;

    EXPECT_FALSE(props.isIpcEnabled());

    props.setIpcEnabled(true);
    EXPECT_TRUE(props.isIpcEnabled());
}

TEST(FrameworkPropertiesTest, SetAndGetSignatureVerification) {
    FrameworkProperties props;

    EXPECT_FALSE(props.isSignatureVerificationEnabled());

    props.setSignatureVerificationEnabled(true);
    EXPECT_TRUE(props.isSignatureVerificationEnabled());
}

TEST(FrameworkPropertiesTest, SetAndGetAutoStartModules) {
    FrameworkProperties props;

    EXPECT_TRUE(props.isAutoStartModulesEnabled());

    props.setAutoStartModulesEnabled(false);
    EXPECT_FALSE(props.isAutoStartModulesEnabled());
}

TEST(FrameworkPropertiesTest, SetAndGetEventThreadPoolSize) {
    FrameworkProperties props;

    EXPECT_EQ(4u, props.getEventThreadPoolSize());

    props.setEventThreadPoolSize(8);
    EXPECT_EQ(8u, props.getEventThreadPoolSize());
}

TEST(FrameworkPropertiesTest, SetAndGetServiceCacheSize) {
    FrameworkProperties props;

    EXPECT_EQ(100u, props.getServiceCacheSize());

    props.setServiceCacheSize(200);
    EXPECT_EQ(200u, props.getServiceCacheSize());
}

TEST(FrameworkPropertiesTest, SetAndGetModuleSearchPath) {
    FrameworkProperties props;

    EXPECT_EQ("./modules", props.getModuleSearchPath());

    props.setModuleSearchPath("/opt/cdmf/modules");
    EXPECT_EQ("/opt/cdmf/modules", props.getModuleSearchPath());
}

TEST(FrameworkPropertiesTest, SetAndGetLogLevel) {
    FrameworkProperties props;

    EXPECT_EQ("INFO", props.getLogLevel());

    props.setLogLevel("DEBUG");
    EXPECT_EQ("DEBUG", props.getLogLevel());
}

TEST(FrameworkPropertiesTest, SetAndGetLogFile) {
    FrameworkProperties props;

    EXPECT_EQ("cdmf.log", props.getLogFile());

    props.setLogFile("/var/log/cdmf.log");
    EXPECT_EQ("/var/log/cdmf.log", props.getLogFile());
}

TEST(FrameworkPropertiesTest, ValidateDefaults) {
    FrameworkProperties props;
    EXPECT_TRUE(props.validate());
}

TEST(FrameworkPropertiesTest, ValidateInvalidThreadPoolSize) {
    FrameworkProperties props;

    props.setEventThreadPoolSize(0);
    EXPECT_FALSE(props.validate());

    props.setEventThreadPoolSize(101);
    EXPECT_FALSE(props.validate());

    props.setEventThreadPoolSize(10);
    EXPECT_TRUE(props.validate());
}

TEST(FrameworkPropertiesTest, ValidateInvalidCacheSize) {
    FrameworkProperties props;

    props.setServiceCacheSize(0);
    EXPECT_FALSE(props.validate());

    props.setServiceCacheSize(100);
    EXPECT_TRUE(props.validate());
}

TEST(FrameworkPropertiesTest, ConstructFromBaseProperties) {
    Properties base;
    base.set(FrameworkProperties::PROP_FRAMEWORK_NAME, std::string("CustomFramework"));
    base.set(FrameworkProperties::PROP_ENABLE_SECURITY, true);

    FrameworkProperties props(base);

    EXPECT_EQ("CustomFramework", props.getFrameworkName());
    EXPECT_TRUE(props.isSecurityEnabled());
    // Defaults should still be loaded for missing properties
    EXPECT_EQ("1.0.0", props.getFrameworkVersion());
}

TEST(FrameworkPropertiesTest, LoadDefaults) {
    FrameworkProperties props;

    // Clear some properties
    props.remove(FrameworkProperties::PROP_FRAMEWORK_NAME);
    props.remove(FrameworkProperties::PROP_EVENT_THREAD_POOL_SIZE);

    // Reload defaults
    props.loadDefaults();

    EXPECT_EQ("CDMF", props.getFrameworkName());
    EXPECT_EQ(4u, props.getEventThreadPoolSize());
}

TEST(FrameworkPropertiesTest, PropertyKeys) {
    // Verify property key constants are defined
    EXPECT_STREQ("framework.name", FrameworkProperties::PROP_FRAMEWORK_NAME);
    EXPECT_STREQ("framework.version", FrameworkProperties::PROP_FRAMEWORK_VERSION);
    EXPECT_STREQ("framework.vendor", FrameworkProperties::PROP_FRAMEWORK_VENDOR);
    EXPECT_STREQ("framework.security.enabled", FrameworkProperties::PROP_ENABLE_SECURITY);
    EXPECT_STREQ("framework.ipc.enabled", FrameworkProperties::PROP_ENABLE_IPC);
    EXPECT_STREQ("framework.security.verify_signatures", FrameworkProperties::PROP_VERIFY_SIGNATURES);
    EXPECT_STREQ("framework.modules.auto_start", FrameworkProperties::PROP_AUTO_START_MODULES);
    EXPECT_STREQ("framework.event.thread_pool_size", FrameworkProperties::PROP_EVENT_THREAD_POOL_SIZE);
    EXPECT_STREQ("framework.service.cache_size", FrameworkProperties::PROP_SERVICE_CACHE_SIZE);
    EXPECT_STREQ("framework.modules.search_path", FrameworkProperties::PROP_MODULE_SEARCH_PATH);
    EXPECT_STREQ("framework.log.level", FrameworkProperties::PROP_LOG_LEVEL);
    EXPECT_STREQ("framework.log.file", FrameworkProperties::PROP_LOG_FILE);
}

TEST(FrameworkPropertiesTest, InheritsFromProperties) {
    FrameworkProperties props;

    // Can use base Properties methods
    props.set("custom.property", std::string("custom_value"));
    EXPECT_TRUE(props.has("custom.property"));
    EXPECT_EQ("custom_value", props.getString("custom.property"));
}
