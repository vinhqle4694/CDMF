#include "security/permission_manager.h"
#include "utils/log.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace cdmf {
namespace security {

// ========== Singleton Instance ==========

PermissionManager& PermissionManager::getInstance() {
    static PermissionManager instance;
    return instance;
}

PermissionManager::PermissionManager() {
    LOGI("Initializing PermissionManager");
    // Initialize with some safe default permissions
    defaultPermissions_.push_back(
        std::make_shared<Permission>(PermissionType::SERVICE_GET, "*")
    );
    defaultPermissions_.push_back(
        std::make_shared<Permission>(PermissionType::EVENT_SUBSCRIBE, "*")
    );
    defaultPermissions_.push_back(
        std::make_shared<Permission>(PermissionType::PROPERTY_READ, "*")
    );
    LOGI_FMT("PermissionManager initialized with " << defaultPermissions_.size() << " default permissions");
}

// ========== Permission Management ==========

bool PermissionManager::grantPermission(const std::string& moduleId,
                                       std::shared_ptr<Permission> permission) {
    if (!permission || moduleId.empty()) {
        LOGW_FMT("Failed to grant permission: " << (moduleId.empty() ? "empty moduleId" : "null permission"));
        return false;
    }

    LOGI_FMT("Granting permission to module '" << moduleId << "': " << permission->toString());
    std::lock_guard<std::mutex> lock(mutex_);
    auto& collection = getOrCreateCollection(moduleId);
    collection.add(permission);
    LOGD_FMT("Permission granted successfully. Module '" << moduleId << "' now has " << collection.size() << " permissions");
    return true;
}

bool PermissionManager::revokePermission(const std::string& moduleId,
                                        std::shared_ptr<Permission> permission) {
    if (!permission || moduleId.empty()) {
        LOGW_FMT("Failed to revoke permission: " << (moduleId.empty() ? "empty moduleId" : "null permission"));
        return false;
    }

    LOGI_FMT("Revoking permission from module '" << moduleId << "': " << permission->toString());
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modulePermissions_.find(moduleId);
    if (it == modulePermissions_.end()) {
        LOGW_FMT("Module '" << moduleId << "' not found in permission registry");
        return false;
    }

    bool result = it->second.remove(permission);
    if (result) {
        LOGD_FMT("Permission revoked successfully. Module '" << moduleId << "' now has " << it->second.size() << " permissions");
    } else {
        LOGD_FMT("Permission not found for module '" << moduleId << "'");
    }
    return result;
}

bool PermissionManager::hasPermission(const std::string& moduleId,
                                      const Permission& permission) const {
    LOGD_FMT("Checking if module '" << moduleId << "' has permission: " << permission.toString());
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modulePermissions_.find(moduleId);
    if (it == modulePermissions_.end()) {
        LOGD_FMT("  Module '" << moduleId << "' not found in permission registry");
        return false;
    }

    bool result = it->second.implies(permission);
    LOGI_FMT("  Permission check for '" << moduleId << "': " << (result ? "GRANTED" : "DENIED"));
    return result;
}

bool PermissionManager::checkPermission(const std::string& moduleId,
                                       PermissionType type,
                                       const std::string& target) const {
    LOGD_FMT("checkPermission for module '" << moduleId << "': type=" << permissionTypeToString(type) << ", target=" << target);
    Permission checkPerm(type, target, PermissionAction::GRANT);
    bool result = hasPermission(moduleId, checkPerm);
    LOGD_FMT("  checkPermission result: " << (result ? "ALLOWED" : "DENIED"));
    return result;
}

std::vector<std::shared_ptr<Permission>> PermissionManager::getPermissions(
    const std::string& moduleId) const {
    LOGD_FMT("Getting all permissions for module: " << moduleId);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modulePermissions_.find(moduleId);
    if (it == modulePermissions_.end()) {
        LOGD_FMT("Module '" << moduleId << "' has no permissions");
        return {};
    }

    auto perms = it->second.getPermissions();
    LOGD_FMT("Module '" << moduleId << "' has " << perms.size() << " permissions");
    return perms;
}

std::vector<std::shared_ptr<Permission>> PermissionManager::getPermissionsByType(
    const std::string& moduleId, PermissionType type) const {
    LOGD_FMT("Getting permissions by type for module '" << moduleId << "': " << permissionTypeToString(type));
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modulePermissions_.find(moduleId);
    if (it == modulePermissions_.end()) {
        LOGD_FMT("Module '" << moduleId << "' not found");
        return {};
    }

    auto perms = it->second.getPermissionsByType(type);
    LOGD_FMT("Found " << perms.size() << " permissions of type " << permissionTypeToString(type));
    return perms;
}

void PermissionManager::clearPermissions(const std::string& moduleId) {
    LOGI_FMT("Clearing all permissions for module: " << moduleId);
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = modulePermissions_.find(moduleId);
    if (it != modulePermissions_.end()) {
        size_t count = it->second.size();
        it->second.clear();
        modulePermissions_.erase(it);
        LOGI_FMT("Cleared " << count << " permissions for module '" << moduleId << "'");
    } else {
        LOGD_FMT("Module '" << moduleId << "' has no permissions to clear");
    }
}

// ========== Default Permissions ==========

void PermissionManager::setDefaultPermissions(
    const std::vector<std::shared_ptr<Permission>>& permissions) {
    LOGI_FMT("Setting default permissions (count: " << permissions.size() << ")");
    std::lock_guard<std::mutex> lock(mutex_);
    defaultPermissions_ = permissions;
    for (const auto& perm : defaultPermissions_) {
        LOGD_FMT("  Default permission: " << perm->toString());
    }
}

std::vector<std::shared_ptr<Permission>> PermissionManager::getDefaultPermissions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    LOGD_FMT("Getting default permissions (count: " << defaultPermissions_.size() << ")");
    return defaultPermissions_;
}

void PermissionManager::applyDefaultPermissions(const std::string& moduleId) {
    if (moduleId.empty()) {
        LOGW("Cannot apply default permissions: empty moduleId");
        return;
    }

    LOGI_FMT("Applying default permissions to module: " << moduleId);
    std::lock_guard<std::mutex> lock(mutex_);
    auto& collection = getOrCreateCollection(moduleId);

    for (const auto& perm : defaultPermissions_) {
        collection.add(perm);
    }
    LOGI_FMT("Applied " << defaultPermissions_.size() << " default permissions to module '" << moduleId << "'");
}

// ========== Configuration Persistence ==========

bool PermissionManager::loadPermissionsFromConfig(const std::string& configPath) {
    LOGI_FMT("Loading permissions from config: " << configPath);
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open permissions config file: " << configPath);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    modulePermissions_.clear();

    std::string line;
    std::string currentModule;
    int lineNum = 0;
    int permissionsLoaded = 0;

    while (std::getline(file, line)) {
        lineNum++;
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Module declaration: [module_id]
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            currentModule = line.substr(1, line.length() - 2);
            LOGD_FMT("Loading permissions for module: " << currentModule);
            continue;
        }

        // Permission declaration: TYPE:TARGET:ACTION
        if (!currentModule.empty()) {
            try {
                auto permission = Permission::fromString(line);
                auto& collection = getOrCreateCollection(currentModule);
                collection.add(permission);
                permissionsLoaded++;
            } catch (const std::exception& e) {
                LOGW_FMT("Skipping invalid permission at line " << lineNum << ": " << line << " (error: " << e.what() << ")");
                continue;
            }
        }
    }

    file.close();
    LOGI_FMT("Successfully loaded " << permissionsLoaded << " permissions for " << modulePermissions_.size() << " modules");
    return true;
}

bool PermissionManager::savePermissionsToConfig(const std::string& configPath) const {
    LOGI_FMT("Saving permissions to config: " << configPath);
    std::ofstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open config file for writing: " << configPath);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Write header
    file << "# CDMF Permission Configuration\n";
    file << "# Format: [module_id] followed by permissions (TYPE:TARGET:ACTION)\n\n";

    // Write default permissions
    file << "[DEFAULT]\n";
    for (const auto& perm : defaultPermissions_) {
        file << perm->toString() << "\n";
    }
    file << "\n";
    LOGD_FMT("Saved " << defaultPermissions_.size() << " default permissions");

    // Write module permissions
    int totalPerms = 0;
    for (const auto& pair : modulePermissions_) {
        file << "[" << pair.first << "]\n";
        auto perms = pair.second.getPermissions();
        for (const auto& perm : perms) {
            file << perm->toString() << "\n";
        }
        file << "\n";
        totalPerms += perms.size();
        LOGD_FMT("Saved " << perms.size() << " permissions for module '" << pair.first << "'");
    }

    file.close();
    LOGI_FMT("Successfully saved " << totalPerms << " permissions for " << modulePermissions_.size() << " modules to " << configPath);
    return true;
}

// ========== Module Management ==========

std::vector<std::string> PermissionManager::getAllModuleIds() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> ids;
    ids.reserve(modulePermissions_.size());

    for (const auto& pair : modulePermissions_) {
        ids.push_back(pair.first);
    }

    LOGD_FMT("Retrieved " << ids.size() << " module IDs from permission registry");
    return ids;
}

bool PermissionManager::hasModule(const std::string& moduleId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    bool exists = modulePermissions_.find(moduleId) != modulePermissions_.end();
    LOGD_FMT("Module '" << moduleId << "' exists in registry: " << (exists ? "yes" : "no"));
    return exists;
}

void PermissionManager::reset() {
    LOGW("Resetting PermissionManager - clearing all module permissions");
    std::lock_guard<std::mutex> lock(mutex_);
    size_t moduleCount = modulePermissions_.size();
    modulePermissions_.clear();
    LOGI_FMT("PermissionManager reset complete. Cleared permissions for " << moduleCount << " modules");
}

// ========== Private Methods ==========

PermissionCollection& PermissionManager::getOrCreateCollection(const std::string& moduleId) {
    // Note: This method assumes mutex is already locked by the caller
    LOGD_FMT("getOrCreateCollection for module: " << moduleId);
    auto it = modulePermissions_.find(moduleId);
    if (it == modulePermissions_.end()) {
        LOGD_FMT("  Creating new permission collection for module: " << moduleId);
        modulePermissions_[moduleId] = PermissionCollection();
        return modulePermissions_[moduleId];
    }
    LOGD_FMT("  Found existing permission collection with " << it->second.size() << " permissions");
    return it->second;
}

} // namespace security
} // namespace cdmf
