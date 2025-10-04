#include "core/framework_properties.h"

namespace cdmf {

FrameworkProperties::FrameworkProperties() : Properties() {
    loadDefaults();
}

FrameworkProperties::FrameworkProperties(const Properties& props) : Properties(props) {
    // Ensure defaults are set for any missing properties
    if (!has(PROP_FRAMEWORK_NAME)) {
        loadDefaults();
    }
}

std::string FrameworkProperties::getFrameworkName() const {
    return getString(PROP_FRAMEWORK_NAME, "CDMF");
}

void FrameworkProperties::setFrameworkName(const std::string& name) {
    set(PROP_FRAMEWORK_NAME, name);
}

std::string FrameworkProperties::getFrameworkVersion() const {
    return getString(PROP_FRAMEWORK_VERSION, "1.0.0");
}

void FrameworkProperties::setFrameworkVersion(const std::string& version) {
    set(PROP_FRAMEWORK_VERSION, version);
}

std::string FrameworkProperties::getFrameworkVendor() const {
    return getString(PROP_FRAMEWORK_VENDOR, "CDMF Project");
}

void FrameworkProperties::setFrameworkVendor(const std::string& vendor) {
    set(PROP_FRAMEWORK_VENDOR, vendor);
}

bool FrameworkProperties::isSecurityEnabled() const {
    return getBool(PROP_ENABLE_SECURITY, false);
}

void FrameworkProperties::setSecurityEnabled(bool enabled) {
    set(PROP_ENABLE_SECURITY, enabled);
}

bool FrameworkProperties::isIpcEnabled() const {
    return getBool(PROP_ENABLE_IPC, false);
}

void FrameworkProperties::setIpcEnabled(bool enabled) {
    set(PROP_ENABLE_IPC, enabled);
}

bool FrameworkProperties::isSignatureVerificationEnabled() const {
    return getBool(PROP_VERIFY_SIGNATURES, false);
}

void FrameworkProperties::setSignatureVerificationEnabled(bool enabled) {
    set(PROP_VERIFY_SIGNATURES, enabled);
}

bool FrameworkProperties::isAutoStartModulesEnabled() const {
    return getBool(PROP_AUTO_START_MODULES, true);
}

void FrameworkProperties::setAutoStartModulesEnabled(bool enabled) {
    set(PROP_AUTO_START_MODULES, enabled);
}

size_t FrameworkProperties::getEventThreadPoolSize() const {
    return static_cast<size_t>(getInt(PROP_EVENT_THREAD_POOL_SIZE, 4));
}

void FrameworkProperties::setEventThreadPoolSize(size_t size) {
    set(PROP_EVENT_THREAD_POOL_SIZE, static_cast<int>(size));
}

size_t FrameworkProperties::getServiceCacheSize() const {
    return static_cast<size_t>(getInt(PROP_SERVICE_CACHE_SIZE, 100));
}

void FrameworkProperties::setServiceCacheSize(size_t size) {
    set(PROP_SERVICE_CACHE_SIZE, static_cast<int>(size));
}

std::string FrameworkProperties::getModuleSearchPath() const {
    return getString(PROP_MODULE_SEARCH_PATH, "./modules");
}

void FrameworkProperties::setModuleSearchPath(const std::string& path) {
    set(PROP_MODULE_SEARCH_PATH, path);
}

std::string FrameworkProperties::getLogLevel() const {
    return getString(PROP_LOG_LEVEL, "INFO");
}

void FrameworkProperties::setLogLevel(const std::string& level) {
    set(PROP_LOG_LEVEL, level);
}

std::string FrameworkProperties::getLogFile() const {
    return getString(PROP_LOG_FILE, "cdmf.log");
}

void FrameworkProperties::setLogFile(const std::string& file) {
    set(PROP_LOG_FILE, file);
}

bool FrameworkProperties::validate() const {
    // Check required properties
    if (getFrameworkName().empty()) {
        return false;
    }

    if (getFrameworkVersion().empty()) {
        return false;
    }

    // Validate thread pool size
    if (getEventThreadPoolSize() == 0 || getEventThreadPoolSize() > 100) {
        return false;
    }

    // Validate cache size
    if (getServiceCacheSize() == 0) {
        return false;
    }

    return true;
}

void FrameworkProperties::loadDefaults() {
    // Framework identity
    if (!has(PROP_FRAMEWORK_NAME)) {
        set(PROP_FRAMEWORK_NAME, std::string("CDMF"));
    }
    if (!has(PROP_FRAMEWORK_VERSION)) {
        set(PROP_FRAMEWORK_VERSION, std::string("1.0.0"));
    }
    if (!has(PROP_FRAMEWORK_VENDOR)) {
        set(PROP_FRAMEWORK_VENDOR, std::string("CDMF Project"));
    }

    // Security settings
    if (!has(PROP_ENABLE_SECURITY)) {
        set(PROP_ENABLE_SECURITY, false);
    }
    if (!has(PROP_VERIFY_SIGNATURES)) {
        set(PROP_VERIFY_SIGNATURES, false);
    }

    // IPC settings
    if (!has(PROP_ENABLE_IPC)) {
        set(PROP_ENABLE_IPC, false);
    }

    // Module settings
    if (!has(PROP_AUTO_START_MODULES)) {
        set(PROP_AUTO_START_MODULES, true);
    }
    if (!has(PROP_MODULE_SEARCH_PATH)) {
        set(PROP_MODULE_SEARCH_PATH, std::string("./modules"));
    }

    // Performance settings
    if (!has(PROP_EVENT_THREAD_POOL_SIZE)) {
        set(PROP_EVENT_THREAD_POOL_SIZE, 4);
    }
    if (!has(PROP_SERVICE_CACHE_SIZE)) {
        set(PROP_SERVICE_CACHE_SIZE, 100);
    }

    // Logging settings
    if (!has(PROP_LOG_LEVEL)) {
        set(PROP_LOG_LEVEL, std::string("INFO"));
    }
    if (!has(PROP_LOG_FILE)) {
        set(PROP_LOG_FILE, std::string("cdmf.log"));
    }
}

} // namespace cdmf
