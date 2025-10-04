#ifndef CDMF_FRAMEWORK_PROPERTIES_H
#define CDMF_FRAMEWORK_PROPERTIES_H

#include "utils/properties.h"
#include <string>

namespace cdmf {

/**
 * @brief Framework configuration properties
 *
 * Extends the base Properties class with framework-specific configuration.
 * Provides strongly-typed access to common framework properties.
 */
class FrameworkProperties : public Properties {
public:
    // Property keys
    static constexpr const char* PROP_FRAMEWORK_NAME = "framework.name";
    static constexpr const char* PROP_FRAMEWORK_VERSION = "framework.version";
    static constexpr const char* PROP_FRAMEWORK_VENDOR = "framework.vendor";

    static constexpr const char* PROP_ENABLE_SECURITY = "framework.security.enabled";
    static constexpr const char* PROP_ENABLE_IPC = "framework.ipc.enabled";
    static constexpr const char* PROP_VERIFY_SIGNATURES = "framework.security.verify_signatures";
    static constexpr const char* PROP_AUTO_START_MODULES = "framework.modules.auto_start";

    static constexpr const char* PROP_EVENT_THREAD_POOL_SIZE = "framework.event.thread_pool_size";
    static constexpr const char* PROP_SERVICE_CACHE_SIZE = "framework.service.cache_size";
    static constexpr const char* PROP_MODULE_SEARCH_PATH = "framework.modules.search_path";

    static constexpr const char* PROP_LOG_LEVEL = "framework.log.level";
    static constexpr const char* PROP_LOG_FILE = "framework.log.file";

    /**
     * @brief Default constructor with default values
     */
    FrameworkProperties();

    /**
     * @brief Construct from base Properties
     */
    explicit FrameworkProperties(const Properties& props);

    /**
     * @brief Get framework name
     */
    std::string getFrameworkName() const;

    /**
     * @brief Set framework name
     */
    void setFrameworkName(const std::string& name);

    /**
     * @brief Get framework version
     */
    std::string getFrameworkVersion() const;

    /**
     * @brief Set framework version
     */
    void setFrameworkVersion(const std::string& version);

    /**
     * @brief Get framework vendor
     */
    std::string getFrameworkVendor() const;

    /**
     * @brief Set framework vendor
     */
    void setFrameworkVendor(const std::string& vendor);

    /**
     * @brief Check if security is enabled
     */
    bool isSecurityEnabled() const;

    /**
     * @brief Set security enabled
     */
    void setSecurityEnabled(bool enabled);

    /**
     * @brief Check if IPC is enabled
     */
    bool isIpcEnabled() const;

    /**
     * @brief Set IPC enabled
     */
    void setIpcEnabled(bool enabled);

    /**
     * @brief Check if signature verification is enabled
     */
    bool isSignatureVerificationEnabled() const;

    /**
     * @brief Set signature verification enabled
     */
    void setSignatureVerificationEnabled(bool enabled);

    /**
     * @brief Check if modules should auto-start
     */
    bool isAutoStartModulesEnabled() const;

    /**
     * @brief Set auto-start modules enabled
     */
    void setAutoStartModulesEnabled(bool enabled);

    /**
     * @brief Get event thread pool size
     */
    size_t getEventThreadPoolSize() const;

    /**
     * @brief Set event thread pool size
     */
    void setEventThreadPoolSize(size_t size);

    /**
     * @brief Get service cache size
     */
    size_t getServiceCacheSize() const;

    /**
     * @brief Set service cache size
     */
    void setServiceCacheSize(size_t size);

    /**
     * @brief Get module search path
     */
    std::string getModuleSearchPath() const;

    /**
     * @brief Set module search path
     */
    void setModuleSearchPath(const std::string& path);

    /**
     * @brief Get log level
     */
    std::string getLogLevel() const;

    /**
     * @brief Set log level
     */
    void setLogLevel(const std::string& level);

    /**
     * @brief Get log file path
     */
    std::string getLogFile() const;

    /**
     * @brief Set log file path
     */
    void setLogFile(const std::string& file);

    /**
     * @brief Validate framework properties
     * @return true if all required properties are set and valid
     */
    bool validate() const;

    /**
     * @brief Load default properties
     */
    void loadDefaults();
};

} // namespace cdmf

#endif // CDMF_FRAMEWORK_PROPERTIES_H
