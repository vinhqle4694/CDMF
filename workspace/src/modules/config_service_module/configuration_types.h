#ifndef CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_TYPES_H
#define CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_TYPES_H

namespace cdmf {
namespace services {

/**
 * @enum ConfigurationEventType
 * @brief Types of configuration events
 */
enum class ConfigurationEventType {
    CREATED,   ///< Configuration was created
    UPDATED,   ///< Configuration was updated
    DELETED    ///< Configuration was deleted
};

} // namespace services
} // namespace cdmf

#endif // CDMF_SERVICES_CONFIG_SERVICE_CONFIGURATION_TYPES_H
