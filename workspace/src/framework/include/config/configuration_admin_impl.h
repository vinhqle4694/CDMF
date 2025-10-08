#ifndef CDMF_CONFIGURATION_ADMIN_IMPL_H
#define CDMF_CONFIGURATION_ADMIN_IMPL_H

#include "config/configuration_admin.h"
#include "config/configuration_impl.h"
#include "config/configuration_listener.h"
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <memory>

namespace cdmf {

/**
 * @brief Configuration Admin implementation
 *
 * Thread-safe implementation of the Configuration Admin service.
 * Manages all configurations and fires events to registered listeners.
 */
class ConfigurationAdminImpl : public IConfigurationAdmin {
public:
    /**
     * @brief Default constructor
     */
    ConfigurationAdminImpl();

    /**
     * @brief Destructor
     */
    ~ConfigurationAdminImpl() override;

    // Disable copy and move
    ConfigurationAdminImpl(const ConfigurationAdminImpl&) = delete;
    ConfigurationAdminImpl& operator=(const ConfigurationAdminImpl&) = delete;
    ConfigurationAdminImpl(ConfigurationAdminImpl&&) = delete;
    ConfigurationAdminImpl& operator=(ConfigurationAdminImpl&&) = delete;

    // ==================================================================
    // Configuration Management
    // ==================================================================

    Configuration* createConfiguration(const std::string& pid) override;
    Configuration* getConfiguration(const std::string& pid) override;
    std::vector<Configuration*> listConfigurations(const std::string& filter = "") override;
    bool deleteConfiguration(const std::string& pid) override;

    // ==================================================================
    // Factory Configurations
    // ==================================================================

    Configuration* createFactoryConfiguration(const std::string& factoryPid) override;
    Configuration* createFactoryConfiguration(const std::string& factoryPid,
                                              const std::string& instanceName) override;
    std::vector<Configuration*> listFactoryConfigurations(
        const std::string& factoryPid) override;

    // ==================================================================
    // File Persistence
    // ==================================================================

    bool loadFromFile(const std::string& path) override;
    bool saveToFile(const std::string& path, const std::string& pid) override;
    int loadFromDirectory(const std::string& path) override;
    int saveToDirectory(const std::string& path) override;

    // ==================================================================
    // Configuration Listeners
    // ==================================================================

    void addConfigurationListener(IConfigurationListener* listener) override;
    bool removeConfigurationListener(IConfigurationListener* listener) override;
    size_t getListenerCount() const override;

    // ==================================================================
    // Statistics and Information
    // ==================================================================

    size_t getConfigurationCount() const override;
    size_t getFactoryConfigurationCount() const override;
    void clearAll() override;

private:
    // ==================================================================
    // Internal Methods
    // ==================================================================

    /**
     * @brief Update callback from ConfigurationImpl
     *
     * Called when a configuration is updated or removed.
     * Fires appropriate event to all listeners.
     *
     * @param config Configuration that was updated/removed
     * @param pid Configuration PID
     * @param factoryPid Factory PID (empty if not a factory configuration)
     * @param oldProps Old properties (before update)
     * @param newProps New properties (after update, empty if removed)
     * @param removed true if removed, false if updated
     */
    void onConfigurationChanged(ConfigurationImpl* config,
                                const std::string& pid,
                                const std::string& factoryPid,
                                const Properties& oldProps,
                                const Properties& newProps,
                                bool removed);

    /**
     * @brief Fire configuration event to all listeners
     *
     * @param event Configuration event to fire
     */
    void fireConfigurationEvent(const ConfigurationEvent& event);

    /**
     * @brief Generate unique instance name for factory configuration
     *
     * @param factoryPid Factory PID
     * @return Unique instance name
     */
    std::string generateInstanceName(const std::string& factoryPid);

    /**
     * @brief Build instance PID from factory PID and instance name
     *
     * Format: {factoryPid}~{instanceName}
     *
     * @param factoryPid Factory PID
     * @param instanceName Instance name
     * @return Full instance PID
     */
    std::string buildInstancePid(const std::string& factoryPid,
                                  const std::string& instanceName);

    /**
     * @brief Validate PID format
     *
     * @param pid PID to validate
     * @return true if valid, false otherwise
     */
    bool isValidPid(const std::string& pid) const;

    /**
     * @brief Match PID against filter
     *
     * Supports wildcards:
     * - "module.*" matches "module.http.server", "module.database", etc.
     * - "*" matches all
     *
     * @param pid PID to match
     * @param filter Filter pattern
     * @return true if matches, false otherwise
     */
    bool matchesFilter(const std::string& pid, const std::string& filter) const;

    // ==================================================================
    // Data Members
    // ==================================================================

    // Configuration storage: PID → Configuration
    std::map<std::string, std::unique_ptr<ConfigurationImpl>> configurations_;

    // Factory configuration tracking: Factory PID → Set of instance PIDs
    std::map<std::string, std::set<std::string>> factory_instances_;

    // Registered listeners
    std::set<IConfigurationListener*> listeners_;

    // Factory instance counter for generating unique instance names
    std::atomic<uint64_t> factory_instance_counter_;

    // Mutex for thread-safety
    mutable std::mutex mutex_;
};

} // namespace cdmf

#endif // CDMF_CONFIGURATION_ADMIN_IMPL_H
