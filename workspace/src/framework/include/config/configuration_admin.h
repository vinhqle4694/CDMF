#ifndef CDMF_CONFIGURATION_ADMIN_H
#define CDMF_CONFIGURATION_ADMIN_H

#include "config/configuration.h"
#include "config/configuration_listener.h"
#include <string>
#include <vector>
#include <memory>

namespace cdmf {

/**
 * @brief Configuration Admin service interface
 *
 * Central service for managing configurations in the CDMF framework.
 * Provides:
 * - Configuration CRUD operations (create, read, update, delete)
 * - Factory configurations (multiple instances from one factory PID)
 * - File persistence (JSON load/save)
 * - Configuration events (listener registration and notification)
 *
 * The Configuration Admin service:
 * 1. Manages all configurations for framework and modules
 * 2. Persists configurations to/from files
 * 3. Distributes configurations to modules on activation
 * 4. Fires events when configurations change
 * 5. Supports factory configurations for multi-instance services
 *
 * PID (Persistent Identifier) Naming Conventions:
 * - Framework: "cdmf.framework"
 * - Module: "module.{symbolic-name}"
 * - Service: "service.{interface-name}"
 * - Factory instance: "{factory-pid}~{instance-name}"
 *
 * Thread-safety: All methods are thread-safe.
 *
 * Example usage:
 * @code
 * // Get Configuration Admin service from framework
 * IConfigurationAdmin* configAdmin = framework->getConfigurationAdmin();
 *
 * // Load configurations from file
 * configAdmin->loadFromFile("config/framework.json");
 * configAdmin->loadFromDirectory("config/modules/");
 *
 * // Create a new configuration
 * Configuration* config = configAdmin->createConfiguration("module.http.server");
 * Properties props;
 * props.set("server.port", 8080);
 * props.set("server.threads", 10);
 * config->update(props);
 *
 * // Save configurations to file
 * configAdmin->saveToFile("config/framework.json");
 * configAdmin->saveToDirectory("config/modules/");
 * @endcode
 */
class IConfigurationAdmin {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IConfigurationAdmin() = default;

    // ==================================================================
    // Configuration Management
    // ==================================================================

    /**
     * @brief Create a new configuration with the specified PID
     *
     * If a configuration with this PID already exists, returns the existing one.
     *
     * The configuration is initially empty. Use Configuration::update() to
     * set properties, which will fire a CREATED event.
     *
     * @param pid Persistent Identifier (must be unique)
     * @return Pointer to configuration object (never null)
     * @throws std::invalid_argument if PID is empty or invalid
     */
    virtual Configuration* createConfiguration(const std::string& pid) = 0;

    /**
     * @brief Get an existing configuration by PID
     *
     * @param pid Persistent Identifier
     * @return Pointer to configuration, or nullptr if not found
     */
    virtual Configuration* getConfiguration(const std::string& pid) = 0;

    /**
     * @brief List all configurations matching a filter
     *
     * Filter syntax (LDAP-style):
     * - Empty string: Match all configurations
     * - "pid=module.*": Match PIDs starting with "module."
     * - "factoryPid=factory.database.connection": Match factory configurations
     *
     * @param filter LDAP-style filter (empty = all)
     * @return Vector of matching configurations
     */
    virtual std::vector<Configuration*> listConfigurations(const std::string& filter = "") = 0;

    /**
     * @brief Delete a configuration by PID
     *
     * Removes the configuration and fires a DELETED event.
     * The Configuration object should not be used after deletion.
     *
     * @param pid Persistent Identifier
     * @return true if deleted, false if not found
     */
    virtual bool deleteConfiguration(const std::string& pid) = 0;

    // ==================================================================
    // Factory Configurations
    // ==================================================================

    /**
     * @brief Create a new factory configuration instance
     *
     * Factory configurations allow creating multiple configuration instances
     * from a single factory PID.
     *
     * Instance PIDs are generated as: "{factoryPid}~{unique-id}"
     *
     * Example:
     * - Factory PID: "factory.database.connection"
     * - Instance 1: "factory.database.connection~primary"
     * - Instance 2: "factory.database.connection~secondary"
     *
     * @param factoryPid Factory PID
     * @return Pointer to new configuration instance
     * @throws std::invalid_argument if factory PID is empty
     */
    virtual Configuration* createFactoryConfiguration(const std::string& factoryPid) = 0;

    /**
     * @brief Create a factory configuration with explicit instance name
     *
     * @param factoryPid Factory PID
     * @param instanceName Instance name (will generate PID: factoryPid~instanceName)
     * @return Pointer to new configuration instance
     * @throws std::invalid_argument if factory PID or instance name is empty
     */
    virtual Configuration* createFactoryConfiguration(const std::string& factoryPid,
                                                      const std::string& instanceName) = 0;

    /**
     * @brief List all factory configuration instances for a factory PID
     *
     * @param factoryPid Factory PID
     * @return Vector of configurations with this factory PID
     */
    virtual std::vector<Configuration*> listFactoryConfigurations(
        const std::string& factoryPid) = 0;

    // ==================================================================
    // File Persistence
    // ==================================================================

    /**
     * @brief Load a single configuration from a JSON file
     *
     * File format:
     * {
     *   "pid": "module.http.server",
     *   "properties": {
     *     "server.port": 8080,
     *     "server.host": "0.0.0.0"
     *   }
     * }
     *
     * @param path Path to JSON file
     * @return true if loaded successfully, false on error
     */
    virtual bool loadFromFile(const std::string& path) = 0;

    /**
     * @brief Save a single configuration to a JSON file
     *
     * @param path Path to JSON file
     * @param pid PID of configuration to save
     * @return true if saved successfully, false on error
     */
    virtual bool saveToFile(const std::string& path, const std::string& pid) = 0;

    /**
     * @brief Load all configurations from a directory
     *
     * Loads all *.json files from the specified directory.
     * Each file should contain a single configuration.
     *
     * @param path Directory path
     * @return Number of configurations loaded
     */
    virtual int loadFromDirectory(const std::string& path) = 0;

    /**
     * @brief Save all configurations to a directory
     *
     * Saves each configuration to a separate JSON file:
     * - {directory}/{pid}.json
     *
     * @param path Directory path
     * @return Number of configurations saved
     */
    virtual int saveToDirectory(const std::string& path) = 0;

    // ==================================================================
    // Configuration Listeners
    // ==================================================================

    /**
     * @brief Add a configuration listener
     *
     * Listener will receive notifications for all configuration events
     * (CREATED, UPDATED, DELETED).
     *
     * Listeners are not automatically removed. Call removeConfigurationListener()
     * when done.
     *
     * @param listener Configuration listener (must not be null)
     * @throws std::invalid_argument if listener is null
     */
    virtual void addConfigurationListener(IConfigurationListener* listener) = 0;

    /**
     * @brief Remove a configuration listener
     *
     * @param listener Configuration listener to remove
     * @return true if removed, false if not registered
     */
    virtual bool removeConfigurationListener(IConfigurationListener* listener) = 0;

    /**
     * @brief Get the number of registered listeners
     * @return Number of listeners
     */
    virtual size_t getListenerCount() const = 0;

    // ==================================================================
    // Statistics and Information
    // ==================================================================

    /**
     * @brief Get the total number of configurations
     * @return Number of configurations
     */
    virtual size_t getConfigurationCount() const = 0;

    /**
     * @brief Get the number of factory configurations
     * @return Number of factory configuration instances
     */
    virtual size_t getFactoryConfigurationCount() const = 0;

    /**
     * @brief Clear all configurations
     *
     * Removes all configurations and fires DELETED events.
     * Use with caution - typically only for testing or shutdown.
     */
    virtual void clearAll() = 0;
};

} // namespace cdmf

#endif // CDMF_CONFIGURATION_ADMIN_H
