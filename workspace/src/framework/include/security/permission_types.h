#ifndef CDMF_PERMISSION_TYPES_H
#define CDMF_PERMISSION_TYPES_H

#include <string>

namespace cdmf {
namespace security {

/**
 * @enum PermissionType
 * @brief Defines the types of permissions that can be granted to modules
 */
enum class PermissionType {
    SERVICE_GET,           ///< Permission to get/lookup services
    SERVICE_REGISTER,      ///< Permission to register services
    MODULE_LOAD,           ///< Permission to load modules
    MODULE_UNLOAD,         ///< Permission to unload modules
    MODULE_EXECUTE,        ///< Permission to execute module code
    FILE_READ,             ///< Permission to read files
    FILE_WRITE,            ///< Permission to write files
    NETWORK_CONNECT,       ///< Permission to make network connections
    NETWORK_BIND,          ///< Permission to bind to network ports
    IPC_SEND,              ///< Permission to send IPC messages
    IPC_RECEIVE,           ///< Permission to receive IPC messages
    PROPERTY_READ,         ///< Permission to read properties
    PROPERTY_WRITE,        ///< Permission to write properties
    EVENT_PUBLISH,         ///< Permission to publish events
    EVENT_SUBSCRIBE,       ///< Permission to subscribe to events
    ADMIN                  ///< Administrative permission (all permissions)
};

/**
 * @enum PermissionAction
 * @brief Actions that can be performed with permissions
 */
enum class PermissionAction {
    GRANT,                 ///< Grant the permission
    DENY,                  ///< Deny the permission
    REVOKE                 ///< Revoke a previously granted permission
};

/**
 * @brief Convert PermissionType to string representation
 * @param type The permission type
 * @return String representation of the permission type
 */
std::string permissionTypeToString(PermissionType type);

/**
 * @brief Convert string to PermissionType
 * @param str The string representation
 * @return The permission type
 * @throws std::invalid_argument if string is not a valid permission type
 */
PermissionType stringToPermissionType(const std::string& str);

/**
 * @brief Convert PermissionAction to string representation
 * @param action The permission action
 * @return String representation of the permission action
 */
std::string permissionActionToString(PermissionAction action);

/**
 * @brief Convert string to PermissionAction
 * @param str The string representation
 * @return The permission action
 * @throws std::invalid_argument if string is not a valid permission action
 */
PermissionAction stringToPermissionAction(const std::string& str);

} // namespace security
} // namespace cdmf

#endif // CDMF_PERMISSION_TYPES_H
