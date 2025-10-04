#include "security/resource_limiter.h"
#include "utils/log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace cdmf {
namespace security {

// ========== Helper Functions ==========

std::string ResourceLimiter::resourceTypeToString(ResourceType type) {
    std::string result;
    switch (type) {
        case ResourceType::MEMORY: result = "MEMORY"; break;
        case ResourceType::CPU_TIME: result = "CPU_TIME"; break;
        case ResourceType::FILE_DESCRIPTORS: result = "FILE_DESCRIPTORS"; break;
        case ResourceType::THREADS: result = "THREADS"; break;
        case ResourceType::NETWORK_BANDWIDTH: result = "NETWORK_BANDWIDTH"; break;
        case ResourceType::DISK_IO: result = "DISK_IO"; break;
        case ResourceType::PROCESS_COUNT: result = "PROCESS_COUNT"; break;
        default: result = "UNKNOWN"; break;
    }
    LOGD_FMT("resourceTypeToString(" << static_cast<int>(type) << ") -> " << result);
    return result;
}

ResourceType ResourceLimiter::stringToResourceType(const std::string& str) {
    LOGD_FMT("stringToResourceType('" << str << "')");
    if (str == "MEMORY") return ResourceType::MEMORY;
    if (str == "CPU_TIME") return ResourceType::CPU_TIME;
    if (str == "FILE_DESCRIPTORS") return ResourceType::FILE_DESCRIPTORS;
    if (str == "THREADS") return ResourceType::THREADS;
    if (str == "NETWORK_BANDWIDTH") return ResourceType::NETWORK_BANDWIDTH;
    if (str == "DISK_IO") return ResourceType::DISK_IO;
    if (str == "PROCESS_COUNT") return ResourceType::PROCESS_COUNT;
    LOGE_FMT("Invalid resource type: " << str);
    throw std::invalid_argument("Invalid resource type: " + str);
}

// ========== ResourceLimiter::Impl ==========

class ResourceLimiter::Impl {
public:
    // Module resource limits: moduleId -> (ResourceType -> ResourceLimit)
    std::map<std::string, std::map<ResourceType, ResourceLimit>> moduleLimits;

    // Module resource usage: moduleId -> (ResourceType -> usage data)
    std::map<std::string, std::map<ResourceType, ResourceUsage>> moduleUsage;

    // Default resource limits
    std::vector<ResourceLimit> defaultLimits;

    bool limitingEnabled = true;
    mutable std::mutex mutex;

    Impl() {
        // Initialize default limits
        ResourceLimit memLimit{ResourceType::MEMORY, 256 * 1024 * 1024, 512 * 1024 * 1024, true};
        ResourceLimit cpuLimit{ResourceType::CPU_TIME, 5000, 10000, true};
        ResourceLimit fdLimit{ResourceType::FILE_DESCRIPTORS, 128, 256, true};
        ResourceLimit threadLimit{ResourceType::THREADS, 10, 20, true};

        defaultLimits = {memLimit, cpuLimit, fdLimit, threadLimit};
    }
};

// ========== ResourceLimiter Singleton ==========

ResourceLimiter& ResourceLimiter::getInstance() {
    static ResourceLimiter instance;
    return instance;
}

ResourceLimiter::ResourceLimiter() : pImpl_(std::make_unique<Impl>()) {
    LOGI("ResourceLimiter initialized");
    LOGI_FMT("Default limits: Memory=" << pImpl_->defaultLimits[0].hardLimit / (1024*1024)
             << "MB, CPU=" << pImpl_->defaultLimits[1].hardLimit << "ms, FD="
             << pImpl_->defaultLimits[2].hardLimit << ", Threads=" << pImpl_->defaultLimits[3].hardLimit);
}

ResourceLimiter::~ResourceLimiter() = default;

// ========== Resource Limit Management ==========

bool ResourceLimiter::setResourceLimit(const std::string& moduleId,
                                      const ResourceLimit& limit) {
    if (moduleId.empty()) {
        LOGW("Cannot set resource limit: empty moduleId");
        return false;
    }

    LOGI_FMT("Setting resource limit for module '" << moduleId << "': "
             << resourceTypeToString(limit.type) << " soft=" << limit.softLimit
             << " hard=" << limit.hardLimit << " enabled=" << limit.enabled);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->moduleLimits[moduleId][limit.type] = limit;

    // Initialize usage tracking if not exists
    if (pImpl_->moduleUsage[moduleId].find(limit.type) == pImpl_->moduleUsage[moduleId].end()) {
        ResourceUsage usage;
        usage.type = limit.type;
        usage.currentUsage = 0;
        usage.peakUsage = 0;
        usage.softLimit = limit.softLimit;
        usage.hardLimit = limit.hardLimit;
        usage.isExceeded = false;
        usage.isViolated = false;
        pImpl_->moduleUsage[moduleId][limit.type] = usage;
    }

    return true;
}

std::shared_ptr<ResourceLimit> ResourceLimiter::getResourceLimit(
    const std::string& moduleId, ResourceType type) const {
    LOGD_FMT("Getting resource limit for module '" << moduleId << "': " << resourceTypeToString(type));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto modIt = pImpl_->moduleLimits.find(moduleId);
    if (modIt == pImpl_->moduleLimits.end()) {
        LOGD_FMT("  Module '" << moduleId << "' not found");
        return nullptr;
    }

    auto typeIt = modIt->second.find(type);
    if (typeIt == modIt->second.end()) {
        LOGD_FMT("  Resource type " << resourceTypeToString(type) << " not found for module '" << moduleId << "'");
        return nullptr;
    }

    LOGD_FMT("  Found limit: soft=" << typeIt->second.softLimit << ", hard=" << typeIt->second.hardLimit);
    return std::make_shared<ResourceLimit>(typeIt->second);
}

bool ResourceLimiter::removeResourceLimit(const std::string& moduleId, ResourceType type) {
    LOGI_FMT("Removing resource limit for module '" << moduleId << "': " << resourceTypeToString(type));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto modIt = pImpl_->moduleLimits.find(moduleId);
    if (modIt == pImpl_->moduleLimits.end()) {
        LOGD_FMT("  Module '" << moduleId << "' not found");
        return false;
    }

    bool removed = modIt->second.erase(type) > 0;
    LOGD_FMT("  Resource limit removal " << (removed ? "succeeded" : "failed (type not found)"));
    return removed;
}

std::vector<ResourceLimit> ResourceLimiter::getResourceLimits(const std::string& moduleId) const {
    LOGD_FMT("Getting all resource limits for module: " << moduleId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    std::vector<ResourceLimit> limits;
    auto modIt = pImpl_->moduleLimits.find(moduleId);
    if (modIt != pImpl_->moduleLimits.end()) {
        for (const auto& pair : modIt->second) {
            limits.push_back(pair.second);
        }
    }
    LOGD_FMT("  Found " << limits.size() << " resource limits for module '" << moduleId << "'");
    return limits;
}

// ========== Resource Usage Tracking ==========

bool ResourceLimiter::recordUsage(const std::string& moduleId, ResourceType type,
                                 uint64_t usage) {
    if (moduleId.empty() || !pImpl_->limitingEnabled) {
        return false;
    }

    LOGD_FMT("Recording resource usage for '" << moduleId << "': "
             << resourceTypeToString(type) << "=" << usage);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    // Get or create usage entry
    auto& usageEntry = pImpl_->moduleUsage[moduleId][type];
    usageEntry.type = type;
    usageEntry.currentUsage = usage;

    // Update peak usage
    if (usage > usageEntry.peakUsage) {
        usageEntry.peakUsage = usage;
        LOGD_FMT("  New peak usage for " << resourceTypeToString(type) << ": " << usage);
    }

    // Check limits
    auto limitIt = pImpl_->moduleLimits[moduleId].find(type);
    if (limitIt != pImpl_->moduleLimits[moduleId].end()) {
        const auto& limit = limitIt->second;
        usageEntry.softLimit = limit.softLimit;
        usageEntry.hardLimit = limit.hardLimit;
        usageEntry.isExceeded = (usage >= limit.softLimit);
        usageEntry.isViolated = (usage >= limit.hardLimit);

        if (usageEntry.isViolated) {
            LOGW_FMT("Resource limit VIOLATED for '" << moduleId << "': "
                     << resourceTypeToString(type) << " usage=" << usage
                     << " exceeds hard limit=" << limit.hardLimit);
        } else if (usageEntry.isExceeded) {
            LOGW_FMT("Resource limit EXCEEDED (soft) for '" << moduleId << "': "
                     << resourceTypeToString(type) << " usage=" << usage
                     << " exceeds soft limit=" << limit.softLimit);
        }
    }

    return true;
}

ResourceUsage ResourceLimiter::getResourceUsage(const std::string& moduleId,
                                               ResourceType type) const {
    LOGD_FMT("Getting resource usage for module '" << moduleId << "': " << resourceTypeToString(type));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    ResourceUsage usage;
    usage.type = type;
    usage.currentUsage = 0;
    usage.peakUsage = 0;
    usage.softLimit = 0;
    usage.hardLimit = 0;
    usage.isExceeded = false;
    usage.isViolated = false;

    auto modIt = pImpl_->moduleUsage.find(moduleId);
    if (modIt != pImpl_->moduleUsage.end()) {
        auto typeIt = modIt->second.find(type);
        if (typeIt != modIt->second.end()) {
            usage = typeIt->second;
            LOGD_FMT("  Current usage=" << usage.currentUsage << ", peak=" << usage.peakUsage
                     << ", exceeded=" << usage.isExceeded << ", violated=" << usage.isViolated);
        } else {
            LOGD_FMT("  No usage data for resource type " << resourceTypeToString(type));
        }
    } else {
        LOGD_FMT("  No usage data for module '" << moduleId << "'");
    }

    return usage;
}

std::vector<ResourceUsage> ResourceLimiter::getAllResourceUsage(
    const std::string& moduleId) const {
    LOGD_FMT("Getting all resource usage for module: " << moduleId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    std::vector<ResourceUsage> usages;
    auto modIt = pImpl_->moduleUsage.find(moduleId);
    if (modIt != pImpl_->moduleUsage.end()) {
        for (const auto& pair : modIt->second) {
            usages.push_back(pair.second);
        }
    }
    LOGD_FMT("  Found " << usages.size() << " resource usage entries for module '" << moduleId << "'");
    return usages;
}

bool ResourceLimiter::canAllocate(const std::string& moduleId, ResourceType type,
                                 uint64_t amount) const {
    if (!pImpl_->limitingEnabled) {
        return true;
    }

    LOGD_FMT("Checking if module '" << moduleId << "' can allocate "
             << amount << " units of " << resourceTypeToString(type));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    // Check if limit is set
    auto modLimitIt = pImpl_->moduleLimits.find(moduleId);
    if (modLimitIt == pImpl_->moduleLimits.end()) {
        LOGD("  No limits set for module, allocation allowed");
        return true; // No limits set
    }

    auto typeLimitIt = modLimitIt->second.find(type);
    if (typeLimitIt == modLimitIt->second.end() || !typeLimitIt->second.enabled) {
        LOGD("  No limit for this resource type or disabled, allocation allowed");
        return true; // No limit for this type or disabled
    }

    // Get current usage
    uint64_t currentUsage = 0;
    auto modUsageIt = pImpl_->moduleUsage.find(moduleId);
    if (modUsageIt != pImpl_->moduleUsage.end()) {
        auto typeUsageIt = modUsageIt->second.find(type);
        if (typeUsageIt != modUsageIt->second.end()) {
            currentUsage = typeUsageIt->second.currentUsage;
        }
    }

    // Check if allocation would exceed hard limit
    bool canAlloc = (currentUsage + amount) < typeLimitIt->second.hardLimit;
    LOGD_FMT("  Current=" << currentUsage << " + Amount=" << amount
             << " vs HardLimit=" << typeLimitIt->second.hardLimit
             << " -> " << (canAlloc ? "ALLOWED" : "DENIED"));
    return canAlloc;
}

bool ResourceLimiter::resetUsage(const std::string& moduleId, ResourceType type) {
    LOGI_FMT("Resetting resource usage for module '" << moduleId << "': " << resourceTypeToString(type));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto modIt = pImpl_->moduleUsage.find(moduleId);
    if (modIt == pImpl_->moduleUsage.end()) {
        LOGD_FMT("  Module '" << moduleId << "' not found");
        return false;
    }

    auto typeIt = modIt->second.find(type);
    if (typeIt != modIt->second.end()) {
        typeIt->second.currentUsage = 0;
        typeIt->second.peakUsage = 0;
        typeIt->second.isExceeded = false;
        typeIt->second.isViolated = false;
        LOGD_FMT("  Resource usage reset successfully for " << resourceTypeToString(type));
        return true;
    }

    LOGD_FMT("  Resource type " << resourceTypeToString(type) << " not found");
    return false;
}

// ========== Configuration ==========

void ResourceLimiter::setLimitingEnabled(bool enabled) {
    LOGI_FMT("Setting resource limiting enabled: " << (enabled ? "true" : "false"));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->limitingEnabled = enabled;
    LOGD_FMT("Resource limiting is now " << (enabled ? "enabled" : "disabled"));
}

bool ResourceLimiter::isLimitingEnabled() const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    LOGD_FMT("Checking if resource limiting is enabled: " << (pImpl_->limitingEnabled ? "yes" : "no"));
    return pImpl_->limitingEnabled;
}

void ResourceLimiter::setDefaultLimits(const std::vector<ResourceLimit>& limits) {
    LOGI_FMT("Setting default resource limits (count: " << limits.size() << ")");
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->defaultLimits = limits;
    for (const auto& limit : limits) {
        LOGD_FMT("  Default limit: " << resourceTypeToString(limit.type)
                 << " soft=" << limit.softLimit << " hard=" << limit.hardLimit);
    }
}

std::vector<ResourceLimit> ResourceLimiter::getDefaultLimits() const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    LOGD_FMT("Getting default resource limits (count: " << pImpl_->defaultLimits.size() << ")");
    return pImpl_->defaultLimits;
}

void ResourceLimiter::applyDefaultLimits(const std::string& moduleId) {
    if (moduleId.empty()) {
        LOGW("Cannot apply default limits: empty moduleId");
        return;
    }

    LOGI_FMT("Applying default resource limits to module: " << moduleId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    for (const auto& limit : pImpl_->defaultLimits) {
        pImpl_->moduleLimits[moduleId][limit.type] = limit;

        // Initialize usage tracking
        ResourceUsage usage;
        usage.type = limit.type;
        usage.currentUsage = 0;
        usage.peakUsage = 0;
        usage.softLimit = limit.softLimit;
        usage.hardLimit = limit.hardLimit;
        usage.isExceeded = false;
        usage.isViolated = false;
        pImpl_->moduleUsage[moduleId][limit.type] = usage;
    }
    LOGI_FMT("Applied " << pImpl_->defaultLimits.size() << " default limits to module '" << moduleId << "'");
}

// ========== Persistence ==========

bool ResourceLimiter::loadLimits(const std::string& configPath) {
    LOGI_FMT("Loading resource limits from: " << configPath);
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open resource limits config file: " << configPath);
        return false;
    }

    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->moduleLimits.clear();

    std::string line;
    std::string currentModule;
    int limitsLoaded = 0;

    while (std::getline(file, line)) {
        // Trim and skip comments
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Module declaration
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            currentModule = line.substr(1, line.length() - 2);
            LOGD_FMT("Loading limits for module: " << currentModule);
            continue;
        }

        // Limit: TYPE:SOFT:HARD:ENABLED
        if (!currentModule.empty()) {
            std::stringstream ss(line);
            std::string typeStr, softStr, hardStr, enabledStr;

            std::getline(ss, typeStr, ':');
            std::getline(ss, softStr, ':');
            std::getline(ss, hardStr, ':');
            std::getline(ss, enabledStr, ':');

            try {
                ResourceLimit limit;
                limit.type = stringToResourceType(typeStr);
                limit.softLimit = std::stoull(softStr);
                limit.hardLimit = std::stoull(hardStr);
                limit.enabled = (enabledStr == "true" || enabledStr == "1");

                pImpl_->moduleLimits[currentModule][limit.type] = limit;
                limitsLoaded++;
            } catch (...) {
                LOGW_FMT("Skipping invalid resource limit line: " << line);
            }
        }
    }

    file.close();
    LOGI_FMT("Successfully loaded " << limitsLoaded << " resource limits for " << pImpl_->moduleLimits.size() << " modules");
    return true;
}

bool ResourceLimiter::saveLimits(const std::string& configPath) const {
    LOGI_FMT("Saving resource limits to: " << configPath);
    std::ofstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open file for writing resource limits: " << configPath);
        return false;
    }

    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    file << "# CDMF Resource Limits Configuration\n";
    file << "# Format: TYPE:SOFT_LIMIT:HARD_LIMIT:ENABLED\n\n";

    int totalLimits = 0;
    for (const auto& modPair : pImpl_->moduleLimits) {
        file << "[" << modPair.first << "]\n";
        for (const auto& limitPair : modPair.second) {
            const auto& limit = limitPair.second;
            file << resourceTypeToString(limit.type) << ":"
                 << limit.softLimit << ":"
                 << limit.hardLimit << ":"
                 << (limit.enabled ? "true" : "false") << "\n";
            totalLimits++;
        }
        file << "\n";
        LOGD_FMT("Saved " << modPair.second.size() << " limits for module '" << modPair.first << "'");
    }

    file.close();
    LOGI_FMT("Successfully saved " << totalLimits << " resource limits for " << pImpl_->moduleLimits.size() << " modules to " << configPath);
    return true;
}

} // namespace security
} // namespace cdmf
