#include "ipc/metadata.h"
#include "ipc/proxy_generator.h"
#include "ipc/reflection_proxy_generator.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace ipc::metadata;
using namespace ipc::proxy;

// Test fixture for metadata tests
class MetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple service metadata for testing
        serviceMetadata_ = std::make_shared<ServiceMetadata>("TestService", "1.0.0");
        serviceMetadata_->setNamespace("test");
        serviceMetadata_->setDescription("Test service for unit tests");
        serviceMetadata_->setServiceId(1001);

        // Add a simple method: int add(int a, int b)
        auto intType = TypeRegistry::instance().getType("int32");
        auto addMethod = std::make_shared<MethodMetadata>("add", intType);
        addMethod->setMethodId(1);
        addMethod->setTimeout(3000);

        auto paramA = std::make_shared<ParameterMetadata>("a", intType, ParameterDirection::IN);
        auto paramB = std::make_shared<ParameterMetadata>("b", intType, ParameterDirection::IN);

        addMethod->addParameter(paramA);
        addMethod->addParameter(paramB);
        serviceMetadata_->addMethod(addMethod);

        // Add void method: void notify(string message)
        auto voidType = TypeRegistry::instance().getType("void");
        auto stringType = TypeRegistry::instance().getType("string");

        auto notifyMethod = std::make_shared<MethodMetadata>("notify", voidType);
        notifyMethod->setMethodId(2);
        notifyMethod->setCallType(MethodCallType::ONEWAY);

        auto paramMsg = std::make_shared<ParameterMetadata>("message", stringType, ParameterDirection::IN);
        notifyMethod->addParameter(paramMsg);
        serviceMetadata_->addMethod(notifyMethod);
    }

    std::shared_ptr<ServiceMetadata> serviceMetadata_;
};

// Test TypeDescriptor
TEST(TypeDescriptorTest, BasicProperties) {
    auto intType = std::make_shared<TypeDescriptor>(
        "int32",
        std::type_index(typeid(int32_t)),
        sizeof(int32_t),
        true
    );

    EXPECT_EQ(intType->getName(), "int32");
    EXPECT_EQ(intType->getSize(), sizeof(int32_t));
    EXPECT_TRUE(intType->isPrimitive());
    EXPECT_FALSE(intType->isArray());
    EXPECT_FALSE(intType->isPointer());
}

TEST(TypeDescriptorTest, ArrayType) {
    auto intType = std::make_shared<TypeDescriptor>(
        "int32",
        std::type_index(typeid(int32_t)),
        sizeof(int32_t),
        true
    );

    auto arrayType = std::make_shared<TypeDescriptor>(
        "int32[]",
        std::type_index(typeid(int32_t*)),
        sizeof(int32_t*),
        false
    );
    arrayType->setArray(true);
    arrayType->setElementType(intType);

    EXPECT_TRUE(arrayType->isArray());
    EXPECT_EQ(arrayType->getElementType(), intType);
}

TEST(TypeDescriptorTest, JsonSerialization) {
    auto intType = std::make_shared<TypeDescriptor>(
        "int32",
        std::type_index(typeid(int32_t)),
        sizeof(int32_t),
        true
    );

    std::string json = intType->toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("int32"), std::string::npos);
    EXPECT_NE(json.find("\"isPrimitive\":true"), std::string::npos);
}

// Test ParameterMetadata
TEST(ParameterMetadataTest, BasicProperties) {
    auto intType = std::make_shared<TypeDescriptor>(
        "int32",
        std::type_index(typeid(int32_t)),
        sizeof(int32_t),
        true
    );

    auto param = std::make_shared<ParameterMetadata>("value", intType, ParameterDirection::IN);

    EXPECT_EQ(param->getName(), "value");
    EXPECT_EQ(param->getType(), intType);
    EXPECT_EQ(param->getDirection(), ParameterDirection::IN);
}

TEST(ParameterMetadataTest, Annotations) {
    auto intType = std::make_shared<TypeDescriptor>(
        "int32",
        std::type_index(typeid(int32_t)),
        sizeof(int32_t),
        true
    );

    auto param = std::make_shared<ParameterMetadata>("value", intType);
    param->addAnnotation("validation", "range(0,100)");
    param->addAnnotation("default", "42");

    auto validation = param->getAnnotation("validation");
    ASSERT_TRUE(validation.has_value());
    EXPECT_EQ(*validation, "range(0,100)");

    auto defaultVal = param->getAnnotation("default");
    ASSERT_TRUE(defaultVal.has_value());
    EXPECT_EQ(*defaultVal, "42");

    auto notFound = param->getAnnotation("notexist");
    EXPECT_FALSE(notFound.has_value());
}

// Test MethodMetadata
TEST(MethodMetadataTest, BasicProperties) {
    auto intType = TypeRegistry::instance().getType("int32");

    auto method = std::make_shared<MethodMetadata>("calculate", intType);
    method->setMethodId(100);
    method->setTimeout(5000);
    method->setCallType(MethodCallType::SYNCHRONOUS);

    EXPECT_EQ(method->getName(), "calculate");
    EXPECT_EQ(method->getMethodId(), 100);
    EXPECT_EQ(method->getReturnType(), intType);
    EXPECT_EQ(method->getCallType(), MethodCallType::SYNCHRONOUS);

    auto timeout = method->getTimeout();
    ASSERT_TRUE(timeout.has_value());
    EXPECT_EQ(*timeout, 5000);
}

TEST(MethodMetadataTest, Parameters) {
    auto intType = TypeRegistry::instance().getType("int32");
    auto method = std::make_shared<MethodMetadata>("add", intType);

    auto paramA = std::make_shared<ParameterMetadata>("a", intType, ParameterDirection::IN);
    auto paramB = std::make_shared<ParameterMetadata>("b", intType, ParameterDirection::IN);

    method->addParameter(paramA);
    method->addParameter(paramB);

    const auto& params = method->getParameters();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0]->getName(), "a");
    EXPECT_EQ(params[1]->getName(), "b");
}

// Test ServiceMetadata
TEST_F(MetadataTest, ServiceBasicProperties) {
    EXPECT_EQ(serviceMetadata_->getName(), "TestService");
    EXPECT_EQ(serviceMetadata_->getVersion(), "1.0.0");
    EXPECT_EQ(serviceMetadata_->getNamespace(), "test");
    EXPECT_EQ(serviceMetadata_->getServiceId(), 1001);
}

TEST_F(MetadataTest, ServiceMethods) {
    const auto& methods = serviceMetadata_->getMethods();
    EXPECT_EQ(methods.size(), 2);

    auto addMethod = serviceMetadata_->getMethod("add");
    ASSERT_NE(addMethod, nullptr);
    EXPECT_EQ(addMethod->getName(), "add");
    EXPECT_EQ(addMethod->getParameters().size(), 2);

    auto notifyMethod = serviceMetadata_->getMethod("notify");
    ASSERT_NE(notifyMethod, nullptr);
    EXPECT_EQ(notifyMethod->getCallType(), MethodCallType::ONEWAY);

    auto notFound = serviceMetadata_->getMethod("notexist");
    EXPECT_EQ(notFound, nullptr);
}

TEST_F(MetadataTest, JsonSerialization) {
    std::string json = serviceMetadata_->toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("TestService"), std::string::npos);
    EXPECT_NE(json.find("1.0.0"), std::string::npos);
    EXPECT_NE(json.find("add"), std::string::npos);
    EXPECT_NE(json.find("notify"), std::string::npos);
}

// Test TypeRegistry
TEST(TypeRegistryTest, BuiltinTypes) {
    auto& registry = TypeRegistry::instance();

    auto intType = registry.getType("int32");
    ASSERT_NE(intType, nullptr);
    EXPECT_EQ(intType->getName(), "int32");
    EXPECT_TRUE(intType->isPrimitive());

    auto stringType = registry.getType("string");
    ASSERT_NE(stringType, nullptr);
    EXPECT_EQ(stringType->getName(), "string");

    auto voidType = registry.getType("void");
    ASSERT_NE(voidType, nullptr);
    EXPECT_TRUE(voidType->isPrimitive());
}

TEST(TypeRegistryTest, CustomType) {
    auto& registry = TypeRegistry::instance();

    auto customType = std::make_shared<TypeDescriptor>(
        "CustomStruct",
        std::type_index(typeid(void)),
        128,
        false
    );

    registry.registerType(customType);

    auto retrieved = registry.getType("CustomStruct");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getName(), "CustomStruct");
    EXPECT_EQ(retrieved->getSize(), 128);
}

// Test MockInvocationHandler
TEST(MockInvocationHandlerTest, BasicInvocation) {
    auto handler = std::make_shared<MockInvocationHandler>();
    auto intType = TypeRegistry::instance().getType("int32");

    auto method = std::make_shared<MethodMetadata>("add", intType);
    method->setMethodId(1);

    handler->setReturnValue("add", std::any(42));

    InvocationContext context;
    context.methodMetadata = method;
    context.arguments = {std::any(10), std::any(32)};

    auto result = handler->invoke(context);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(std::any_cast<int>(result.returnValue), 42);
    EXPECT_EQ(handler->getCallCount("add"), 1);
}

TEST(MockInvocationHandlerTest, ExceptionHandling) {
    auto handler = std::make_shared<MockInvocationHandler>();
    auto intType = TypeRegistry::instance().getType("int32");

    auto method = std::make_shared<MethodMetadata>("divide", intType);
    handler->setException("divide", "DivideByZeroException", "Cannot divide by zero");

    InvocationContext context;
    context.methodMetadata = method;
    context.arguments = {std::any(10), std::any(0)};

    auto result = handler->invoke(context);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.exceptionType, "DivideByZeroException");
    EXPECT_EQ(result.errorMessage, "Cannot divide by zero");
}

TEST(MockInvocationHandlerTest, CustomHandler) {
    auto handler = std::make_shared<MockInvocationHandler>();
    auto intType = TypeRegistry::instance().getType("int32");

    auto method = std::make_shared<MethodMetadata>("multiply", intType);

    handler->setMethodHandler("multiply", [](const InvocationContext& ctx) {
        InvocationResult result;
        result.success = true;
        int a = std::any_cast<int>(ctx.arguments[0]);
        int b = std::any_cast<int>(ctx.arguments[1]);
        result.returnValue = std::any(a * b);
        return result;
    });

    InvocationContext context;
    context.methodMetadata = method;
    context.arguments = {std::any(6), std::any(7)};

    auto result = handler->invoke(context);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(std::any_cast<int>(result.returnValue), 42);
}

// Test ReflectionProxyGenerator
TEST_F(MetadataTest, GenerateProxy) {
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto handler = std::make_shared<MockInvocationHandler>();

    handler->setReturnValue("add", std::any(15));

    auto proxy = generator->generateProxy(serviceMetadata_, handler);
    ASSERT_NE(proxy, nullptr);

    auto reflectionProxy = std::dynamic_pointer_cast<ReflectionServiceProxy>(proxy);
    ASSERT_NE(reflectionProxy, nullptr);

    EXPECT_TRUE(reflectionProxy->hasMethod("add"));
    EXPECT_TRUE(reflectionProxy->hasMethod("notify"));
    EXPECT_FALSE(reflectionProxy->hasMethod("notexist"));
}

TEST_F(MetadataTest, ProxyMethodInvocation) {
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto handler = std::make_shared<MockInvocationHandler>();

    handler->setReturnValue("add", std::any(15));

    auto proxy = generator->generateProxy(serviceMetadata_, handler);
    auto reflectionProxy = std::dynamic_pointer_cast<ReflectionServiceProxy>(proxy);

    std::vector<std::any> args = {std::any(10), std::any(5)};
    auto result = reflectionProxy->invoke("add", args);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(std::any_cast<int>(result.returnValue), 15);
    EXPECT_EQ(handler->getCallCount("add"), 1);
}

TEST_F(MetadataTest, ProxyAsyncInvocation) {
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto handler = std::make_shared<MockInvocationHandler>();

    handler->setReturnValue("add", std::any(100));

    auto proxy = generator->generateProxy(serviceMetadata_, handler);
    auto reflectionProxy = std::dynamic_pointer_cast<ReflectionServiceProxy>(proxy);

    std::vector<std::any> args = {std::any(50), std::any(50)};
    auto futureResult = reflectionProxy->invokeAsync("add", args);

    auto result = futureResult.get();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(std::any_cast<int>(result.returnValue), 100);
}

TEST_F(MetadataTest, ProxyOnewayInvocation) {
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto handler = std::make_shared<MockInvocationHandler>();

    auto proxy = generator->generateProxy(serviceMetadata_, handler);
    auto reflectionProxy = std::dynamic_pointer_cast<ReflectionServiceProxy>(proxy);

    std::vector<std::any> args = {std::any(std::string("Hello"))};
    EXPECT_NO_THROW(reflectionProxy->invokeOneway("notify", args));

    EXPECT_EQ(handler->getCallCount("notify"), 1);
}

// Test ServiceMetadataBuilder
TEST(ServiceMetadataBuilderTest, BuildSimpleService) {
    ServiceMetadataBuilder builder("CalculatorService", "2.0.0");

    auto service = builder
        .setNamespace("math")
        .setDescription("Simple calculator service")
        .setServiceId(2001)
        .beginMethod("add", "int32")
            .setMethodId(1)
            .addParameter("a", "int32", ParameterDirection::IN)
            .addParameter("b", "int32", ParameterDirection::IN)
            .setMethodTimeout(3000)
            .endMethod()
        .beginMethod("subtract", "int32")
            .setMethodId(2)
            .addParameter("a", "int32", ParameterDirection::IN)
            .addParameter("b", "int32", ParameterDirection::IN)
            .endMethod()
        .build();

    EXPECT_EQ(service->getName(), "CalculatorService");
    EXPECT_EQ(service->getVersion(), "2.0.0");
    EXPECT_EQ(service->getNamespace(), "math");
    EXPECT_EQ(service->getServiceId(), 2001);
    EXPECT_EQ(service->getMethods().size(), 2);

    auto addMethod = service->getMethod("add");
    ASSERT_NE(addMethod, nullptr);
    EXPECT_EQ(addMethod->getMethodId(), 1);
    EXPECT_EQ(addMethod->getParameters().size(), 2);
}

TEST(ServiceMetadataBuilderTest, BuildAsyncService) {
    ServiceMetadataBuilder builder("AsyncService", "1.0.0");

    auto service = builder
        .beginMethod("fetchData", "string")
            .setMethodId(1)
            .setMethodCallType(MethodCallType::ASYNCHRONOUS)
            .addParameter("url", "string", ParameterDirection::IN)
            .setMethodTimeout(10000)
            .addMethodAnnotation("cache", "true")
            .endMethod()
        .build();

    auto fetchMethod = service->getMethod("fetchData");
    ASSERT_NE(fetchMethod, nullptr);
    EXPECT_EQ(fetchMethod->getCallType(), MethodCallType::ASYNCHRONOUS);

    auto timeout = fetchMethod->getTimeout();
    ASSERT_TRUE(timeout.has_value());
    EXPECT_EQ(*timeout, 10000);
}

// Test ProxyGeneratorRegistry
TEST(ProxyGeneratorRegistryTest, RegisterAndRetrieve) {
    auto& registry = ProxyGeneratorRegistry::instance();
    auto generator = std::make_shared<ReflectionProxyGenerator>();

    registry.registerGenerator(generator);

    auto retrieved = registry.getGenerator("ReflectionProxyGenerator");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getName(), "ReflectionProxyGenerator");
}

TEST(ProxyGeneratorRegistryTest, FindGeneratorForService) {
    auto& registry = ProxyGeneratorRegistry::instance();
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    registry.registerGenerator(generator);

    ServiceMetadataBuilder builder("TestService", "1.0.0");
    auto service = builder
        .beginMethod("test", "void")
            .setMethodId(1)
            .endMethod()
        .build();

    auto found = registry.findGeneratorForService(service);
    ASSERT_NE(found, nullptr);
    EXPECT_TRUE(found->supportsService(service));
}

// Test typed invocation helpers
TEST_F(MetadataTest, TypedMethodInvocation) {
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto handler = std::make_shared<MockInvocationHandler>();

    handler->setReturnValue("add", std::any(42));

    auto proxy = generator->generateProxy(serviceMetadata_, handler);

    int result = invokeTypedMethod<int>(proxy.get(), "add", 20, 22);
    EXPECT_EQ(result, 42);
}

TEST_F(MetadataTest, TypedMethodException) {
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto handler = std::make_shared<MockInvocationHandler>();

    handler->setException("add", "InvalidArgumentException", "Invalid arguments");

    auto proxy = generator->generateProxy(serviceMetadata_, handler);

    EXPECT_THROW({
        invokeTypedMethod<int>(proxy.get(), "add", 10, 20);
    }, RemoteException);
}

// Integration test
TEST(ProxyGeneratorIntegrationTest, EndToEndScenario) {
    // Build service metadata
    ServiceMetadataBuilder builder("UserService", "1.0.0");
    auto service = builder
        .setNamespace("user")
        .setDescription("User management service")
        .beginMethod("getUserName", "string")
            .setMethodId(1)
            .addParameter("userId", "int32", ParameterDirection::IN)
            .setMethodTimeout(2000)
            .endMethod()
        .beginMethod("updateUser", "bool")
            .setMethodId(2)
            .addParameter("userId", "int32", ParameterDirection::IN)
            .addParameter("name", "string", ParameterDirection::IN)
            .setMethodTimeout(5000)
            .endMethod()
        .build();

    // Create mock handler
    auto handler = std::make_shared<MockInvocationHandler>();
    handler->setReturnValue("getUserName", std::any(std::string("John Doe")));
    handler->setReturnValue("updateUser", std::any(true));

    // Generate proxy
    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto proxy = generator->generateProxy(service, handler);

    // Test invocations
    std::string name = invokeTypedMethod<std::string>(proxy.get(), "getUserName", 123);
    EXPECT_EQ(name, "John Doe");

    bool updated = invokeTypedMethod<bool>(proxy.get(), "updateUser", 123, std::string("Jane Doe"));
    EXPECT_TRUE(updated);

    EXPECT_EQ(handler->getCallCount("getUserName"), 1);
    EXPECT_EQ(handler->getCallCount("updateUser"), 1);
}

// Performance test
TEST(ProxyGeneratorPerformanceTest, MultipleInvocations) {
    ServiceMetadataBuilder builder("PerfService", "1.0.0");
    auto service = builder
        .beginMethod("compute", "int32")
            .setMethodId(1)
            .addParameter("value", "int32", ParameterDirection::IN)
            .endMethod()
        .build();

    auto handler = std::make_shared<MockInvocationHandler>();
    handler->setReturnValue("compute", std::any(42));

    auto generator = std::make_shared<ReflectionProxyGenerator>();
    auto proxy = generator->generateProxy(service, handler);

    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        int result = invokeTypedMethod<int>(proxy.get(), "compute", i);
        ASSERT_EQ(result, 42);
    }

    EXPECT_EQ(handler->getCallCount("compute"), iterations);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
