#ifndef CDMF_RESOURCE_LIMITER_H
#define CDMF_RESOURCE_LIMITER_H

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <cstdint>

namespace cdmf {
namespace security {

/**
 * @enum ResourceType
 * @brief Types of resources that can be limited
 */
enum class ResourceType {
    MEMORY,              ///< Memory usage in bytes
    CPU_TIME,            ///< CPU time in milliseconds
    FILE_DESCRIPTORS,    ///< Number of open file descriptors
    THREADS,             ///< Number of threads
    NETWORK_BANDWIDTH,   ///< Network bandwidth in bytes/sec
    DISK_IO,             ///< Disk I/O in bytes/sec
    PROCESS_COUNT        ///< Number of processes
};

/**
 * @struct ResourceLimit
 * @brief Resource limit specification
 */
struct ResourceLimit {
    ResourceType type;
    uint64_t softLimit;  ///< Soft limit (warning threshold)
    uint64_t hardLimit;  ///< Hard limit (enforcement threshold)
    bool enabled;
};

/**
 * @struct ResourceUsage
 * @brief Current resource usage
 */
struct ResourceUsage {
    ResourceType type;
    uint64_t currentUsage;
    uint64_t peakUsage;
    uint64_t softLimit;
    uint64_t hardLimit;
    bool isExceeded;     ///< Has exceeded soft limit
    bool isViolated;     ///< Has exceeded hard limit
};

/**
 * @class ResourceLimiter
 * @brief Enforces resource limits on modules
 *
 * The ResourceLimiter monitors and enforces resource consumption limits
 * for modules to prevent resource exhaustion and ensure fair sharing.
 */
class ResourceLimiter {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the ResourceLimiter instance
     */
    static ResourceLimiter& getInstance();

    /**
     * @brief Delete copy constructor
     */
    ResourceLimiter(const ResourceLimiter&) = delete;

    /**
     * @brief Delete assignment operator
     */
    ResourceLimiter& operator=(const ResourceLimiter&) = delete;

    /**
     * @brief Set resource limit for a module
     * @param moduleId The module ID
     * @param limit The resource limit
     * @return true if set successfully
     */
    bool setResourceLimit(const std::string& moduleId, const ResourceLimit& limit);

    /**
     * @brief Get resource limit for a module
     * @param moduleId The module ID
     * @param type The resource type
     * @return ResourceLimit or null if not set
     */
    std::shared_ptr<ResourceLimit> getResourceLimit(const std::string& moduleId,
                                                     ResourceType type) const;

    /**
     * @brief Remove resource limit
     * @param moduleId The module ID
     * @param type The resource type
     * @return true if removed successfully
     */
    bool removeResourceLimit(const std::string& moduleId, ResourceType type);

    /**
     * @brief Get all resource limits for a module
     * @param moduleId The module ID
     * @return Vector of resource limits
     */
    std::vector<ResourceLimit> getResourceLimits(const std::string& moduleId) const;

    /**
     * @brief Record resource usage
     * @param moduleId The module ID
     * @param type The resource type
     * @param usage The usage amount
     * @return true if recorded successfully
     */
    bool recordUsage(const std::string& moduleId, ResourceType type, uint64_t usage);

    /**
     * @brief Get current resource usage
     * @param moduleId The module ID
     * @param type The resource type
     * @return ResourceUsage
     */
    ResourceUsage getResourceUsage(const std::string& moduleId, ResourceType type) const;

    /**
     * @brief Get all resource usage for a module
     * @param moduleId The module ID
     * @return Vector of resource usage
     */
    std::vector<ResourceUsage> getAllResourceUsage(const std::string& moduleId) const;

    /**
     * @brief Check if a module can allocate resources
     * @param moduleId The module ID
     * @param type The resource type
     * @param amount The amount to allocate
     * @return true if allocation is allowed
     */
    bool canAllocate(const std::string& moduleId, ResourceType type, uint64_t amount) const;

    /**
     * @brief Reset resource usage for a module
     * @param moduleId The module ID
     * @param type The resource type (or all if not specified)
     * @return true if reset successfully
     */
    bool resetUsage(const std::string& moduleId, ResourceType type);

    /**
     * @brief Enable or disable resource limiting globally
     * @param enabled true to enable
     */
    void setLimitingEnabled(bool enabled);

    /**
     * @brief Check if resource limiting is enabled
     * @return true if enabled
     */
    bool isLimitingEnabled() const;

    /**
     * @brief Set default resource limits
     * @param limits Vector of default limits
     */
    void setDefaultLimits(const std::vector<ResourceLimit>& limits);

    /**
     * @brief Get default resource limits
     * @return Vector of default limits
     */
    std::vector<ResourceLimit> getDefaultLimits() const;

    /**
     * @brief Apply default limits to a module
     * @param moduleId The module ID
     */
    void applyDefaultLimits(const std::string& moduleId);

    /**
     * @brief Load resource limits from configuration
     * @param configPath Path to configuration file
     * @return true if loaded successfully
     */
    bool loadLimits(const std::string& configPath);

    /**
     * @brief Save resource limits to configuration
     * @param configPath Path to configuration file
     * @return true if saved successfully
     */
    bool saveLimits(const std::string& configPath) const;

    /**
     * @brief Convert ResourceType to string
     * @param type The resource type
     * @return String representation
     */
    static std::string resourceTypeToString(ResourceType type);

    /**
     * @brief Convert string to ResourceType
     * @param str The string representation
     * @return ResourceType
     */
    static ResourceType stringToResourceType(const std::string& str);

private:
    /**
     * @brief Private constructor for singleton
     */
    ResourceLimiter();

    /**
     * @brief Destructor
     */
    ~ResourceLimiter();

    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace security
} // namespace cdmf

#endif // CDMF_RESOURCE_LIMITER_H
