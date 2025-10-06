#include "security/permission.h"
#include "utils/log.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace cdmf {
namespace security {

// ========== Helper Functions ==========

std::string permissionTypeToString(PermissionType type) {
    switch (type) {
        case PermissionType::SERVICE_GET: return "SERVICE_GET";
        case PermissionType::SERVICE_REGISTER: return "SERVICE_REGISTER";
        case PermissionType::MODULE_LOAD: return "MODULE_LOAD";
        case PermissionType::MODULE_UNLOAD: return "MODULE_UNLOAD";
        case PermissionType::MODULE_EXECUTE: return "MODULE_EXECUTE";
        case PermissionType::FILE_READ: return "FILE_READ";
        case PermissionType::FILE_WRITE: return "FILE_WRITE";
        case PermissionType::NETWORK_CONNECT: return "NETWORK_CONNECT";
        case PermissionType::NETWORK_BIND: return "NETWORK_BIND";
        case PermissionType::IPC_SEND: return "IPC_SEND";
        case PermissionType::IPC_RECEIVE: return "IPC_RECEIVE";
        case PermissionType::PROPERTY_READ: return "PROPERTY_READ";
        case PermissionType::PROPERTY_WRITE: return "PROPERTY_WRITE";
        case PermissionType::EVENT_PUBLISH: return "EVENT_PUBLISH";
        case PermissionType::EVENT_SUBSCRIBE: return "EVENT_SUBSCRIBE";
        case PermissionType::ADMIN: return "ADMIN";
        default: return "UNKNOWN";
    }
}

PermissionType stringToPermissionType(const std::string& str) {
    LOGD_FMT("Converting string to PermissionType: " << str);
    if (str == "SERVICE_GET") return PermissionType::SERVICE_GET;
    if (str == "SERVICE_REGISTER") return PermissionType::SERVICE_REGISTER;
    if (str == "MODULE_LOAD") return PermissionType::MODULE_LOAD;
    if (str == "MODULE_UNLOAD") return PermissionType::MODULE_UNLOAD;
    if (str == "MODULE_EXECUTE") return PermissionType::MODULE_EXECUTE;
    if (str == "FILE_READ") return PermissionType::FILE_READ;
    if (str == "FILE_WRITE") return PermissionType::FILE_WRITE;
    if (str == "NETWORK_CONNECT") return PermissionType::NETWORK_CONNECT;
    if (str == "NETWORK_BIND") return PermissionType::NETWORK_BIND;
    if (str == "IPC_SEND") return PermissionType::IPC_SEND;
    if (str == "IPC_RECEIVE") return PermissionType::IPC_RECEIVE;
    if (str == "PROPERTY_READ") return PermissionType::PROPERTY_READ;
    if (str == "PROPERTY_WRITE") return PermissionType::PROPERTY_WRITE;
    if (str == "EVENT_PUBLISH") return PermissionType::EVENT_PUBLISH;
    if (str == "EVENT_SUBSCRIBE") return PermissionType::EVENT_SUBSCRIBE;
    if (str == "ADMIN") return PermissionType::ADMIN;
    LOGE_FMT("Invalid permission type: " << str);
    throw std::invalid_argument("Invalid permission type: " + str);
}

std::string permissionActionToString(PermissionAction action) {
    switch (action) {
        case PermissionAction::GRANT: return "GRANT";
        case PermissionAction::DENY: return "DENY";
        case PermissionAction::REVOKE: return "REVOKE";
        default: return "UNKNOWN";
    }
}

PermissionAction stringToPermissionAction(const std::string& str) {
    LOGD_FMT("Converting string to PermissionAction: " << str);
    if (str == "GRANT") return PermissionAction::GRANT;
    if (str == "DENY") return PermissionAction::DENY;
    if (str == "REVOKE") return PermissionAction::REVOKE;
    LOGE_FMT("Invalid permission action: " << str);
    throw std::invalid_argument("Invalid permission action: " + str);
}

// ========== Permission Class ==========

Permission::Permission(PermissionType type, const std::string& target, PermissionAction action)
    : type_(type), target_(target), action_(action) {
    LOGD_FMT("Created Permission: " << permissionTypeToString(type_) << ":" << target_ << ":" << permissionActionToString(action_));
}

PermissionType Permission::getType() const {
    LOGV_FMT("Permission::getType() -> " << permissionTypeToString(type_));
    return type_;
}

std::string Permission::getTarget() const {
    LOGV_FMT("Permission::getTarget() -> " << target_);
    return target_;
}

PermissionAction Permission::getAction() const {
    LOGV_FMT("Permission::getAction() -> " << permissionActionToString(action_));
    return action_;
}

bool Permission::implies(const Permission& other) const {
    LOGD_FMT("Checking if " << toString() << " implies " << other.toString());

    // If this is DENY, it doesn't imply anything
    if (action_ == PermissionAction::DENY) {
        LOGD_FMT("  Result: false (DENY doesn't imply anything)");
        return false;
    }

    // If this is ADMIN, it implies everything
    if (type_ == PermissionType::ADMIN) {
        LOGD_FMT("  Result: true (ADMIN implies everything)");
        return true;
    }

    // Types must match
    if (type_ != other.type_) {
        LOGD_FMT("  Result: false (types don't match)");
        return false;
    }

    // Check if our target pattern matches the other's target
    bool result = matchesTarget(other.target_);
    LOGD_FMT("  Result: " << (result ? "true" : "false") << " (target match)");
    return result;
}

bool Permission::matchesTarget(const std::string& target) const {
    LOGD_FMT("Matching pattern '" << target_ << "' against target '" << target << "'");
    bool result = wildcardMatch(target_, target);
    LOGD_FMT("  Match result: " << (result ? "true" : "false"));
    return result;
}

bool Permission::wildcardMatch(const std::string& pattern, const std::string& target) const {
    LOGD_FMT("wildcardMatch: pattern='" << pattern << "' target='" << target << "'");
    size_t p = 0, t = 0;
    size_t starIdx = std::string::npos, matchIdx = 0;

    while (t < target.length()) {
        if (p < pattern.length() && (pattern[p] == target[t] || pattern[p] == '?')) {
            p++;
            t++;
        } else if (p < pattern.length() && pattern[p] == '*') {
            starIdx = p;
            matchIdx = t;
            p++;
        } else if (starIdx != std::string::npos) {
            p = starIdx + 1;
            matchIdx++;
            t = matchIdx;
        } else {
            LOGD("  Wildcard match failed");
            return false;
        }
    }

    while (p < pattern.length() && pattern[p] == '*') {
        p++;
    }

    bool matched = (p == pattern.length());
    LOGD_FMT("  Wildcard match result: " << (matched ? "true" : "false"));
    return matched;
}

std::string Permission::toString() const {
    std::string result = permissionTypeToString(type_) + ":" + target_ + ":" +
           permissionActionToString(action_);
    LOGV_FMT("Permission::toString() -> " << result);
    return result;
}

bool Permission::equals(const Permission& other) const {
    bool eq = type_ == other.type_ &&
           target_ == other.target_ &&
           action_ == other.action_;
    LOGD_FMT("Permission::equals() comparing " << toString() << " with " << other.toString() << " -> " << (eq ? "true" : "false"));
    return eq;
}

std::shared_ptr<Permission> Permission::fromString(const std::string& str) {
    LOGD_FMT("Parsing permission from string: " << str);
    std::stringstream ss(str);
    std::string typeStr, target, actionStr;

    if (!std::getline(ss, typeStr, ':')) {
        LOGE_FMT("Invalid permission string format: " << str);
        throw std::invalid_argument("Invalid permission string format");
    }

    std::getline(ss, target, ':');
    std::getline(ss, actionStr, ':');

    if (target.empty()) {
        target = "*";
        LOGD("Using default target: *");
    }
    if (actionStr.empty()) {
        actionStr = "GRANT";
        LOGD("Using default action: GRANT");
    }

    PermissionType type = stringToPermissionType(typeStr);
    PermissionAction action = stringToPermissionAction(actionStr);

    LOGI_FMT("Created permission from string: " << typeStr << ":" << target << ":" << actionStr);
    return std::make_shared<Permission>(type, target, action);
}

// ========== PermissionCollection Class ==========

void PermissionCollection::add(std::shared_ptr<Permission> permission) {
    if (permission) {
        LOGD_FMT("Adding permission to collection: " << permission->toString() << " (collection size: " << permissions_.size() << ")");
        permissions_.push_back(permission);
    } else {
        LOGW("Attempted to add null permission to collection");
    }
}

bool PermissionCollection::remove(std::shared_ptr<Permission> permission) {
    if (!permission) {
        LOGW("Attempted to remove null permission from collection");
        return false;
    }

    LOGD_FMT("Removing permission from collection: " << permission->toString());
    auto it = std::find_if(permissions_.begin(), permissions_.end(),
        [&permission](const std::shared_ptr<Permission>& p) {
            return p->equals(*permission);
        });

    if (it != permissions_.end()) {
        permissions_.erase(it);
        LOGD_FMT("Permission removed successfully (collection size: " << permissions_.size() << ")");
        return true;
    }
    LOGD("Permission not found in collection");
    return false;
}

bool PermissionCollection::implies(const Permission& permission) const {
    LOGD_FMT("PermissionCollection::implies checking: " << permission.toString() << " (collection size: " << permissions_.size() << ")");

    // Check for explicit DENY first
    for (const auto& p : permissions_) {
        if (p->getAction() == PermissionAction::DENY) {
            // For DENY, check type and target match directly (not using implies() since DENY doesn't imply)
            if (p->getType() == permission.getType() && p->matchesTarget(permission.getTarget())) {
                LOGD_FMT("  Found DENY permission that matches: " << p->toString());
                return false;
            }
        }
    }

    // Check for GRANT
    for (const auto& p : permissions_) {
        if (p->getAction() == PermissionAction::GRANT && p->implies(permission)) {
            LOGD_FMT("  Found GRANT permission that matches: " << p->toString());
            return true;
        }
    }

    LOGD("  No matching permission found in collection");
    return false;
}

std::vector<std::shared_ptr<Permission>> PermissionCollection::getPermissions() const {
    LOGD_FMT("PermissionCollection::getPermissions() returning " << permissions_.size() << " permissions");
    return permissions_;
}

std::vector<std::shared_ptr<Permission>> PermissionCollection::getPermissionsByType(PermissionType type) const {
    LOGD_FMT("Getting permissions by type: " << permissionTypeToString(type));
    std::vector<std::shared_ptr<Permission>> result;
    std::copy_if(permissions_.begin(), permissions_.end(), std::back_inserter(result),
        [type](const std::shared_ptr<Permission>& p) {
            return p->getType() == type;
        });
    LOGD_FMT("Found " << result.size() << " permissions of type " << permissionTypeToString(type));
    return result;
}

void PermissionCollection::clear() {
    LOGD_FMT("Clearing permission collection (current size: " << permissions_.size() << ")");
    permissions_.clear();
}

size_t PermissionCollection::size() const {
    LOGV_FMT("PermissionCollection::size() -> " << permissions_.size());
    return permissions_.size();
}

bool PermissionCollection::empty() const {
    bool isEmpty = permissions_.empty();
    LOGV_FMT("PermissionCollection::empty() -> " << (isEmpty ? "true" : "false"));
    return isEmpty;
}

} // namespace security
} // namespace cdmf
