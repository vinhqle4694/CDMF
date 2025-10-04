#ifndef CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_EVENT_H
#define CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_EVENT_H

#include <string>
#include "configuration_types.h"

namespace cdmf {
namespace services {

class Configuration;

/**
 * @class ConfigurationEvent
 * @brief Represents an event related to configuration changes
 */
class ConfigurationEvent {
public:
    /**
     * @brief Constructor
     * @param type Event type
     * @param pid Persistent identifier of the configuration
     */
    ConfigurationEvent(ConfigurationEventType type, const std::string& pid);

    /**
     * @brief Constructor with factory PID
     * @param type Event type
     * @param pid Persistent identifier of the configuration
     * @param factoryPid Factory persistent identifier
     */
    ConfigurationEvent(ConfigurationEventType type, const std::string& pid,
                       const std::string& factoryPid);

    /**
     * @brief Constructor with configuration reference
     * @param type Event type
     * @param pid Persistent identifier of the configuration
     * @param reference Pointer to the configuration object
     */
    ConfigurationEvent(ConfigurationEventType type, const std::string& pid,
                       Configuration* reference);

    /**
     * @brief Get the event type
     * @return Event type
     */
    ConfigurationEventType getType() const { return type_; }

    /**
     * @brief Get the configuration PID
     * @return PID string
     */
    const std::string& getPid() const { return pid_; }

    /**
     * @brief Get the factory PID (if applicable)
     * @return Factory PID string
     */
    const std::string& getFactoryPid() const { return factoryPid_; }

    /**
     * @brief Get the configuration reference
     * @return Pointer to configuration, or nullptr if not available
     */
    Configuration* getReference() const { return reference_; }

private:
    ConfigurationEventType type_;   ///< Event type
    std::string pid_;               ///< Persistent identifier
    std::string factoryPid_;        ///< Factory PID (optional)
    Configuration* reference_;      ///< Configuration reference (optional)
};

} // namespace services
} // namespace cdmf

#endif // CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_EVENT_H
