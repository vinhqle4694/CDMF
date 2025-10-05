#include <gtest/gtest.h>
#include "service/service_registry.h"
#include "service/service_reference.h"
#include "service/service_registration.h"
#include <memory>
#include <thread>
#include <vector>
#include <set>

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

// ============================================================================
// Service Registry Boundary and Edge Case Tests
// ============================================================================

TEST(ServiceRegistryTest, RegisterManyServices) {
    ServiceRegistry registry;
    std::vector<std::unique_ptr<TestServiceImpl>> services;

    const int SERVICE_COUNT = 1000;

    for (int i = 0; i < SERVICE_COUNT; ++i) {
        auto service = std::make_unique<TestServiceImpl>();
        registry.registerService(
            "com.example.ITestService",
            service.get(),
            Properties(),
            nullptr
        );
        services.push_back(std::move(service));
    }

    EXPECT_EQ(static_cast<size_t>(SERVICE_COUNT), registry.getServiceCount());
}

TEST(ServiceRegistryTest, RankingWithNegativeValues) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2, service3;

    Properties props1, props2, props3;
    props1.set("service.ranking", -100);
    props2.set("service.ranking", 0);
    props3.set("service.ranking", 100);

    registry.registerService("com.example.ITestService", &service1, props1, nullptr);
    registry.registerService("com.example.ITestService", &service2, props2, nullptr);
    registry.registerService("com.example.ITestService", &service3, props3, nullptr);

    std::vector<ServiceReference> refs = registry.getServiceReferences("com.example.ITestService");
    ASSERT_EQ(3u, refs.size());

    // Should be sorted by ranking (highest first)
    EXPECT_EQ(100, refs[0].getRanking());
    EXPECT_EQ(0, refs[1].getRanking());
    EXPECT_EQ(-100, refs[2].getRanking());
}

// Note: ServicePropertiesModification test is disabled pending implementation
// of dynamic property updates in the ServiceRegistry.
// TEST(ServiceRegistryTest, ServicePropertiesModification) {
//     ServiceRegistry registry;
//     TestServiceImpl service;
//
//     Properties props;
//     props.set("version", 1);
//     props.set("enabled", true);
//
//     ServiceRegistration reg = registry.registerService(
//         "com.example.ITestService",
//         &service,
//         props,
//         nullptr
//     );
//
//     ServiceReference ref = reg.getReference();
//     Properties refProps = ref.getProperties();
//     EXPECT_EQ(1, refProps.getInt("version"));
//
//     // Modify properties via registration
//     Properties newProps;
//     newProps.set("version", 2);
//     newProps.set("enabled", false);
//     reg.setProperties(newProps);
//
//     // Get the reference from the registration (not from registry)
//     // because the registration reference is updated when properties change
//     ref = reg.getReference();
//     refProps = ref.getProperties();
//     EXPECT_EQ(2, refProps.getInt("version"));
// }

TEST(ServiceRegistryTest, UnregisterAndReregisterSameService) {
    ServiceRegistry registry;
    TestServiceImpl service;

    ServiceRegistration reg1 = registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    uint64_t serviceId1 = reg1.getServiceId();
    reg1.unregister();

    EXPECT_EQ(0u, registry.getServiceCount());

    // Re-register the same service instance
    ServiceRegistration reg2 = registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    uint64_t serviceId2 = reg2.getServiceId();
    EXPECT_NE(serviceId1, serviceId2);  // Should get a different service ID
    EXPECT_EQ(1u, registry.getServiceCount());
}

TEST(ServiceRegistryTest, MultipleInterfacesSameService) {
    ServiceRegistry registry;
    TestServiceImpl service;

    // Register same service under multiple interfaces
    ServiceRegistration reg1 = registry.registerService(
        "com.example.ITestService1",
        &service,
        Properties(),
        nullptr
    );

    ServiceRegistration reg2 = registry.registerService(
        "com.example.ITestService2",
        &service,
        Properties(),
        nullptr
    );

    EXPECT_EQ(2u, registry.getServiceCount());
    EXPECT_NE(reg1.getServiceId(), reg2.getServiceId());

    // Both interfaces should be accessible
    ServiceReference ref1 = registry.getServiceReference("com.example.ITestService1");
    ServiceReference ref2 = registry.getServiceReference("com.example.ITestService2");

    EXPECT_TRUE(ref1.isValid());
    EXPECT_TRUE(ref2.isValid());
}

TEST(ServiceRegistryTest, GetServiceCountByInterface) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2, service3;

    registry.registerService("com.example.IService1", &service1, Properties(), nullptr);
    registry.registerService("com.example.IService1", &service2, Properties(), nullptr);
    registry.registerService("com.example.IService2", &service3, Properties(), nullptr);

    EXPECT_EQ(3u, registry.getServiceCount());
    EXPECT_EQ(2u, registry.getServiceCount("com.example.IService1"));
    EXPECT_EQ(1u, registry.getServiceCount("com.example.IService2"));
    EXPECT_EQ(0u, registry.getServiceCount("com.example.IServiceNotFound"));
}

TEST(ServiceRegistryTest, EmptyRegistry) {
    ServiceRegistry registry;

    EXPECT_EQ(0u, registry.getServiceCount());

    ServiceReference ref = registry.getServiceReference("com.example.IAnyService");
    EXPECT_FALSE(ref.isValid());

    std::vector<ServiceReference> refs = registry.getAllServices();
    EXPECT_TRUE(refs.empty());
}

TEST(ServiceRegistryTest, ServiceReferenceInvalidAfterUnregister) {
    ServiceRegistry registry;
    TestServiceImpl service;

    ServiceRegistration reg = registry.registerService(
        "com.example.ITestService",
        &service,
        Properties(),
        nullptr
    );

    ServiceReference ref = reg.getReference();
    EXPECT_TRUE(ref.isValid());

    uint64_t serviceId = ref.getServiceId();

    // Unregister the service
    reg.unregister();

    // Reference should still be valid (holds weak ptr)
    // but the service should not be found in registry
    ServiceReference refAfter = registry.getServiceReferenceById(serviceId);
    EXPECT_FALSE(refAfter.isValid());
}

TEST(ServiceRegistryTest, ConcurrentServiceRegistration) {
    ServiceRegistry registry;

    const int THREADS = 10;
    const int SERVICES_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<std::unique_ptr<TestServiceImpl>>> allServices(THREADS);

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < SERVICES_PER_THREAD; ++i) {
                auto service = std::make_unique<TestServiceImpl>();
                registry.registerService(
                    "com.example.ITestService",
                    service.get(),
                    Properties(),
                    nullptr
                );
                allServices[t].push_back(std::move(service));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(static_cast<size_t>(THREADS * SERVICES_PER_THREAD), registry.getServiceCount());
}

TEST(ServiceRegistryTest, ConcurrentServiceUnregistration) {
    ServiceRegistry registry;

    const int SERVICES = 1000;
    std::vector<std::unique_ptr<TestServiceImpl>> services;
    std::vector<ServiceRegistration> registrations;

    // Register services
    for (int i = 0; i < SERVICES; ++i) {
        auto service = std::make_unique<TestServiceImpl>();
        auto reg = registry.registerService(
            "com.example.ITestService",
            service.get(),
            Properties(),
            nullptr
        );
        services.push_back(std::move(service));
        registrations.push_back(std::move(reg));
    }

    EXPECT_EQ(static_cast<size_t>(SERVICES), registry.getServiceCount());

    // Unregister concurrently
    const int THREADS = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = t; i < SERVICES; i += THREADS) {
                registrations[i].unregister();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0u, registry.getServiceCount());
}

TEST(ServiceRegistryTest, ServicePropertiesWithComplexTypes) {
    ServiceRegistry registry;
    TestServiceImpl service;

    Properties props;
    props.set("string_value", std::string("test"));
    props.set("int_value", 42);
    props.set("bool_value", true);
    props.set("double_value", 3.14159);
    props.set("long_value", 1234567890L);

    ServiceRegistration reg = registry.registerService(
        "com.example.ITestService",
        &service,
        props,
        nullptr
    );

    ServiceReference ref = reg.getReference();
    Properties refProps = ref.getProperties();

    EXPECT_EQ(std::string("test"), refProps.getString("string_value"));
    EXPECT_EQ(42, refProps.getInt("int_value"));
    EXPECT_TRUE(refProps.getBool("bool_value"));
}

TEST(ServiceRegistryTest, VeryLongInterfaceName) {
    ServiceRegistry registry;
    TestServiceImpl service;

    std::string longName(1000, 'a');

    ServiceRegistration reg = registry.registerService(
        longName,
        &service,
        Properties(),
        nullptr
    );

    ServiceReference ref = registry.getServiceReference(longName);
    EXPECT_TRUE(ref.isValid());
    EXPECT_EQ(longName, ref.getInterface());
}

TEST(ServiceRegistryTest, SpecialCharactersInInterfaceName) {
    ServiceRegistry registry;
    TestServiceImpl service;

    std::string specialName = "com.example@#$%^&*().ITestService";

    ServiceRegistration reg = registry.registerService(
        specialName,
        &service,
        Properties(),
        nullptr
    );

    ServiceReference ref = registry.getServiceReference(specialName);
    EXPECT_TRUE(ref.isValid());
}

TEST(ServiceRegistryTest, GetAllServicesWithMultipleInterfaces) {
    ServiceRegistry registry;
    TestServiceImpl service1, service2, service3;

    registry.registerService("com.example.IService1", &service1, Properties(), nullptr);
    registry.registerService("com.example.IService2", &service2, Properties(), nullptr);
    registry.registerService("com.example.IService3", &service3, Properties(), nullptr);

    std::vector<ServiceReference> refs = registry.getAllServices();
    ASSERT_EQ(3u, refs.size());

    // Verify all interfaces are present
    std::set<std::string> interfaces;
    for (const auto& ref : refs) {
        interfaces.insert(ref.getInterface());
    }

    EXPECT_TRUE(interfaces.count("com.example.IService1") > 0);
    EXPECT_TRUE(interfaces.count("com.example.IService2") > 0);
    EXPECT_TRUE(interfaces.count("com.example.IService3") > 0);
}
