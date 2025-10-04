#include <gtest/gtest.h>
#include "module/module_types.h"
#include "module/module_activator.h"
#include "service/service_types.h"

using namespace cdmf;

// ============================================================================
// Module Types Tests
// ============================================================================

TEST(ModuleTypesTest, ModuleStateToString) {
    EXPECT_STREQ("INSTALLED", moduleStateToString(ModuleState::INSTALLED));
    EXPECT_STREQ("RESOLVED", moduleStateToString(ModuleState::RESOLVED));
    EXPECT_STREQ("STARTING", moduleStateToString(ModuleState::STARTING));
    EXPECT_STREQ("ACTIVE", moduleStateToString(ModuleState::ACTIVE));
    EXPECT_STREQ("STOPPING", moduleStateToString(ModuleState::STOPPING));
    EXPECT_STREQ("UNINSTALLED", moduleStateToString(ModuleState::UNINSTALLED));
}

TEST(ModuleTypesTest, ModuleEventTypeToString) {
    EXPECT_STREQ("MODULE_INSTALLED", moduleEventTypeToString(ModuleEventType::MODULE_INSTALLED));
    EXPECT_STREQ("MODULE_RESOLVED", moduleEventTypeToString(ModuleEventType::MODULE_RESOLVED));
    EXPECT_STREQ("MODULE_STARTING", moduleEventTypeToString(ModuleEventType::MODULE_STARTING));
    EXPECT_STREQ("MODULE_STARTED", moduleEventTypeToString(ModuleEventType::MODULE_STARTED));
    EXPECT_STREQ("MODULE_STOPPING", moduleEventTypeToString(ModuleEventType::MODULE_STOPPING));
    EXPECT_STREQ("MODULE_STOPPED", moduleEventTypeToString(ModuleEventType::MODULE_STOPPED));
    EXPECT_STREQ("MODULE_UPDATED", moduleEventTypeToString(ModuleEventType::MODULE_UPDATED));
    EXPECT_STREQ("MODULE_UNINSTALLED", moduleEventTypeToString(ModuleEventType::MODULE_UNINSTALLED));
    EXPECT_STREQ("MODULE_RESOLVED_FAILED", moduleEventTypeToString(ModuleEventType::MODULE_RESOLVED_FAILED));
}

// ============================================================================
// Service Types Tests
// Note: Full service tests are in PHASE_5 test files:
// - test_service_reference.cpp
// - test_service_registration.cpp
// - test_service_registry.cpp
// - test_service_tracker.cpp
// ============================================================================

TEST(ServiceTypesTest, ServiceReferenceDefaultConstruction) {
    // Default construction - invalid
    ServiceReference ref;
    EXPECT_FALSE(ref.isValid());
    EXPECT_EQ(0u, ref.getServiceId());
}

TEST(ServiceTypesTest, ServiceRegistrationDefaultConstruction) {
    // Default construction - invalid
    ServiceRegistration reg;
    EXPECT_FALSE(reg.isValid());
    EXPECT_EQ(0u, reg.getServiceId());
}

// ============================================================================
// Module Activator Interface Tests
// ============================================================================

// Mock activator for testing interface
class MockActivator : public IModuleActivator {
public:
    bool startCalled = false;
    bool stopCalled = false;
    IModuleContext* startContext = nullptr;
    IModuleContext* stopContext = nullptr;

    void start(IModuleContext* context) override {
        startCalled = true;
        startContext = context;
    }

    void stop(IModuleContext* context) override {
        stopCalled = true;
        stopContext = context;
    }
};

TEST(ModuleActivatorTest, InterfaceCompilation) {
    MockActivator activator;

    EXPECT_FALSE(activator.startCalled);
    EXPECT_FALSE(activator.stopCalled);

    activator.start(nullptr);
    EXPECT_TRUE(activator.startCalled);
    EXPECT_FALSE(activator.stopCalled);

    activator.stop(nullptr);
    EXPECT_TRUE(activator.startCalled);
    EXPECT_TRUE(activator.stopCalled);
}

TEST(ModuleActivatorTest, PolymorphicUsage) {
    MockActivator mockActivator;
    IModuleActivator* activator = &mockActivator;

    activator->start(nullptr);
    activator->stop(nullptr);

    EXPECT_TRUE(mockActivator.startCalled);
    EXPECT_TRUE(mockActivator.stopCalled);
}

// ============================================================================
// Module Exception Tests
// ============================================================================

#include "module/module.h"

TEST(ModuleExceptionTest, Construction) {
    ModuleException ex("Test error");
    EXPECT_STREQ("Test error", ex.what());
}

TEST(ModuleExceptionTest, ConstructionWithState) {
    ModuleException ex("Cannot start", ModuleState::INSTALLED);
    std::string message = ex.what();

    EXPECT_NE(message.find("Cannot start"), std::string::npos);
    EXPECT_NE(message.find("INSTALLED"), std::string::npos);
}

TEST(ModuleExceptionTest, ThrowAndCatch) {
    bool caught = false;

    try {
        throw ModuleException("Test exception");
    } catch (const ModuleException& e) {
        caught = true;
        EXPECT_STREQ("Test exception", e.what());
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }

    EXPECT_TRUE(caught);
}

TEST(ModuleExceptionTest, CatchAsStdException) {
    bool caught = false;

    try {
        throw ModuleException("Test exception");
    } catch (const std::runtime_error& e) {
        caught = true;
        EXPECT_STREQ("Test exception", e.what());
    } catch (...) {
        FAIL() << "Wrong exception type caught";
    }

    EXPECT_TRUE(caught);
}
