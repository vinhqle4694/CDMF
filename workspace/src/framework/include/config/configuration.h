#ifndef CDMF_CONFIGURATION_H
#define CDMF_CONFIGURATION_H

#include "utils/properties.h"
#include <string>
#include <vector>
#include <memory>

namespace cdmf {

/**
 * @brief Configuration object
 *
 * Represents a configuration for a module or service. Each configuration
 * is identified by a unique PID (Persistent Identifier).
 *
 * Configurations contain:
 * - PID: Unique identifier (e.g., "module.com.example.httpserver")
 * - Factory PID: Optional factory identifier for factory configurations
 * - Properties: Key-value pairs with type-safe accessors
 *
 * Configuration types:
 * 1. **Regular Configuration**: One configuration per PID
 *    - PID: "module.{symbolic-name}"
 *    - Used by: Single-instance modules
 *
 * 2. **Factory Configuration**: Multiple configurations from one factory PID
 *    - Factory PID: "factory.database.connection"
 *    - Instance PID: "factory.database.connection~primary"
 *    - Used by: Multi-instance services (e.g., connection pools)
 *
 * Thread-safety: All methods are thread-safe.
 *
 * Lifecycle:
 * - Created: Via ConfigurationAdmin::createConfiguration()
 * - Updated: Via Configuration::update()
 * - Deleted: Via Configuration::remove() or ConfigurationAdmin::deleteConfiguration()
 *
 * Example usage:
 * @code
 * // Get configuration
 * Configuration* config = context->getConfiguration();
 *
 * // Read properties with type-safe accessors and defaults
 * int port = config->getInt("server.port", 8080);
 * std::string host = config->getString("server.host", "0.0.0.0");
 * bool ssl = config->getBool("server.ssl.enabled", false);
 * double timeout = config->getDouble("server.timeout", 30.0);
 *
 * // Update configuration
 * Properties newProps;
 * newProps.set("server.port", 9090);
 * newProps.set("server.threads", 20);
 * config->update(newProps);  // Fires UPDATED event
 * @endcode
 */
class Configuration {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~Configuration() = default;

    // ==================================================================
    // Identity
    // ==================================================================

    /**
     * @brief Get the Persistent Identifier (PID)
     *
     * The PID uniquely identifies this configuration.
     * - Regular configuration: "module.{symbolic-name}"
     * - Factory configuration: "{factory-pid}~{instance-name}"
     *
     * @return Configuration PID
     */
    virtual std::string getPid() const = 0;

    /**
     * @brief Get the Factory PID
     *
     * For factory configurations, this returns the factory PID.
     * For regular configurations, this returns an empty string.
     *
     * @return Factory PID, or empty string if not a factory configuration
     */
    virtual std::string getFactoryPid() const = 0;

    /**
     * @brief Check if this is a factory configuration
     * @return true if factory PID is set, false otherwise
     */
    virtual bool isFactoryConfiguration() const = 0;

    // ==================================================================
    // Properties Access
    // ==================================================================

    /**
     * @brief Get all properties (read-only)
     * @return Reference to properties
     */
    virtual const Properties& getProperties() const = 0;

    /**
     * @brief Get all properties (mutable)
     *
     * Warning: Modifying properties directly does not fire events.
     * Use update() to trigger event notification.
     *
     * @return Reference to properties
     */
    virtual Properties& getProperties() = 0;

    // ==================================================================
    // Type-Safe Getters
    // ==================================================================

    /**
     * @brief Get a string property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as string, or defaultValue if not found
     */
    virtual std::string getString(const std::string& key,
                                   const std::string& defaultValue = "") const = 0;

    /**
     * @brief Get an integer property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as int, or defaultValue if not found
     */
    virtual int getInt(const std::string& key, int defaultValue = 0) const = 0;

    /**
     * @brief Get a boolean property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as bool, or defaultValue if not found
     */
    virtual bool getBool(const std::string& key, bool defaultValue = false) const = 0;

    /**
     * @brief Get a double property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as double, or defaultValue if not found
     */
    virtual double getDouble(const std::string& key, double defaultValue = 0.0) const = 0;

    /**
     * @brief Get a long property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as long, or defaultValue if not found
     */
    virtual long getLong(const std::string& key, long defaultValue = 0L) const = 0;

    /**
     * @brief Get a string array property
     *
     * String arrays are stored as vector<string> in Properties.
     *
     * @param key Property key
     * @return Vector of strings, or empty vector if not found
     */
    virtual std::vector<std::string> getStringArray(const std::string& key) const = 0;

    /**
     * @brief Check if a property exists
     * @param key Property key
     * @return true if property exists, false otherwise
     */
    virtual bool hasProperty(const std::string& key) const = 0;

    // ==================================================================
    // Configuration Modification
    // ==================================================================

    /**
     * @brief Update configuration with new properties
     *
     * Replaces all existing properties with the provided properties.
     * Fires a UPDATED event to registered configuration listeners.
     *
     * Thread-safety: This method is thread-safe and can be called
     * from any thread.
     *
     * @param props New properties to set
     * @throws std::runtime_error if configuration was already deleted
     */
    virtual void update(const Properties& props) = 0;

    /**
     * @brief Remove this configuration
     *
     * Deletes this configuration from the Configuration Admin.
     * Fires a DELETED event to registered configuration listeners.
     *
     * After removal, this Configuration object should not be used.
     *
     * @throws std::runtime_error if configuration was already deleted
     */
    virtual void remove() = 0;

    // ==================================================================
    // State
    // ==================================================================

    /**
     * @brief Check if configuration has been deleted
     * @return true if deleted, false otherwise
     */
    virtual bool isDeleted() const = 0;

    /**
     * @brief Get the number of properties
     * @return Number of properties
     */
    virtual size_t size() const = 0;

    /**
     * @brief Check if configuration has no properties
     * @return true if empty, false otherwise
     */
    virtual bool empty() const = 0;
};

} // namespace cdmf

#endif // CDMF_CONFIGURATION_H
