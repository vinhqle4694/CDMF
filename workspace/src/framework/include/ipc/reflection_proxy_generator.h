#ifndef IPC_REFLECTION_PROXY_GENERATOR_H
#define IPC_REFLECTION_PROXY_GENERATOR_H

#include "ipc/proxy_generator.h"
#include "ipc/metadata.h"
#include <memory>
#include <string>
#include <map>
#include <functional>

namespace ipc {
namespace proxy {

/**
 * @brief Runtime proxy implementation using reflection
 *
 * This class generates proxies at runtime using service metadata.
 * All method calls are dispatched through the invocation handler.
 */
class ReflectionServiceProxy : public ServiceProxy {
public:
    ReflectionServiceProxy(std::shared_ptr<metadata::ServiceMetadata> serviceMetadata,
                          std::shared_ptr<ProxyInvocationHandler> handler);

    ~ReflectionServiceProxy() override = default;

    /**
     * @brief Invoke a method by name
     * @param methodName Name of the method to invoke
     * @param arguments Method arguments
     * @return Invocation result
     */
    InvocationResult invoke(const std::string& methodName,
                           const std::vector<std::any>& arguments);

    /**
     * @brief Invoke a method asynchronously by name
     * @param methodName Name of the method to invoke
     * @param arguments Method arguments
     * @return Future containing invocation result
     */
    std::future<InvocationResult> invokeAsync(const std::string& methodName,
                                             const std::vector<std::any>& arguments);

    /**
     * @brief Invoke a one-way method by name
     * @param methodName Name of the method to invoke
     * @param arguments Method arguments
     */
    void invokeOneway(const std::string& methodName,
                     const std::vector<std::any>& arguments);

    /**
     * @brief Check if a method exists
     */
    bool hasMethod(const std::string& methodName) const;

    /**
     * @brief Get method metadata
     */
    std::shared_ptr<metadata::MethodMetadata> getMethodMetadata(
        const std::string& methodName) const;

private:
    void validateArguments(std::shared_ptr<metadata::MethodMetadata> method,
                          const std::vector<std::any>& arguments);

    uint32_t getMethodTimeout(std::shared_ptr<metadata::MethodMetadata> method);
};

/**
 * @brief Reflection-based proxy generator
 *
 * Generates service proxies at runtime using metadata.
 * No code generation required - everything is handled through reflection.
 */
class ReflectionProxyGenerator : public ProxyGenerator {
public:
    ReflectionProxyGenerator();
    ~ReflectionProxyGenerator() override = default;

    std::shared_ptr<ServiceProxy> generateProxy(
        std::shared_ptr<metadata::ServiceMetadata> serviceMetadata,
        std::shared_ptr<ProxyInvocationHandler> handler) override;

    bool supportsService(
        std::shared_ptr<metadata::ServiceMetadata> serviceMetadata) const override;

    std::string getName() const override {
        return "ReflectionProxyGenerator";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    /**
     * @brief Set whether to validate arguments at invocation time
     */
    void setValidateArguments(bool validate) {
        validateArguments_ = validate;
    }

    bool getValidateArguments() const {
        return validateArguments_;
    }

    /**
     * @brief Set default timeout for methods without explicit timeout
     */
    void setDefaultTimeout(uint32_t timeoutMs) {
        defaultTimeoutMs_ = timeoutMs;
    }

    uint32_t getDefaultTimeout() const {
        return defaultTimeoutMs_;
    }

private:
    bool validateArguments_;
    uint32_t defaultTimeoutMs_;
};

/**
 * @brief Mock invocation handler for testing
 *
 * Allows setting up expected calls and return values for testing
 * proxy functionality without actual network communication.
 */
class MockInvocationHandler : public ProxyInvocationHandler {
public:
    MockInvocationHandler() = default;
    ~MockInvocationHandler() override = default;

    InvocationResult invoke(const InvocationContext& context) override;
    std::future<InvocationResult> invokeAsync(const InvocationContext& context) override;
    void invokeOneway(const InvocationContext& context) override;

    /**
     * @brief Set expected return value for a method
     */
    void setReturnValue(const std::string& methodName, const std::any& value);

    /**
     * @brief Set exception to throw for a method
     */
    void setException(const std::string& methodName,
                     const std::string& exceptionType,
                     const std::string& message);

    /**
     * @brief Set custom handler for a method
     */
    using MethodHandler = std::function<InvocationResult(const InvocationContext&)>;
    void setMethodHandler(const std::string& methodName, MethodHandler handler);

    /**
     * @brief Get number of times a method was called
     */
    int getCallCount(const std::string& methodName) const;

    /**
     * @brief Get last invocation context for a method
     */
    std::optional<InvocationContext> getLastInvocation(const std::string& methodName) const;

    /**
     * @brief Clear all mock state
     */
    void reset();

private:
    struct MethodMock {
        std::any returnValue;
        std::string exceptionType;
        std::string exceptionMessage;
        MethodHandler customHandler;
        int callCount = 0;
        InvocationContext lastContext;
    };

    std::map<std::string, MethodMock> methodMocks_;
    mutable std::mutex mutex_;

    MethodMock& getOrCreateMock(const std::string& methodName);
};

/**
 * @brief Helper for building service metadata programmatically
 */
class ServiceMetadataBuilder {
public:
    ServiceMetadataBuilder(const std::string& serviceName, const std::string& version);

    ServiceMetadataBuilder& setNamespace(const std::string& ns);
    ServiceMetadataBuilder& setDescription(const std::string& desc);
    ServiceMetadataBuilder& setServiceId(uint32_t id);
    ServiceMetadataBuilder& addAnnotation(const std::string& key, const std::string& value);

    /**
     * @brief Begin adding a method
     */
    ServiceMetadataBuilder& beginMethod(const std::string& methodName,
                                       const std::string& returnType);

    /**
     * @brief Add parameter to current method
     */
    ServiceMetadataBuilder& addParameter(const std::string& name,
                                        const std::string& type,
                                        metadata::ParameterDirection direction = metadata::ParameterDirection::IN);

    /**
     * @brief Set timeout for current method
     */
    ServiceMetadataBuilder& setMethodTimeout(uint32_t timeoutMs);

    /**
     * @brief Set call type for current method
     */
    ServiceMetadataBuilder& setMethodCallType(metadata::MethodCallType callType);

    /**
     * @brief Set method ID for current method
     */
    ServiceMetadataBuilder& setMethodId(uint32_t id);

    /**
     * @brief Add annotation to current method
     */
    ServiceMetadataBuilder& addMethodAnnotation(const std::string& key, const std::string& value);

    /**
     * @brief Finish adding current method
     */
    ServiceMetadataBuilder& endMethod();

    /**
     * @brief Build the service metadata
     */
    std::shared_ptr<metadata::ServiceMetadata> build();

private:
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata_;
    std::shared_ptr<metadata::MethodMetadata> currentMethod_;

    std::shared_ptr<metadata::TypeDescriptor> getOrCreateType(const std::string& typeName);
};

} // namespace proxy
} // namespace ipc

#endif // IPC_REFLECTION_PROXY_GENERATOR_H
