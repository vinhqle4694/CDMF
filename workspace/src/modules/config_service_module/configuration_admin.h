#ifndef CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_ADMIN_H
#define CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_ADMIN_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <shared_mutex>
#include "configuration.h"
#include "configuration_listener.h"
#include "configuration_event.h"
#include "persistence_manager.h"
#include "service/command_dispatcher.h"

namespace cdmf {
namespace services {

/**
 * @class ConfigurationAdmin
 * @brief Central service for managing configurations
 *
 * The Configuration Admin service provides a centralized way to manage
 * configuration objects. It supports creating, updating, and deleting
 * configurations, as well as notifying listeners of changes.
 *
 * Implements ICommandDispatcher to provide CLI access to configuration
 * management operations. CLI methods are declared in the module manifest.
 */
class ConfigurationAdmin : public cdmf::ICommandDispatcher {
public:
    /**
     * @brief Constructor
     * @param storageDir Directory for persisting configurations (default: "./config")
     */
    explicit ConfigurationAdmin(const std::string& storageDir = "./config");

    /**
     * @brief Destructor
     */
    ~ConfigurationAdmin();

    /**
     * @brief Create a new configuration
     * @param pid Persistent identifier for the configuration
     * @return Pointer to the created configuration
     * @throws std::runtime_error if configuration already exists
     */
    Configuration* createConfiguration(const std::string& pid);

    /**
     * @brief Get an existing configuration or create if it doesn't exist
     * @param pid Persistent identifier
     * @return Pointer to the configuration
     */
    Configuration* getConfiguration(const std::string& pid);

    /**
     * @brief List configurations matching a filter
     * @param filter Filter string (LDAP-style filter, empty string matches all)
     * @return Vector of configuration pointers
     */
    std::vector<Configuration*> listConfigurations(const std::string& filter = "");

    /**
     * @brief Delete a configuration
     * @param pid Persistent identifier
     */
    void deleteConfiguration(const std::string& pid);

    /**
     * @brief Add a configuration listener
     * @param listener Listener to add
     */
    void addConfigurationListener(ConfigurationListener* listener);

    /**
     * @brief Remove a configuration listener
     * @param listener Listener to remove
     */
    void removeConfigurationListener(ConfigurationListener* listener);

    /**
     * @brief Load all configurations from persistent storage
     */
    void loadConfigurations();

    /**
     * @brief Save all configurations to persistent storage
     */
    void saveConfigurations();

    // ICommandDispatcher interface implementation
    /**
     * @brief Dispatch a CLI command to the appropriate method
     * @param methodName Name of the method to invoke
     * @param args Command arguments
     * @return Result message (success or error)
     */
    std::string dispatchCommand(const std::string& methodName,
                               const std::vector<std::string>& args) override;

private:
    /**
     * @brief Notify all listeners of a configuration event
     * @param event The event to notify
     */
    void notifyListeners(const ConfigurationEvent& event);

    /**
     * @brief Check if a configuration matches a filter
     * @param config Configuration to check
     * @param filter Filter string
     * @return true if matches, false otherwise
     */
    bool matchesFilter(const Configuration* config, const std::string& filter) const;

    std::map<std::string, std::unique_ptr<Configuration>> configurations_;  ///< All configurations
    std::vector<ConfigurationListener*> listeners_;                         ///< Registered listeners
    mutable std::shared_mutex mutex_;                                       ///< Mutex for thread-safety
    std::unique_ptr<PersistenceManager> persistenceManager_;               ///< Persistence manager
};

} // namespace services
} // namespace cdmf

#endif // CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_ADMIN_H
