#include <gtest/gtest.h>
#include "service/service_registration.h"
#include "service/service_registry.h"
#include "service/service_entry.h"

using namespace cdmf;

// ============================================================================
// Service Registration Tests
// ============================================================================

class DummyService {
public:
    void doSomething() {}
};

TEST(ServiceRegistrationTest, DefaultConstructor) {
    ServiceRegistration reg;

    EXPECT_FALSE(reg.isValid());
    EXPECT_EQ(0u, reg.getServiceId());
}

TEST(ServiceRegistrationTest, GetReference) {
    ServiceRegistry registry;
    DummyService service;

    ServiceRegistration reg = registry.registerService(
        "com.example.IDummy",
        &service,
        Properties(),
        nullptr
    );

    EXPECT_TRUE(reg.isValid());
    EXPECT_NE(0u, reg.getServiceId());

    ServiceReference ref = reg.getReference();
    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ(reg.getServiceId(), ref.getServiceId());
    EXPECT_EQ("com.example.IDummy", ref.getInterface());
}

TEST(ServiceRegistrationTest, Unregister) {
    ServiceRegistry registry;
    DummyService service;

    ServiceRegistration reg = registry.registerService(
        "com.example.IDummy",
        &service,
        Properties(),
        nullptr
    );

    EXPECT_TRUE(reg.isValid());

    reg.unregister();

    EXPECT_FALSE(reg.isValid());
    EXPECT_EQ(0u, reg.getServiceId());
}

TEST(ServiceRegistrationTest, UnregisterTwice) {
    ServiceRegistry registry;
    DummyService service;

    ServiceRegistration reg = registry.registerService(
        "com.example.IDummy",
        &service,
        Properties(),
        nullptr
    );

    reg.unregister();
    reg.unregister(); // Should be no-op

    EXPECT_FALSE(reg.isValid());
}

TEST(ServiceRegistrationTest, Comparison) {
    ServiceRegistry registry;
    DummyService service1, service2;

    ServiceRegistration reg1 = registry.registerService(
        "com.example.IDummy",
        &service1,
        Properties(),
        nullptr
    );

    ServiceRegistration reg2 = registry.registerService(
        "com.example.IDummy",
        &service2,
        Properties(),
        nullptr
    );

    ServiceRegistration reg1_copy = reg1;

    EXPECT_EQ(reg1, reg1_copy);
    EXPECT_NE(reg1, reg2);
}
