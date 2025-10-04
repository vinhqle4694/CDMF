#ifndef CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_LISTENER_H
#define CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_LISTENER_H

namespace cdmf {
namespace services {

class ConfigurationEvent;

/**
 * @interface ConfigurationListener
 * @brief Interface for listening to configuration events
 *
 * Implement this interface to receive notifications when
 * configurations are created, updated, or deleted.
 */
class ConfigurationListener {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ConfigurationListener() = default;

    /**
     * @brief Called when a configuration event occurs
     * @param event The configuration event
     */
    virtual void configurationEvent(const ConfigurationEvent& event) = 0;
};

} // namespace services
} // namespace cdmf

#endif // CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_LISTENER_H
