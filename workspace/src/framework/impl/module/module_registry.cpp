#include "module/module_registry.h"
#include <algorithm>
#include <stdexcept>
#include <mutex>

namespace cdmf {

ModuleRegistry::ModuleRegistry()
    : modulesById_()
    , modulesByName_()
    , nextModuleId_(1)
    , mutex_() {
}

ModuleRegistry::~ModuleRegistry() {
    // Modules are not owned by registry, so no cleanup needed
}

void ModuleRegistry::registerModule(Module* module) {
    if (!module) {
        throw std::invalid_argument("Cannot register null module");
    }

    std::unique_lock lock(mutex_);

    uint64_t moduleId = module->getModuleId();

    // Check if already registered
    if (modulesById_.find(moduleId) != modulesById_.end()) {
        throw std::runtime_error("Module ID already registered: " +
                                std::to_string(moduleId));
    }

    // Add to ID map
    modulesById_[moduleId] = module;

    // Add to name map
    std::string symbolicName = module->getSymbolicName();
    modulesByName_[symbolicName].push_back(module);

    // Sort by version (highest first)
    sortByVersion(modulesByName_[symbolicName]);
}

bool ModuleRegistry::unregisterModule(uint64_t moduleId) {
    std::unique_lock lock(mutex_);

    auto it = modulesById_.find(moduleId);
    if (it == modulesById_.end()) {
        return false;
    }

    Module* module = it->second;
    std::string symbolicName = module->getSymbolicName();

    // Remove from ID map
    modulesById_.erase(it);

    // Remove from name map
    auto& nameVec = modulesByName_[symbolicName];
    nameVec.erase(
        std::remove(nameVec.begin(), nameVec.end(), module),
        nameVec.end()
    );

    // Remove empty entries
    if (nameVec.empty()) {
        modulesByName_.erase(symbolicName);
    }

    return true;
}

Module* ModuleRegistry::getModule(uint64_t moduleId) const {
    std::shared_lock lock(mutex_);

    auto it = modulesById_.find(moduleId);
    if (it != modulesById_.end()) {
        return it->second;
    }

    return nullptr;
}

Module* ModuleRegistry::getModule(const std::string& symbolicName) const {
    std::shared_lock lock(mutex_);

    auto it = modulesByName_.find(symbolicName);
    if (it != modulesByName_.end() && !it->second.empty()) {
        // Return highest version (first in sorted vector)
        return it->second[0];
    }

    return nullptr;
}

Module* ModuleRegistry::getModule(const std::string& symbolicName, const Version& version) const {
    std::shared_lock lock(mutex_);

    auto it = modulesByName_.find(symbolicName);
    if (it == modulesByName_.end()) {
        return nullptr;
    }

    for (Module* module : it->second) {
        if (module->getVersion() == version) {
            return module;
        }
    }

    return nullptr;
}

std::vector<Module*> ModuleRegistry::getModules(const std::string& symbolicName) const {
    std::shared_lock lock(mutex_);

    auto it = modulesByName_.find(symbolicName);
    if (it != modulesByName_.end()) {
        return it->second;
    }

    return {};
}

std::vector<Module*> ModuleRegistry::getAllModules() const {
    std::shared_lock lock(mutex_);

    std::vector<Module*> modules;
    modules.reserve(modulesById_.size());

    for (const auto& pair : modulesById_) {
        modules.push_back(pair.second);
    }

    return modules;
}

Module* ModuleRegistry::findCompatibleModule(const std::string& symbolicName,
                                             const VersionRange& versionRange) const {
    std::shared_lock lock(mutex_);

    auto it = modulesByName_.find(symbolicName);
    if (it == modulesByName_.end()) {
        return nullptr;
    }

    // Modules are sorted by version (highest first)
    // Return first matching module
    for (Module* module : it->second) {
        if (versionRange.includes(module->getVersion())) {
            return module;
        }
    }

    return nullptr;
}

std::vector<Module*> ModuleRegistry::findCompatibleModules(const std::string& symbolicName,
                                                           const VersionRange& versionRange) const {
    std::shared_lock lock(mutex_);

    std::vector<Module*> matching;

    auto it = modulesByName_.find(symbolicName);
    if (it == modulesByName_.end()) {
        return matching;
    }

    for (Module* module : it->second) {
        if (versionRange.includes(module->getVersion())) {
            matching.push_back(module);
        }
    }

    return matching;
}

size_t ModuleRegistry::getModuleCount() const {
    std::shared_lock lock(mutex_);
    return modulesById_.size();
}

std::vector<Module*> ModuleRegistry::getModulesByState(ModuleState state) const {
    std::shared_lock lock(mutex_);

    std::vector<Module*> modules;

    for (const auto& pair : modulesById_) {
        if (pair.second->getState() == state) {
            modules.push_back(pair.second);
        }
    }

    return modules;
}

bool ModuleRegistry::contains(uint64_t moduleId) const {
    std::shared_lock lock(mutex_);
    return modulesById_.find(moduleId) != modulesById_.end();
}

uint64_t ModuleRegistry::generateModuleId() {
    std::unique_lock lock(mutex_);
    return nextModuleId_++;
}

void ModuleRegistry::sortByVersion(std::vector<Module*>& modules) const {
    std::sort(modules.begin(), modules.end(),
        [](const Module* a, const Module* b) {
            return a->getVersion() > b->getVersion(); // Higher version first
        });
}

} // namespace cdmf
