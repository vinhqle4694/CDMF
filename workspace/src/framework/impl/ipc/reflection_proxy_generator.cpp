#include "ipc/reflection_proxy_generator.h"
#include "utils/log.h"
#include <stdexcept>
#include <sstream>
#include <chrono>

namespace ipc {
namespace proxy {

// ServiceProxy implementation
InvocationResult ServiceProxy::invokeMethod(const std::string& methodName,
                                           const std::vector<std::any>& arguments) {
    LOGD_FMT("ServiceProxy::invokeMethod - method: " << methodName << ", args: " << arguments.size());

    auto method = serviceMetadata_->getMethod(methodName);
    if (!method) {
        LOGE_FMT("ServiceProxy::invokeMethod - method not found: " << methodName);
        InvocationResult result;
        result.success = false;
        result.errorMessage = "Method not found: " + methodName;
        result.errorCode = -1;
        return result;
    }

    InvocationContext context;
    context.serviceMetadata = serviceMetadata_;
    context.methodMetadata = method;
    context.arguments = arguments;
    context.async = false;

    if (method->getTimeout()) {
        context.timeoutMs = *method->getTimeout();
    }

    LOGD_FMT("ServiceProxy::invokeMethod - invoking handler");
    return handler_->invoke(context);
}

std::future<InvocationResult> ServiceProxy::invokeMethodAsync(
    const std::string& methodName,
    const std::vector<std::any>& arguments) {

    LOGD_FMT("ServiceProxy::invokeMethodAsync - method: " << methodName << ", args: " << arguments.size());

    auto method = serviceMetadata_->getMethod(methodName);
    if (!method) {
        LOGE_FMT("ServiceProxy::invokeMethodAsync - method not found: " << methodName);
        std::promise<InvocationResult> promise;
        InvocationResult result;
        result.success = false;
        result.errorMessage = "Method not found: " + methodName;
        result.errorCode = -1;
        promise.set_value(result);
        return promise.get_future();
    }

    InvocationContext context;
    context.serviceMetadata = serviceMetadata_;
    context.methodMetadata = method;
    context.arguments = arguments;
    context.async = true;

    if (method->getTimeout()) {
        context.timeoutMs = *method->getTimeout();
    }

    LOGD_FMT("ServiceProxy::invokeMethodAsync - invoking async handler");
    return handler_->invokeAsync(context);
}

void ServiceProxy::invokeMethodOneway(const std::string& methodName,
                                     const std::vector<std::any>& arguments) {
    LOGD_FMT("ServiceProxy::invokeMethodOneway - method: " << methodName);

    auto method = serviceMetadata_->getMethod(methodName);
    if (!method) {
        LOGE_FMT("ServiceProxy::invokeMethodOneway - method not found: " << methodName);
        throw RemoteException("Method not found: " + methodName);
    }

    InvocationContext context;
    context.serviceMetadata = serviceMetadata_;
    context.methodMetadata = method;
    context.arguments = arguments;
    context.async = false;

    LOGD_FMT("ServiceProxy::invokeMethodOneway - invoking oneway handler");
    handler_->invokeOneway(context);
}

// ReflectionServiceProxy implementation
ReflectionServiceProxy::ReflectionServiceProxy(
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata,
    std::shared_ptr<ProxyInvocationHandler> handler)
    : ServiceProxy(serviceMetadata, handler) {
    LOGD_FMT("ReflectionServiceProxy - created for service: " << serviceMetadata->getName());
}

InvocationResult ReflectionServiceProxy::invoke(
    const std::string& methodName,
    const std::vector<std::any>& arguments) {

    LOGD_FMT("ReflectionServiceProxy::invoke - method: " << methodName);

    auto method = getMethodMetadata(methodName);
    if (!method) {
        LOGE_FMT("ReflectionServiceProxy::invoke - method not found: " << methodName);
        InvocationResult result;
        result.success = false;
        result.errorMessage = "Method not found: " + methodName;
        result.errorCode = -1;
        return result;
    }

    validateArguments(method, arguments);
    LOGD_FMT("ReflectionServiceProxy::invoke - validation passed, invoking");
    return invokeMethod(methodName, arguments);
}

std::future<InvocationResult> ReflectionServiceProxy::invokeAsync(
    const std::string& methodName,
    const std::vector<std::any>& arguments) {

    auto method = getMethodMetadata(methodName);
    if (!method) {
        std::promise<InvocationResult> promise;
        InvocationResult result;
        result.success = false;
        result.errorMessage = "Method not found: " + methodName;
        result.errorCode = -1;
        promise.set_value(result);
        return promise.get_future();
    }

    validateArguments(method, arguments);
    return invokeMethodAsync(methodName, arguments);
}

void ReflectionServiceProxy::invokeOneway(
    const std::string& methodName,
    const std::vector<std::any>& arguments) {

    auto method = getMethodMetadata(methodName);
    if (!method) {
        throw RemoteException("Method not found: " + methodName);
    }

    validateArguments(method, arguments);
    invokeMethodOneway(methodName, arguments);
}

bool ReflectionServiceProxy::hasMethod(const std::string& methodName) const {
    return getServiceMetadata()->getMethod(methodName) != nullptr;
}

std::shared_ptr<metadata::MethodMetadata> ReflectionServiceProxy::getMethodMetadata(
    const std::string& methodName) const {
    return getServiceMetadata()->getMethod(methodName);
}

void ReflectionServiceProxy::validateArguments(
    std::shared_ptr<metadata::MethodMetadata> method,
    const std::vector<std::any>& arguments) {

    LOGD_FMT("ReflectionServiceProxy::validateArguments - method: " << method->getName());

    const auto& params = method->getParameters();

    // Count input parameters
    size_t inputParamCount = 0;
    for (const auto& param : params) {
        if (param->getDirection() == metadata::ParameterDirection::IN ||
            param->getDirection() == metadata::ParameterDirection::INOUT) {
            inputParamCount++;
        }
    }

    LOGD_FMT("ReflectionServiceProxy::validateArguments - expected: " << inputParamCount
             << ", got: " << arguments.size());

    if (arguments.size() != inputParamCount) {
        std::ostringstream oss;
        oss << "Argument count mismatch for method " << method->getName()
            << ": expected " << inputParamCount
            << ", got " << arguments.size();
        LOGE_FMT("ReflectionServiceProxy::validateArguments - " << oss.str());
        throw std::invalid_argument(oss.str());
    }

    LOGD_FMT("ReflectionServiceProxy::validateArguments - validation passed");
}

uint32_t ReflectionServiceProxy::getMethodTimeout(
    std::shared_ptr<metadata::MethodMetadata> method) {

    if (method->getTimeout()) {
        return *method->getTimeout();
    }
    return 5000; // Default 5 seconds
}

// ReflectionProxyGenerator implementation
ReflectionProxyGenerator::ReflectionProxyGenerator()
    : validateArguments_(true)
    , defaultTimeoutMs_(5000) {
    LOGD_FMT("ReflectionProxyGenerator - created with default timeout: " << defaultTimeoutMs_ << "ms");
}

std::shared_ptr<ServiceProxy> ReflectionProxyGenerator::generateProxy(
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata,
    std::shared_ptr<ProxyInvocationHandler> handler) {

    LOGD_FMT("ReflectionProxyGenerator::generateProxy - generating proxy for: "
             << (serviceMetadata ? serviceMetadata->getName() : "null"));

    if (!serviceMetadata) {
        LOGE_FMT("ReflectionProxyGenerator::generateProxy - null service metadata");
        throw std::invalid_argument("Service metadata cannot be null");
    }

    if (!handler) {
        LOGE_FMT("ReflectionProxyGenerator::generateProxy - null handler");
        throw std::invalid_argument("Invocation handler cannot be null");
    }

    LOGD_FMT("ReflectionProxyGenerator::generateProxy - proxy created successfully");
    return std::make_shared<ReflectionServiceProxy>(serviceMetadata, handler);
}

bool ReflectionProxyGenerator::supportsService(
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata) const {

    // Reflection generator supports any service with valid metadata
    return serviceMetadata != nullptr && !serviceMetadata->getMethods().empty();
}

// ProxyGeneratorRegistry implementation
ProxyGeneratorRegistry& ProxyGeneratorRegistry::instance() {
    static ProxyGeneratorRegistry registry;
    return registry;
}

void ProxyGeneratorRegistry::registerGenerator(std::shared_ptr<ProxyGenerator> generator) {
    if (!generator) {
        throw std::invalid_argument("Generator cannot be null");
    }
    generators_[generator->getName()] = generator;
}

std::shared_ptr<ProxyGenerator> ProxyGeneratorRegistry::getGenerator(
    const std::string& name) const {

    auto it = generators_.find(name);
    if (it != generators_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<ProxyGenerator>> ProxyGeneratorRegistry::getAllGenerators() const {
    std::vector<std::shared_ptr<ProxyGenerator>> result;
    result.reserve(generators_.size());
    for (const auto& [name, generator] : generators_) {
        result.push_back(generator);
    }
    return result;
}

std::shared_ptr<ProxyGenerator> ProxyGeneratorRegistry::findGeneratorForService(
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata) const {

    for (const auto& [name, generator] : generators_) {
        if (generator->supportsService(serviceMetadata)) {
            return generator;
        }
    }
    return nullptr;
}

// MockInvocationHandler implementation
InvocationResult MockInvocationHandler::invoke(const InvocationContext& context) {
    std::lock_guard<std::mutex> lock(mutex_);

    LOGD_FMT("MockInvocationHandler::invoke - invoked");

    if (!context.methodMetadata) {
        LOGW_FMT("MockInvocationHandler::invoke - no method metadata");
        InvocationResult result;
        result.success = false;
        result.errorMessage = "No method metadata in context";
        return result;
    }

    const std::string& methodName = context.methodMetadata->getName();
    LOGD_FMT("MockInvocationHandler::invoke - method: " << methodName);

    auto& mock = getOrCreateMock(methodName);
    mock.callCount++;
    mock.lastContext = context;

    LOGD_FMT("MockInvocationHandler::invoke - call count: " << mock.callCount);

    // Custom handler takes precedence
    if (mock.customHandler) {
        LOGD_FMT("MockInvocationHandler::invoke - using custom handler");
        return mock.customHandler(context);
    }

    InvocationResult result;

    // Check if exception should be thrown
    if (!mock.exceptionType.empty()) {
        LOGD_FMT("MockInvocationHandler::invoke - throwing configured exception: " << mock.exceptionType);
        result.success = false;
        result.exceptionType = mock.exceptionType;
        result.errorMessage = mock.exceptionMessage;
        result.errorCode = -1;
        return result;
    }

    // Return configured value
    LOGD_FMT("MockInvocationHandler::invoke - returning configured value");
    result.success = true;
    result.returnValue = mock.returnValue;
    return result;
}

std::future<InvocationResult> MockInvocationHandler::invokeAsync(
    const InvocationContext& context) {

    std::promise<InvocationResult> promise;
    promise.set_value(invoke(context));
    return promise.get_future();
}

void MockInvocationHandler::invokeOneway(const InvocationContext& context) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!context.methodMetadata) {
        return;
    }

    const std::string& methodName = context.methodMetadata->getName();
    auto& mock = getOrCreateMock(methodName);
    mock.callCount++;
    mock.lastContext = context;
}

void MockInvocationHandler::setReturnValue(const std::string& methodName,
                                          const std::any& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& mock = getOrCreateMock(methodName);
    mock.returnValue = value;
    mock.exceptionType.clear();
}

void MockInvocationHandler::setException(const std::string& methodName,
                                        const std::string& exceptionType,
                                        const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& mock = getOrCreateMock(methodName);
    mock.exceptionType = exceptionType;
    mock.exceptionMessage = message;
}

void MockInvocationHandler::setMethodHandler(const std::string& methodName,
                                            MethodHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& mock = getOrCreateMock(methodName);
    mock.customHandler = handler;
}

int MockInvocationHandler::getCallCount(const std::string& methodName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = methodMocks_.find(methodName);
    if (it != methodMocks_.end()) {
        return it->second.callCount;
    }
    return 0;
}

std::optional<InvocationContext> MockInvocationHandler::getLastInvocation(
    const std::string& methodName) const {

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = methodMocks_.find(methodName);
    if (it != methodMocks_.end() && it->second.callCount > 0) {
        return it->second.lastContext;
    }
    return std::nullopt;
}

void MockInvocationHandler::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    methodMocks_.clear();
}

MockInvocationHandler::MethodMock& MockInvocationHandler::getOrCreateMock(
    const std::string& methodName) {
    return methodMocks_[methodName];
}

// ServiceMetadataBuilder implementation
ServiceMetadataBuilder::ServiceMetadataBuilder(const std::string& serviceName,
                                             const std::string& version)
    : serviceMetadata_(std::make_shared<metadata::ServiceMetadata>(serviceName, version))
    , currentMethod_(nullptr) {
}

ServiceMetadataBuilder& ServiceMetadataBuilder::setNamespace(const std::string& ns) {
    serviceMetadata_->setNamespace(ns);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::setDescription(const std::string& desc) {
    serviceMetadata_->setDescription(desc);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::setServiceId(uint32_t id) {
    serviceMetadata_->setServiceId(id);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::addAnnotation(
    const std::string& key,
    const std::string& value) {
    serviceMetadata_->addAnnotation(key, value);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::beginMethod(
    const std::string& methodName,
    const std::string& returnType) {

    if (currentMethod_) {
        throw std::logic_error("Previous method not ended with endMethod()");
    }

    auto returnTypeDesc = getOrCreateType(returnType);
    currentMethod_ = std::make_shared<metadata::MethodMetadata>(methodName, returnTypeDesc);

    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::addParameter(
    const std::string& name,
    const std::string& type,
    metadata::ParameterDirection direction) {

    if (!currentMethod_) {
        throw std::logic_error("No method started. Call beginMethod() first.");
    }

    auto typeDesc = getOrCreateType(type);
    auto param = std::make_shared<metadata::ParameterMetadata>(name, typeDesc, direction);
    currentMethod_->addParameter(param);

    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::setMethodTimeout(uint32_t timeoutMs) {
    if (!currentMethod_) {
        throw std::logic_error("No method started. Call beginMethod() first.");
    }
    currentMethod_->setTimeout(timeoutMs);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::setMethodCallType(
    metadata::MethodCallType callType) {

    if (!currentMethod_) {
        throw std::logic_error("No method started. Call beginMethod() first.");
    }
    currentMethod_->setCallType(callType);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::setMethodId(uint32_t id) {
    if (!currentMethod_) {
        throw std::logic_error("No method started. Call beginMethod() first.");
    }
    currentMethod_->setMethodId(id);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::addMethodAnnotation(
    const std::string& key,
    const std::string& value) {

    if (!currentMethod_) {
        throw std::logic_error("No method started. Call beginMethod() first.");
    }
    currentMethod_->addAnnotation(key, value);
    return *this;
}

ServiceMetadataBuilder& ServiceMetadataBuilder::endMethod() {
    if (!currentMethod_) {
        throw std::logic_error("No method started. Call beginMethod() first.");
    }

    serviceMetadata_->addMethod(currentMethod_);
    currentMethod_ = nullptr;
    return *this;
}

std::shared_ptr<metadata::ServiceMetadata> ServiceMetadataBuilder::build() {
    if (currentMethod_) {
        throw std::logic_error("Method not ended. Call endMethod() first.");
    }
    return serviceMetadata_;
}

std::shared_ptr<metadata::TypeDescriptor> ServiceMetadataBuilder::getOrCreateType(
    const std::string& typeName) {

    auto& registry = metadata::TypeRegistry::instance();

    auto type = registry.getType(typeName);
    if (type) {
        return type;
    }

    // Create a custom type descriptor
    // In production, this should handle complex types properly
    auto newType = std::make_shared<metadata::TypeDescriptor>(
        typeName,
        std::type_index(typeid(void)),  // Unknown type
        0,
        false
    );
    registry.registerType(newType);

    return newType;
}

} // namespace proxy
} // namespace ipc
