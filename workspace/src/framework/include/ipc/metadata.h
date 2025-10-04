#ifndef IPC_METADATA_H
#define IPC_METADATA_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>
#include <any>
#include <functional>
#include <typeinfo>
#include <typeindex>

namespace ipc {
namespace metadata {

// Forward declarations
class TypeDescriptor;
class ParameterMetadata;
class MethodMetadata;
class ServiceMetadata;

/**
 * @brief Enumeration for parameter direction
 */
enum class ParameterDirection {
    IN,      // Input parameter
    OUT,     // Output parameter
    INOUT    // Input/Output parameter
};

/**
 * @brief Enumeration for method call semantics
 */
enum class MethodCallType {
    SYNCHRONOUS,   // Blocking call
    ASYNCHRONOUS,  // Non-blocking call
    ONEWAY         // Fire-and-forget
};

/**
 * @brief Type descriptor for runtime type information
 */
class TypeDescriptor {
public:
    TypeDescriptor(const std::string& name,
                   std::type_index typeIndex,
                   size_t size,
                   bool isPrimitive = false);

    const std::string& getName() const { return name_; }
    std::type_index getTypeIndex() const { return typeIndex_; }
    size_t getSize() const { return size_; }
    bool isPrimitive() const { return isPrimitive_; }
    bool isArray() const { return isArray_; }
    bool isPointer() const { return isPointer_; }

    void setArray(bool value) { isArray_ = value; }
    void setPointer(bool value) { isPointer_ = value; }
    void setElementType(std::shared_ptr<TypeDescriptor> elementType) {
        elementType_ = elementType;
    }

    std::shared_ptr<TypeDescriptor> getElementType() const {
        return elementType_;
    }

    // Serialization support
    std::string toJson() const;
    static std::shared_ptr<TypeDescriptor> fromJson(const std::string& json);

private:
    std::string name_;
    std::type_index typeIndex_;
    size_t size_;
    bool isPrimitive_;
    bool isArray_;
    bool isPointer_;
    std::shared_ptr<TypeDescriptor> elementType_;  // For arrays/pointers
};

/**
 * @brief Parameter metadata for method parameters
 */
class ParameterMetadata {
public:
    ParameterMetadata(const std::string& name,
                     std::shared_ptr<TypeDescriptor> type,
                     ParameterDirection direction = ParameterDirection::IN);

    const std::string& getName() const { return name_; }
    std::shared_ptr<TypeDescriptor> getType() const { return type_; }
    ParameterDirection getDirection() const { return direction_; }

    // Optional default value
    void setDefaultValue(const std::any& value) { defaultValue_ = value; }
    std::optional<std::any> getDefaultValue() const { return defaultValue_; }

    // Annotations
    void addAnnotation(const std::string& key, const std::string& value) {
        annotations_[key] = value;
    }
    std::optional<std::string> getAnnotation(const std::string& key) const;
    const std::map<std::string, std::string>& getAnnotations() const {
        return annotations_;
    }

    std::string toJson() const;
    static std::shared_ptr<ParameterMetadata> fromJson(const std::string& json);

private:
    std::string name_;
    std::shared_ptr<TypeDescriptor> type_;
    ParameterDirection direction_;
    std::optional<std::any> defaultValue_;
    std::map<std::string, std::string> annotations_;
};

/**
 * @brief Method metadata for service methods
 */
class MethodMetadata {
public:
    MethodMetadata(const std::string& name,
                   std::shared_ptr<TypeDescriptor> returnType);

    const std::string& getName() const { return name_; }
    std::shared_ptr<TypeDescriptor> getReturnType() const { return returnType_; }

    // Parameters
    void addParameter(std::shared_ptr<ParameterMetadata> param) {
        parameters_.push_back(param);
    }
    const std::vector<std::shared_ptr<ParameterMetadata>>& getParameters() const {
        return parameters_;
    }

    // Exceptions
    void addException(std::shared_ptr<TypeDescriptor> exceptionType) {
        exceptions_.push_back(exceptionType);
    }
    const std::vector<std::shared_ptr<TypeDescriptor>>& getExceptions() const {
        return exceptions_;
    }

    // Call type
    void setCallType(MethodCallType type) { callType_ = type; }
    MethodCallType getCallType() const { return callType_; }

    // Timeout
    void setTimeout(uint32_t timeoutMs) { timeoutMs_ = timeoutMs; }
    std::optional<uint32_t> getTimeout() const { return timeoutMs_; }

    // Annotations
    void addAnnotation(const std::string& key, const std::string& value) {
        annotations_[key] = value;
    }
    std::optional<std::string> getAnnotation(const std::string& key) const;
    const std::map<std::string, std::string>& getAnnotations() const {
        return annotations_;
    }

    // Method ID for RPC identification
    void setMethodId(uint32_t id) { methodId_ = id; }
    uint32_t getMethodId() const { return methodId_; }

    std::string toJson() const;
    static std::shared_ptr<MethodMetadata> fromJson(const std::string& json);

private:
    std::string name_;
    std::shared_ptr<TypeDescriptor> returnType_;
    std::vector<std::shared_ptr<ParameterMetadata>> parameters_;
    std::vector<std::shared_ptr<TypeDescriptor>> exceptions_;
    MethodCallType callType_;
    std::optional<uint32_t> timeoutMs_;
    std::map<std::string, std::string> annotations_;
    uint32_t methodId_;
};

/**
 * @brief Service metadata for complete service interface
 */
class ServiceMetadata {
public:
    ServiceMetadata(const std::string& name,
                   const std::string& version);

    const std::string& getName() const { return name_; }
    const std::string& getVersion() const { return version_; }

    void setNamespace(const std::string& ns) { namespace_ = ns; }
    const std::string& getNamespace() const { return namespace_; }

    void setDescription(const std::string& desc) { description_ = desc; }
    const std::string& getDescription() const { return description_; }

    // Methods
    void addMethod(std::shared_ptr<MethodMetadata> method) {
        methods_.push_back(method);
        methodMap_[method->getName()] = method;
    }
    const std::vector<std::shared_ptr<MethodMetadata>>& getMethods() const {
        return methods_;
    }
    std::shared_ptr<MethodMetadata> getMethod(const std::string& name) const;

    // Service-level annotations
    void addAnnotation(const std::string& key, const std::string& value) {
        annotations_[key] = value;
    }
    std::optional<std::string> getAnnotation(const std::string& key) const;
    const std::map<std::string, std::string>& getAnnotations() const {
        return annotations_;
    }

    // Service ID for service registry
    void setServiceId(uint32_t id) { serviceId_ = id; }
    uint32_t getServiceId() const { return serviceId_; }

    std::string toJson() const;
    static std::shared_ptr<ServiceMetadata> fromJson(const std::string& json);

    // Load from file
    static std::shared_ptr<ServiceMetadata> fromFile(const std::string& filename);
    bool saveToFile(const std::string& filename) const;

private:
    std::string name_;
    std::string version_;
    std::string namespace_;
    std::string description_;
    std::vector<std::shared_ptr<MethodMetadata>> methods_;
    std::map<std::string, std::shared_ptr<MethodMetadata>> methodMap_;
    std::map<std::string, std::string> annotations_;
    uint32_t serviceId_;
};

/**
 * @brief Type registry for managing type descriptors
 */
class TypeRegistry {
public:
    static TypeRegistry& instance();

    // Register a type
    void registerType(std::shared_ptr<TypeDescriptor> type);

    // Get type by name
    std::shared_ptr<TypeDescriptor> getType(const std::string& name) const;

    // Get type by type_index
    std::shared_ptr<TypeDescriptor> getType(std::type_index typeIndex) const;

    // Check if type is registered
    bool hasType(const std::string& name) const;
    bool hasType(std::type_index typeIndex) const;

    // Register built-in types
    void registerBuiltinTypes();

    // Template helper for registering types
    template<typename T>
    void registerType(const std::string& name, bool isPrimitive = false) {
        auto descriptor = std::make_shared<TypeDescriptor>(
            name,
            std::type_index(typeid(T)),
            sizeof(T),
            isPrimitive
        );
        registerType(descriptor);
    }

private:
    TypeRegistry();
    ~TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    std::map<std::string, std::shared_ptr<TypeDescriptor>> typesByName_;
    std::map<std::type_index, std::shared_ptr<TypeDescriptor>> typesByIndex_;
};

/**
 * @brief Helper macros for metadata annotations
 */
#define IPC_METHOD(name) addAnnotation("ipc.method", name)
#define IPC_TIMEOUT(ms) addAnnotation("ipc.timeout", std::to_string(ms))
#define IPC_ASYNC addAnnotation("ipc.async", "true")
#define IPC_ONEWAY addAnnotation("ipc.oneway", "true")
#define IPC_VERSION(ver) addAnnotation("ipc.version", ver)
#define IPC_DEPRECATED addAnnotation("ipc.deprecated", "true")

} // namespace metadata
} // namespace ipc

#endif // IPC_METADATA_H
