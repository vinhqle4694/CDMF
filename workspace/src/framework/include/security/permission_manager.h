#ifndef CDMF_PERMISSION_MANAGER_H
#define CDMF_PERMISSION_MANAGER_H

#include "permission.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <vector>

namespace cdmf {

// Forward declarations
namespace module {
    class Module;
}

namespace security {

/**
 * @class PermissionManager
 * @brief Manages permissions for modules in the framework
 *
 * The PermissionManager is responsible for:
 * - Granting and revoking permissions for modules
 * - Checking if a module has permission to perform operations
 * - Managing default permissions for new modules
 * - Persisting permission configurations
 *
 * Thread-safe for concurrent access.
 */
class PermissionManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the PermissionManager instance
     */
    static PermissionManager& getInstance();

    /**
     * @brief Delete copy constructor
     */
    PermissionManager(const PermissionManager&) = delete;

    /**
     * @brief Delete assignment operator
     */
    PermissionManager& operator=(const PermissionManager&) = delete;

    /**
     * @brief Grant a permission to a module
     * @param moduleId The module ID
     * @param permission The permission to grant
     * @return true if granted successfully
     */
    bool grantPermission(const std::string& moduleId, std::shared_ptr<Permission> permission);

    /**
     * @brief Revoke a permission from a module
     * @param moduleId The module ID
     * @param permission The permission to revoke
     * @return true if revoked successfully
     */
    bool revokePermission(const std::string& moduleId, std::shared_ptr<Permission> permission);

    /**
     * @brief Check if a module has a specific permission
     * @param moduleId The module ID
     * @param permission The permission to check
     * @return true if the module has the permission
     */
    bool hasPermission(const std::string& moduleId, const Permission& permission) const;

    /**
     * @brief Check if a module can perform a specific operation
     * @param moduleId The module ID
     * @param type The permission type
     * @param target The target resource
     * @return true if the module has permission
     */
    bool checkPermission(const std::string& moduleId, PermissionType type,
                        const std::string& target = "*") const;

    /**
     * @brief Get all permissions for a module
     * @param moduleId The module ID
     * @return Vector of all permissions for the module
     */
    std::vector<std::shared_ptr<Permission>> getPermissions(const std::string& moduleId) const;

    /**
     * @brief Get permissions of a specific type for a module
     * @param moduleId The module ID
     * @param type The permission type
     * @return Vector of matching permissions
     */
    std::vector<std::shared_ptr<Permission>> getPermissionsByType(
        const std::string& moduleId, PermissionType type) const;

    /**
     * @brief Clear all permissions for a module
     * @param moduleId The module ID
     */
    void clearPermissions(const std::string& moduleId);

    /**
     * @brief Set default permissions applied to all new modules
     * @param permissions Vector of default permissions
     */
    void setDefaultPermissions(const std::vector<std::shared_ptr<Permission>>& permissions);

    /**
     * @brief Get default permissions
     * @return Vector of default permissions
     */
    std::vector<std::shared_ptr<Permission>> getDefaultPermissions() const;

    /**
     * @brief Apply default permissions to a module
     * @param moduleId The module ID
     */
    void applyDefaultPermissions(const std::string& moduleId);

    /**
     * @brief Load permissions from a configuration file
     * @param configPath Path to the configuration file
     * @return true if loaded successfully
     */
    bool loadPermissionsFromConfig(const std::string& configPath);

    /**
     * @brief Save permissions to a configuration file
     * @param configPath Path to the configuration file
     * @return true if saved successfully
     */
    bool savePermissionsToConfig(const std::string& configPath) const;

    /**
     * @brief Get all module IDs that have permissions registered
     * @return Vector of module IDs
     */
    std::vector<std::string> getAllModuleIds() const;

    /**
     * @brief Check if a module is registered in the permission system
     * @param moduleId The module ID
     * @return true if the module has any permissions
     */
    bool hasModule(const std::string& moduleId) const;

    /**
     * @brief Reset the permission manager (clear all permissions)
     */
    void reset();

private:
    /**
     * @brief Private constructor for singleton
     */
    PermissionManager();

    /**
     * @brief Destructor
     */
    ~PermissionManager() = default;

    /**
     * @brief Get or create permission collection for a module
     * @param moduleId The module ID
     * @return Reference to the permission collection
     */
    PermissionCollection& getOrCreateCollection(const std::string& moduleId);

    /**
     * @brief Module permissions storage (moduleId -> PermissionCollection)
     */
    std::map<std::string, PermissionCollection> modulePermissions_;

    /**
     * @brief Default permissions applied to new modules
     */
    std::vector<std::shared_ptr<Permission>> defaultPermissions_;

    /**
     * @brief Mutex for thread-safe access
     */
    mutable std::mutex mutex_;
};

} // namespace security
} // namespace cdmf

#endif // CDMF_PERMISSION_MANAGER_H
