#ifndef IPC_PROXY_GENERATOR_H
#define IPC_PROXY_GENERATOR_H

#include "ipc/metadata.h"
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <any>
#include <future>

namespace ipc {
namespace proxy {

// Forward declarations
class ServiceProxy;
class ProxyInvocationHandler;
class RemoteException;

/**
 * @brief Exception thrown when remote method call fails
 */
class RemoteException : public std::runtime_error {
public:
    RemoteException(const std::string& message,
                   const std::string& remoteType = "",
                   int errorCode = 0)
        : std::runtime_error(message)
        , remoteType_(remoteType)
        , errorCode_(errorCode) {
    }

    const std::string& getRemoteType() const { return remoteType_; }
    int getErrorCode() const { return errorCode_; }

private:
    std::string remoteType_;
    int errorCode_;
};

/**
 * @brief Exception thrown when proxy call times out
 */
class TimeoutException : public RemoteException {
public:
    TimeoutException(const std::string& message, uint32_t timeoutMs)
        : RemoteException(message, "TimeoutException", -1)
        , timeoutMs_(timeoutMs) {
    }

    uint32_t getTimeoutMs() const { return timeoutMs_; }

private:
    uint32_t timeoutMs_;
};

/**
 * @brief Result of a method invocation
 */
struct InvocationResult {
    std::any returnValue;
    std::vector<std::any> outParameters;
    bool success;
    std::string errorMessage;
    std::string exceptionType;
    int errorCode;

    InvocationResult()
        : success(true)
        , errorCode(0) {
    }
};

/**
 * @brief Context for method invocation
 */
struct InvocationContext {
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata;
    std::shared_ptr<metadata::MethodMetadata> methodMetadata;
    std::vector<std::any> arguments;
    uint32_t timeoutMs;
    bool async;

    InvocationContext()
        : timeoutMs(5000)  // Default 5 second timeout
        , async(false) {
    }
};

/**
 * @brief Handler for proxy method invocations
 *
 * This interface abstracts the actual transport mechanism.
 * Implementations will handle serialization, network communication,
 * and deserialization.
 */
class ProxyInvocationHandler {
public:
    virtual ~ProxyInvocationHandler() = default;

    /**
     * @brief Invoke a method synchronously
     * @param context Invocation context with method and arguments
     * @return Result of the invocation
     */
    virtual InvocationResult invoke(const InvocationContext& context) = 0;

    /**
     * @brief Invoke a method asynchronously
     * @param context Invocation context with method and arguments
     * @return Future containing the result
     */
    virtual std::future<InvocationResult> invokeAsync(const InvocationContext& context) = 0;

    /**
     * @brief One-way method invocation (fire-and-forget)
     * @param context Invocation context with method and arguments
     */
    virtual void invokeOneway(const InvocationContext& context) = 0;
};

/**
 * @brief Base class for generated service proxies
 */
class ServiceProxy {
public:
    ServiceProxy(std::shared_ptr<metadata::ServiceMetadata> serviceMetadata,
                std::shared_ptr<ProxyInvocationHandler> handler)
        : serviceMetadata_(serviceMetadata)
        , handler_(handler) {
    }

    virtual ~ServiceProxy() = default;

    const std::shared_ptr<metadata::ServiceMetadata>& getServiceMetadata() const {
        return serviceMetadata_;
    }

    const std::shared_ptr<ProxyInvocationHandler>& getInvocationHandler() const {
        return handler_;
    }

protected:
    /**
     * @brief Helper to invoke a method
     */
    InvocationResult invokeMethod(const std::string& methodName,
                                 const std::vector<std::any>& arguments);

    /**
     * @brief Helper to invoke a method asynchronously
     */
    std::future<InvocationResult> invokeMethodAsync(const std::string& methodName,
                                                    const std::vector<std::any>& arguments);

    /**
     * @brief Helper to invoke a one-way method
     */
    void invokeMethodOneway(const std::string& methodName,
                           const std::vector<std::any>& arguments);

    // Friend declarations for helper functions
    template<typename ReturnType, typename... Args>
    friend ReturnType invokeTypedMethod(ServiceProxy* proxy,
                                       const std::string& methodName,
                                       Args&&... args);

private:
    std::shared_ptr<metadata::ServiceMetadata> serviceMetadata_;
    std::shared_ptr<ProxyInvocationHandler> handler_;
};

/**
 * @brief Abstract factory for generating service proxies
 */
class ProxyGenerator {
public:
    virtual ~ProxyGenerator() = default;

    /**
     * @brief Generate a proxy for a service
     * @param serviceMetadata Metadata describing the service interface
     * @param handler Handler for method invocations
     * @return Shared pointer to the generated proxy
     */
    virtual std::shared_ptr<ServiceProxy> generateProxy(
        std::shared_ptr<metadata::ServiceMetadata> serviceMetadata,
        std::shared_ptr<ProxyInvocationHandler> handler) = 0;

    /**
     * @brief Check if this generator supports the given service
     * @param serviceMetadata Metadata describing the service
     * @return true if supported, false otherwise
     */
    virtual bool supportsService(
        std::shared_ptr<metadata::ServiceMetadata> serviceMetadata) const = 0;

    /**
     * @brief Get the name of this generator
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Get the version of this generator
     */
    virtual std::string getVersion() const = 0;
};

/**
 * @brief Proxy generator registry
 */
class ProxyGeneratorRegistry {
public:
    static ProxyGeneratorRegistry& instance();

    void registerGenerator(std::shared_ptr<ProxyGenerator> generator);
    std::shared_ptr<ProxyGenerator> getGenerator(const std::string& name) const;
    std::vector<std::shared_ptr<ProxyGenerator>> getAllGenerators() const;

    std::shared_ptr<ProxyGenerator> findGeneratorForService(
        std::shared_ptr<metadata::ServiceMetadata> serviceMetadata) const;

private:
    ProxyGeneratorRegistry() = default;
    ~ProxyGeneratorRegistry() = default;
    ProxyGeneratorRegistry(const ProxyGeneratorRegistry&) = delete;
    ProxyGeneratorRegistry& operator=(const ProxyGeneratorRegistry&) = delete;

    std::map<std::string, std::shared_ptr<ProxyGenerator>> generators_;
};

/**
 * @brief Helper template for type-safe proxy method calls
 */
template<typename ReturnType, typename... Args>
ReturnType invokeTypedMethod(ServiceProxy* proxy,
                             const std::string& methodName,
                             Args&&... args) {
    std::vector<std::any> arguments;
    (arguments.push_back(std::any(std::forward<Args>(args))), ...);

    auto result = proxy->invokeMethod(methodName, arguments);

    if (!result.success) {
        throw RemoteException(result.errorMessage,
                            result.exceptionType,
                            result.errorCode);
    }

    if constexpr (!std::is_void_v<ReturnType>) {
        try {
            return std::any_cast<ReturnType>(result.returnValue);
        } catch (const std::bad_any_cast&) {
            throw RemoteException("Failed to cast return value to expected type");
        }
    }
}

/**
 * @brief Helper template for async proxy method calls
 */
template<typename ReturnType, typename... Args>
std::future<ReturnType> invokeTypedMethodAsync(ServiceProxy* proxy,
                                               const std::string& methodName,
                                               Args&&... args) {
    std::vector<std::any> arguments;
    (arguments.push_back(std::any(std::forward<Args>(args))), ...);

    auto futureResult = proxy->invokeMethodAsync(methodName, arguments);

    return std::async(std::launch::deferred, [futureResult = std::move(futureResult)]() mutable {
        auto result = futureResult.get();

        if (!result.success) {
            throw RemoteException(result.errorMessage,
                                result.exceptionType,
                                result.errorCode);
        }

        if constexpr (!std::is_void_v<ReturnType>) {
            try {
                return std::any_cast<ReturnType>(result.returnValue);
            } catch (const std::bad_any_cast&) {
                throw RemoteException("Failed to cast return value to expected type");
            }
        }
    });
}

} // namespace proxy
} // namespace ipc

#endif // IPC_PROXY_GENERATOR_H
