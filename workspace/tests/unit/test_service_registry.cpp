#include <gtest/gtest.h>
#include "service/service_registry.h"
#include "service/service_reference.h"
#include "service/service_registration.h"

using namespace cdmf;

// ============================================================================
// Service Registry Tests
// ============================================================================

class TestService {
public:
    virtual ~TestService() = default;
    virtual void execute() = 0;
};

class TestServiceImpl : public TestService {
public:
    void execute() override {}
};

TEST(ServiceRegistryTest, RegisterService) {
    ServiceRegistry registry;
    TestServiceImpl service;

    ServiceRegistration reg = registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    EXPECT_TRUE(reg.isValid());
    EXPECT_NE(0u, reg.getServiceId());
    EXPECT_EQ(1u, registry.getServiceCount());
}

TEST(ServiceRegistryTest, RegisterMultipleServices) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2;

    ServiceRegistration reg1 = registry.registerService(
        "com.example.ITestService",
        &service1,
        Properties(),
        nullptr
    );

    ServiceRegistration reg2 = registry.registerService(
        "com.example.ITestService",
        &service2,
        Properties(),
        nullptr
    );

    EXPECT_EQ(2u, registry.getServiceCount());
    EXPECT_EQ(2u, registry.getServiceCount("com.example.ITestService"));
}

TEST(ServiceRegistryTest, UnregisterService) {
    ServiceRegistry registry;
    TestServiceImpl service;

    ServiceRegistration reg = registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    uint64_t serviceId = reg.getServiceId();
    EXPECT_TRUE(registry.unregisterService(serviceId));
    EXPECT_EQ(0u, registry.getServiceCount());
}

TEST(ServiceRegistryTest, UnregisterNonexistentService) {
    ServiceRegistry registry;

    EXPECT_FALSE(registry.unregisterService(999));
}

TEST(ServiceRegistryTest, GetServiceReference) {
    ServiceRegistry registry;
    TestServiceImpl service;

    registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    ServiceReference ref = registry.getServiceReference("com.example.ITestService");
    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ("com.example.ITestService", ref.getInterface());
}

TEST(ServiceRegistryTest, GetServiceReferenceNotFound) {
    ServiceRegistry registry;

    ServiceReference ref = registry.getServiceReference("com.example.NotFound");
    EXPECT_FALSE(ref.isValid());
}

TEST(ServiceRegistryTest, GetServiceReferences) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2;

    registry.registerService("com.example.ITestService", &service1, Properties(), nullptr);
    registry.registerService("com.example.ITestService", &service2, Properties(), nullptr);

    std::vector<ServiceReference> refs = registry.getServiceReferences("com.example.ITestService");
    EXPECT_EQ(2u, refs.size());
}

TEST(ServiceRegistryTest, GetServiceReferencesByRanking) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2, service3;

    Properties props1, props2, props3;
    props1.set("service.ranking", 10);
    props2.set("service.ranking", 100);
    props3.set("service.ranking", 50);

    registry.registerService("com.example.ITestService", &service1, props1, nullptr);
    registry.registerService("com.example.ITestService", &service2, props2, nullptr);
    registry.registerService("com.example.ITestService", &service3, props3, nullptr);

    std::vector<ServiceReference> refs = registry.getServiceReferences("com.example.ITestService");
    ASSERT_EQ(3u, refs.size());

    // Should be sorted by ranking (highest first)
    EXPECT_EQ(100, refs[0].getRanking());
    EXPECT_EQ(50, refs[1].getRanking());
    EXPECT_EQ(10, refs[2].getRanking());
}

TEST(ServiceRegistryTest, GetServiceReferenceHighestRanking) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2;

    Properties props1, props2;
    props1.set("service.ranking", 10);
    props2.set("service.ranking", 100);

    registry.registerService("com.example.ITestService", &service1, props1, nullptr);
    registry.registerService("com.example.ITestService", &service2, props2, nullptr);

    ServiceReference ref = registry.getServiceReference("com.example.ITestService");
    EXPECT_EQ(100, ref.getRanking()); // Should return highest ranking
}

TEST(ServiceRegistryTest, GetAllServices) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2;

    registry.registerService("com.example.IService1", &service1, Properties(), nullptr);
    registry.registerService("com.example.IService2", &service2, Properties(), nullptr);

    std::vector<ServiceReference> refs = registry.getAllServices();
    EXPECT_EQ(2u, refs.size());
}

TEST(ServiceRegistryTest, GetServiceReferenceById) {
    ServiceRegistry registry;
    TestServiceImpl service;

    ServiceRegistration reg = registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    uint64_t serviceId = reg.getServiceId();
    ServiceReference ref = registry.getServiceReferenceById(serviceId);

    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ(serviceId, ref.getServiceId());
}

TEST(ServiceRegistryTest, InvalidArguments) {
    ServiceRegistry registry;
    TestServiceImpl service;

    EXPECT_THROW(
        registry.registerService("", &service, Properties(), nullptr),
        std::invalid_argument
    );

    EXPECT_THROW(
        registry.registerService("com.example.ITestService", nullptr, Properties(), nullptr),
        std::invalid_argument
    );
}
