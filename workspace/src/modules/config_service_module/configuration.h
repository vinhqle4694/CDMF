#ifndef CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_H
#define CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_H

#include <string>
#include <shared_mutex>
#include "utils/properties.h"

namespace cdmf {
namespace services {

/**
 * @class Configuration
 * @brief Represents a configuration object with properties
 *
 * Configuration objects are identified by a PID (Persistent Identifier)
 * and contain key-value properties that can be updated dynamically.
 */
class Configuration {
public:
    /**
     * @brief Constructor
     * @param pid Persistent identifier for this configuration
     */
    explicit Configuration(const std::string& pid);

    /**
     * @brief Destructor
     */
    ~Configuration();

    /**
     * @brief Get the persistent identifier
     * @return The PID of this configuration
     */
    std::string getPid() const;

    /**
     * @brief Get the configuration properties
     * @return Reference to the properties
     */
    cdmf::Properties& getProperties();

    /**
     * @brief Get the configuration properties (const version)
     * @return Const reference to the properties
     */
    const cdmf::Properties& getProperties() const;

    /**
     * @brief Update the configuration with new properties
     * @param properties New properties to set
     */
    void update(const cdmf::Properties& properties);

    /**
     * @brief Remove this configuration
     */
    void remove();

    /**
     * @brief Check if this configuration is marked for removal
     * @return true if marked for removal, false otherwise
     */
    bool isRemoved() const;

private:
    std::string pid_;                    ///< Persistent identifier
    cdmf::Properties properties_;       ///< Configuration properties
    mutable std::shared_mutex mutex_;    ///< Mutex for thread-safe access
    bool removed_;                       ///< Flag indicating if configuration is removed
};

} // namespace services
} // namespace cdmf

#endif // CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_H
