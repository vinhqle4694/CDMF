#ifndef CDMF_PERMISSION_H
#define CDMF_PERMISSION_H

#include "permission_types.h"
#include <string>
#include <memory>
#include <vector>

namespace cdmf {
namespace security {

/**
 * @class Permission
 * @brief Represents a security permission with a type, target, and action
 *
 * Permissions control access to resources and operations within the framework.
 * Each permission has a type (e.g., SERVICE_GET), an optional target pattern,
 * and can be checked against module requests.
 */
class Permission {
public:
    /**
     * @brief Construct a new Permission
     * @param type The type of permission
     * @param target Target pattern for the permission (e.g., "com.example.*")
     * @param action The action associated with this permission
     */
    Permission(PermissionType type, const std::string& target = "*",
               PermissionAction action = PermissionAction::GRANT);

    /**
     * @brief Destructor
     */
    virtual ~Permission() = default;

    /**
     * @brief Get the permission type
     * @return The permission type
     */
    PermissionType getType() const;

    /**
     * @brief Get the target pattern
     * @return The target pattern string
     */
    std::string getTarget() const;

    /**
     * @brief Get the permission action
     * @return The permission action
     */
    PermissionAction getAction() const;

    /**
     * @brief Check if this permission implies another permission
     * @param other The other permission to check
     * @return true if this permission implies the other
     */
    bool implies(const Permission& other) const;

    /**
     * @brief Check if target pattern matches a specific target
     * @param target The target to match against
     * @return true if the pattern matches
     */
    bool matchesTarget(const std::string& target) const;

    /**
     * @brief Get a string representation of this permission
     * @return String representation
     */
    std::string toString() const;

    /**
     * @brief Check if two permissions are equal
     * @param other The other permission
     * @return true if equal
     */
    bool equals(const Permission& other) const;

    /**
     * @brief Create a permission from a string representation
     * @param str String in format "TYPE:TARGET:ACTION"
     * @return Shared pointer to new Permission
     */
    static std::shared_ptr<Permission> fromString(const std::string& str);

private:
    PermissionType type_;
    std::string target_;
    PermissionAction action_;

    /**
     * @brief Match wildcard pattern against target
     * @param pattern Pattern with wildcards (*)
     * @param target Target string to match
     * @return true if pattern matches target
     */
    bool wildcardMatch(const std::string& pattern, const std::string& target) const;
};

/**
 * @class PermissionCollection
 * @brief Collection of permissions for efficient checking
 */
class PermissionCollection {
public:
    /**
     * @brief Add a permission to the collection
     * @param permission The permission to add
     */
    void add(std::shared_ptr<Permission> permission);

    /**
     * @brief Remove a permission from the collection
     * @param permission The permission to remove
     * @return true if removed, false if not found
     */
    bool remove(std::shared_ptr<Permission> permission);

    /**
     * @brief Check if the collection implies a permission
     * @param permission The permission to check
     * @return true if any permission in the collection implies it
     */
    bool implies(const Permission& permission) const;

    /**
     * @brief Get all permissions in the collection
     * @return Vector of all permissions
     */
    std::vector<std::shared_ptr<Permission>> getPermissions() const;

    /**
     * @brief Get all permissions of a specific type
     * @param type The permission type to filter by
     * @return Vector of matching permissions
     */
    std::vector<std::shared_ptr<Permission>> getPermissionsByType(PermissionType type) const;

    /**
     * @brief Clear all permissions
     */
    void clear();

    /**
     * @brief Get the number of permissions
     * @return Permission count
     */
    size_t size() const;

    /**
     * @brief Check if collection is empty
     * @return true if empty
     */
    bool empty() const;

private:
    std::vector<std::shared_ptr<Permission>> permissions_;
};

} // namespace security
} // namespace cdmf

#endif // CDMF_PERMISSION_H
