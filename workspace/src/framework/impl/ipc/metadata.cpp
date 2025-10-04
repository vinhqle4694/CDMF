#include "ipc/metadata.h"
#include "utils/log.h"
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <iomanip>

namespace ipc {
namespace metadata {

// Simple JSON helper functions (minimal implementation)
namespace json_helper {
    std::string escape(const std::string& str) {
        LOGD_FMT("json_helper::escape - input length: " << str.length());
        std::ostringstream oss;
        for (char c : str) {
            switch (c) {
                case '"':  oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:   oss << c; break;
            }
        }
        LOGD_FMT("json_helper::escape - output length: " << oss.str().length());
        return oss.str();
    }

    std::string quote(const std::string& str) {
        return "\"" + escape(str) + "\"";
    }
}

// TypeDescriptor implementation
TypeDescriptor::TypeDescriptor(const std::string& name,
                               std::type_index typeIndex,
                               size_t size,
                               bool isPrimitive)
    : name_(name)
    , typeIndex_(typeIndex)
    , size_(size)
    , isPrimitive_(isPrimitive)
    , isArray_(false)
    , isPointer_(false)
    , elementType_(nullptr) {
    LOGD_FMT("TypeDescriptor - created: name=" << name << ", size=" << size
             << ", isPrimitive=" << (isPrimitive ? "true" : "false"));
}

std::string TypeDescriptor::toJson() const {
    LOGD_FMT("TypeDescriptor::toJson - serializing: " << name_);
    std::ostringstream oss;
    oss << "{";
    oss << "\"name\":" << json_helper::quote(name_) << ",";
    oss << "\"size\":" << size_ << ",";
    oss << "\"isPrimitive\":" << (isPrimitive_ ? "true" : "false") << ",";
    oss << "\"isArray\":" << (isArray_ ? "true" : "false") << ",";
    oss << "\"isPointer\":" << (isPointer_ ? "true" : "false");

    if (elementType_) {
        oss << ",\"elementType\":" << elementType_->toJson();
    }

    oss << "}";
    LOGD_FMT("TypeDescriptor::toJson - serialization complete");
    return oss.str();
}

std::shared_ptr<TypeDescriptor> TypeDescriptor::fromJson(const std::string& /* json */) {
    LOGW_FMT("TypeDescriptor::fromJson - not implemented");
    // Simplified JSON parsing - in production, use a proper JSON library
    // This is a minimal implementation for demonstration
    throw std::runtime_error("TypeDescriptor::fromJson not yet implemented - use JSON library");
}

// ParameterMetadata implementation
ParameterMetadata::ParameterMetadata(const std::string& name,
                                   std::shared_ptr<TypeDescriptor> type,
                                   ParameterDirection direction)
    : name_(name)
    , type_(type)
    , direction_(direction) {
    LOGD_FMT("ParameterMetadata - created: name=" << name << ", direction=" << static_cast<int>(direction));
}

std::optional<std::string> ParameterMetadata::getAnnotation(const std::string& key) const {
    LOGD_FMT("ParameterMetadata::getAnnotation - key: " << key);
    auto it = annotations_.find(key);
    if (it != annotations_.end()) {
        LOGD_FMT("ParameterMetadata::getAnnotation - found: " << it->second);
        return it->second;
    }
    LOGD_FMT("ParameterMetadata::getAnnotation - not found");
    return std::nullopt;
}

std::string ParameterMetadata::toJson() const {
    LOGD_FMT("ParameterMetadata::toJson - serializing: " << name_);
    std::ostringstream oss;
    oss << "{";
    oss << "\"name\":" << json_helper::quote(name_) << ",";
    oss << "\"type\":" << type_->toJson() << ",";

    std::string dirStr;
    switch (direction_) {
        case ParameterDirection::IN: dirStr = "IN"; break;
        case ParameterDirection::OUT: dirStr = "OUT"; break;
        case ParameterDirection::INOUT: dirStr = "INOUT"; break;
    }
    oss << "\"direction\":" << json_helper::quote(dirStr);

    if (!annotations_.empty()) {
        oss << ",\"annotations\":{";
        bool first = true;
        for (const auto& [key, value] : annotations_) {
            if (!first) oss << ",";
            oss << json_helper::quote(key) << ":" << json_helper::quote(value);
            first = false;
        }
        oss << "}";
    }

    oss << "}";
    LOGD_FMT("ParameterMetadata::toJson - complete");
    return oss.str();
}

std::shared_ptr<ParameterMetadata> ParameterMetadata::fromJson(const std::string& /* json */) {
    throw std::runtime_error("ParameterMetadata::fromJson not yet implemented - use JSON library");
}

// MethodMetadata implementation
MethodMetadata::MethodMetadata(const std::string& name,
                               std::shared_ptr<TypeDescriptor> returnType)
    : name_(name)
    , returnType_(returnType)
    , callType_(MethodCallType::SYNCHRONOUS)
    , methodId_(0) {
    LOGD_FMT("MethodMetadata - created: " << name);
}

std::optional<std::string> MethodMetadata::getAnnotation(const std::string& key) const {
    auto it = annotations_.find(key);
    if (it != annotations_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string MethodMetadata::toJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"name\":" << json_helper::quote(name_) << ",";
    oss << "\"methodId\":" << methodId_ << ",";
    oss << "\"returnType\":" << returnType_->toJson() << ",";

    std::string callTypeStr;
    switch (callType_) {
        case MethodCallType::SYNCHRONOUS: callTypeStr = "SYNCHRONOUS"; break;
        case MethodCallType::ASYNCHRONOUS: callTypeStr = "ASYNCHRONOUS"; break;
        case MethodCallType::ONEWAY: callTypeStr = "ONEWAY"; break;
    }
    oss << "\"callType\":" << json_helper::quote(callTypeStr) << ",";

    // Parameters
    oss << "\"parameters\":[";
    bool first = true;
    for (const auto& param : parameters_) {
        if (!first) oss << ",";
        oss << param->toJson();
        first = false;
    }
    oss << "]";

    // Exceptions
    if (!exceptions_.empty()) {
        oss << ",\"exceptions\":[";
        first = true;
        for (const auto& ex : exceptions_) {
            if (!first) oss << ",";
            oss << ex->toJson();
            first = false;
        }
        oss << "]";
    }

    // Timeout
    if (timeoutMs_) {
        oss << ",\"timeout\":" << *timeoutMs_;
    }

    // Annotations
    if (!annotations_.empty()) {
        oss << ",\"annotations\":{";
        first = true;
        for (const auto& [key, value] : annotations_) {
            if (!first) oss << ",";
            oss << json_helper::quote(key) << ":" << json_helper::quote(value);
            first = false;
        }
        oss << "}";
    }

    oss << "}";
    return oss.str();
}

std::shared_ptr<MethodMetadata> MethodMetadata::fromJson(const std::string& /* json */) {
    throw std::runtime_error("MethodMetadata::fromJson not yet implemented - use JSON library");
}

// ServiceMetadata implementation
ServiceMetadata::ServiceMetadata(const std::string& name,
                                const std::string& version)
    : name_(name)
    , version_(version)
    , serviceId_(0) {
    LOGD_FMT("ServiceMetadata - created: " << name << " v" << version);
}

std::shared_ptr<MethodMetadata> ServiceMetadata::getMethod(const std::string& name) const {
    LOGD_FMT("ServiceMetadata::getMethod - searching for: " << name);
    auto it = methodMap_.find(name);
    if (it != methodMap_.end()) {
        LOGD_FMT("ServiceMetadata::getMethod - found method");
        return it->second;
    }
    LOGW_FMT("ServiceMetadata::getMethod - method not found: " << name);
    return nullptr;
}

std::optional<std::string> ServiceMetadata::getAnnotation(const std::string& key) const {
    auto it = annotations_.find(key);
    if (it != annotations_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string ServiceMetadata::toJson() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"name\":" << json_helper::quote(name_) << ",\n";
    oss << "  \"version\":" << json_helper::quote(version_) << ",\n";
    oss << "  \"serviceId\":" << serviceId_ << ",\n";

    if (!namespace_.empty()) {
        oss << "  \"namespace\":" << json_helper::quote(namespace_) << ",\n";
    }

    if (!description_.empty()) {
        oss << "  \"description\":" << json_helper::quote(description_) << ",\n";
    }

    // Methods
    oss << "  \"methods\":[\n";
    bool first = true;
    for (const auto& method : methods_) {
        if (!first) oss << ",\n";
        std::string methodJson = method->toJson();
        // Indent method JSON
        std::istringstream iss(methodJson);
        std::string line;
        while (std::getline(iss, line)) {
            oss << "    " << line;
            if (!iss.eof()) oss << "\n";
        }
        first = false;
    }
    oss << "\n  ]";

    // Annotations
    if (!annotations_.empty()) {
        oss << ",\n  \"annotations\":{\n";
        first = true;
        for (const auto& [key, value] : annotations_) {
            if (!first) oss << ",\n";
            oss << "    " << json_helper::quote(key) << ":" << json_helper::quote(value);
            first = false;
        }
        oss << "\n  }";
    }

    oss << "\n}";
    return oss.str();
}

std::shared_ptr<ServiceMetadata> ServiceMetadata::fromJson(const std::string& /* json */) {
    throw std::runtime_error("ServiceMetadata::fromJson not yet implemented - use JSON library");
}

std::shared_ptr<ServiceMetadata> ServiceMetadata::fromFile(const std::string& filename) {
    LOGD_FMT("ServiceMetadata::fromFile - loading from: " << filename);
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOGE_FMT("ServiceMetadata::fromFile - failed to open: " << filename);
        throw std::runtime_error("Failed to open file: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    LOGD_FMT("ServiceMetadata::fromFile - file loaded, parsing JSON");
    return fromJson(buffer.str());
}

bool ServiceMetadata::saveToFile(const std::string& filename) const {
    LOGD_FMT("ServiceMetadata::saveToFile - saving to: " << filename);
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOGE_FMT("ServiceMetadata::saveToFile - failed to open: " << filename);
        return false;
    }

    file << toJson();
    bool success = file.good();
    LOGD_FMT("ServiceMetadata::saveToFile - " << (success ? "success" : "failed"));
    return success;
}

// TypeRegistry implementation
TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry registry;
    return registry;
}

TypeRegistry::TypeRegistry() {
    LOGD_FMT("TypeRegistry - initializing and registering builtin types");
    registerBuiltinTypes();
}

void TypeRegistry::registerType(std::shared_ptr<TypeDescriptor> type) {
    LOGD_FMT("TypeRegistry::registerType - registering: " << type->getName());
    typesByName_[type->getName()] = type;
    typesByIndex_[type->getTypeIndex()] = type;
}

std::shared_ptr<TypeDescriptor> TypeRegistry::getType(const std::string& name) const {
    LOGD_FMT("TypeRegistry::getType - looking up: " << name);
    auto it = typesByName_.find(name);
    if (it != typesByName_.end()) {
        LOGD_FMT("TypeRegistry::getType - found");
        return it->second;
    }
    LOGD_FMT("TypeRegistry::getType - not found");
    return nullptr;
}

std::shared_ptr<TypeDescriptor> TypeRegistry::getType(std::type_index typeIndex) const {
    auto it = typesByIndex_.find(typeIndex);
    if (it != typesByIndex_.end()) {
        return it->second;
    }
    return nullptr;
}

bool TypeRegistry::hasType(const std::string& name) const {
    return typesByName_.find(name) != typesByName_.end();
}

bool TypeRegistry::hasType(std::type_index typeIndex) const {
    return typesByIndex_.find(typeIndex) != typesByIndex_.end();
}

void TypeRegistry::registerBuiltinTypes() {
    // Primitive types
    registerType<bool>("bool", true);
    registerType<char>("char", true);
    registerType<int8_t>("int8", true);
    registerType<uint8_t>("uint8", true);
    registerType<int16_t>("int16", true);
    registerType<uint16_t>("uint16", true);
    registerType<int32_t>("int32", true);
    registerType<uint32_t>("uint32", true);
    registerType<int64_t>("int64", true);
    registerType<uint64_t>("uint64", true);
    registerType<float>("float", true);
    registerType<double>("double", true);

    // Common types
    registerType<std::string>("string", false);

    // Note: void type registered separately (no sizeof for void)
    auto voidDescriptor = std::make_shared<TypeDescriptor>(
        "void",
        std::type_index(typeid(void)),
        0,  // size is 0 for void
        true  // isPrimitive
    );
    registerType(voidDescriptor);
}

} // namespace metadata
} // namespace ipc
