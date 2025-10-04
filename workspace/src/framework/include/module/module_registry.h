#ifndef CDMF_MODULE_REGISTRY_H
#define CDMF_MODULE_REGISTRY_H

#include "module/module.h"
#include "utils/version.h"
#include "utils/version_range.h"
#include <string>
#include <vector>
#include <map>
#include <shared_mutex>
#include <memory>

namespace cdmf {

/**
 * @brief Central registry for all installed modules
 *
 * Maintains a collection of all modules installed in the framework.
 * Provides lookup by ID, symbolic name, and version queries.
 *
 * Thread safety:
 * - All methods are thread-safe
 * - Uses shared_mutex for concurrent reads
 *
 * Responsibilities:
 * - Register/unregister modules
 * - Lookup modules by ID or name
 * - Find modules matching version ranges
 * - Track module lifecycle states
 */
class ModuleRegistry {
public:
    /**
     * @brief Constructor
     */
    ModuleRegistry();

    /**
     * @brief Destructor
     */
    ~ModuleRegistry();

    // Prevent copying
    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    /**
     * @brief Register a module
     *
     * @param module Module to register (not owned, must remain valid)
     * @throws std::invalid_argument if module is null
     * @throws std::runtime_error if module ID already registered
     */
    void registerModule(Module* module);

    /**
     * @brief Unregister a module
     *
     * @param moduleId Module ID to unregister
     * @return true if module was unregistered, false if not found
     */
    bool unregisterModule(uint64_t moduleId);

    /**
     * @brief Get module by ID
     *
     * @param moduleId Module ID
     * @return Pointer to module, or nullptr if not found
     */
    Module* getModule(uint64_t moduleId) const;

    /**
     * @brief Get module by symbolic name
     *
     * If multiple versions exist, returns the highest version.
     *
     * @param symbolicName Module symbolic name
     * @return Pointer to module, or nullptr if not found
     */
    Module* getModule(const std::string& symbolicName) const;

    /**
     * @brief Get module by symbolic name and version
     *
     * @param symbolicName Module symbolic name
     * @param version Exact version
     * @return Pointer to module, or nullptr if not found
     */
    Module* getModule(const std::string& symbolicName, const Version& version) const;

    /**
     * @brief Get all modules with a given symbolic name
     *
     * @param symbolicName Module symbolic name
     * @return Vector of matching modules (sorted by version, highest first)
     */
    std::vector<Module*> getModules(const std::string& symbolicName) const;

    /**
     * @brief Get all installed modules
     *
     * @return Vector of all modules
     */
    std::vector<Module*> getAllModules() const;

    /**
     * @brief Find best matching module for dependency
     *
     * Searches for modules matching symbolicName and versionRange,
     * returns the highest version that matches.
     *
     * @param symbolicName Module symbolic name
     * @param versionRange Version range constraint
     * @return Pointer to best matching module, or nullptr if none found
     */
    Module* findCompatibleModule(const std::string& symbolicName,
                                  const VersionRange& versionRange) const;

    /**
     * @brief Find all matching modules for dependency
     *
     * @param symbolicName Module symbolic name
     * @param versionRange Version range constraint
     * @return Vector of matching modules (sorted by version, highest first)
     */
    std::vector<Module*> findCompatibleModules(const std::string& symbolicName,
                                               const VersionRange& versionRange) const;

    /**
     * @brief Get number of registered modules
     * @return Module count
     */
    size_t getModuleCount() const;

    /**
     * @brief Get modules in a specific state
     *
     * @param state Module state to filter by
     * @return Vector of modules in the specified state
     */
    std::vector<Module*> getModulesByState(ModuleState state) const;

    /**
     * @brief Check if a module is registered
     *
     * @param moduleId Module ID
     * @return true if registered
     */
    bool contains(uint64_t moduleId) const;

    /**
     * @brief Generate next unique module ID
     *
     * @return Next available module ID
     */
    uint64_t generateModuleId();

private:
    // Module storage
    std::map<uint64_t, Module*> modulesById_;
    std::map<std::string, std::vector<Module*>> modulesByName_;

    // Module ID generator
    uint64_t nextModuleId_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    /**
     * @brief Sort modules by version (highest first)
     * @param modules Vector to sort
     */
    void sortByVersion(std::vector<Module*>& modules) const;
};

} // namespace cdmf

#endif // CDMF_MODULE_REGISTRY_H
