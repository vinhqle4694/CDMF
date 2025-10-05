#include "module/dependency_resolver.h"
#include "module/manifest_parser.h"
#include "utils/log.h"
#include <algorithm>
#include <stdexcept>

namespace cdmf {

DependencyResolver::DependencyResolver(ModuleRegistry* moduleRegistry)
    : moduleRegistry_(moduleRegistry) {

    if (!moduleRegistry_) {
        throw std::invalid_argument("ModuleRegistry cannot be null");
    }
}

DependencyResolver::~DependencyResolver() {
}

void DependencyResolver::buildGraph() {
    graph_.clear();

    // Get all modules from registry
    auto modules = moduleRegistry_->getAllModules();

    // Add all modules as nodes first
    for (auto* module : modules) {
        graph_.addNode(module->getModuleId(), module->getSymbolicName());
    }

    // Add edges based on dependencies
    for (auto* module : modules) {
        addModuleToGraph(module);
    }

    // Check for cycles
    if (graph_.hasCycle()) {
        auto cycles = graph_.detectCycles();
        std::ostringstream oss;
        oss << "Circular dependencies detected: ";
        for (size_t i = 0; i < cycles.size(); ++i) {
            if (i > 0) oss << "; ";
            oss << cycles[i].toString();
        }
        throw std::runtime_error(oss.str());
    }
}

void DependencyResolver::rebuildGraph() {
    buildGraph();
}

void DependencyResolver::addModuleToGraph(Module* module) {
    if (!module) {
        return;
    }

    // Parse manifest to get dependencies
    const nlohmann::json& manifestJson = module->getManifest();
    ModuleManifest manifest;

    try {
        manifest = ManifestParser::parse(manifestJson);
    } catch (const std::exception& e) {
        LOGW_FMT("Failed to parse manifest for module " << module->getSymbolicName()
                 << ": " << e.what());
        return;
    }

    // Add edges for each dependency
    for (const auto& dep : manifest.dependencies) {
        // Skip optional dependencies for dependency graph
        // (they don't create hard ordering constraints)
        if (dep.optional) {
            continue;
        }

        // Find the dependency module
        Module* depModule = moduleRegistry_->findCompatibleModule(
            dep.symbolicName,
            dep.versionRange
        );

        if (depModule) {
            // Add edge: module depends on depModule
            graph_.addEdge(module->getModuleId(), depModule->getModuleId());
        } else {
            LOGW_FMT("Module " << module->getSymbolicName()
                     << " has unsatisfied dependency: " << dep.symbolicName
                     << " " << dep.versionRange.toString());
        }
    }
}

std::vector<Module*> DependencyResolver::getStartOrder() const {
    // Get topological sort (dependencies before dependents)
    auto moduleIds = graph_.topologicalSort();
    return moduleIdsToModules(moduleIds);
}

std::vector<Module*> DependencyResolver::getStopOrder() const {
    // Get topological sort and reverse it (dependents stop before dependencies)
    auto moduleIds = graph_.topologicalSort();
    std::reverse(moduleIds.begin(), moduleIds.end());
    return moduleIdsToModules(moduleIds);
}

std::vector<DependencyCycle> DependencyResolver::detectCycles() const {
    return graph_.detectCycles();
}

bool DependencyResolver::hasCycle() const {
    return graph_.hasCycle();
}

bool DependencyResolver::validateModule(Module* module) const {
    if (!module) {
        return false;
    }

    // Create temporary graph with the new module
    DependencyGraph tempGraph = graph_;

    // Add the new module
    tempGraph.addNode(module->getModuleId(), module->getSymbolicName());

    // Parse manifest to get dependencies
    const nlohmann::json& manifestJson = module->getManifest();
    ModuleManifest manifest;

    try {
        manifest = ManifestParser::parse(manifestJson);
    } catch (const std::exception&) {
        return false;
    }

    // Add edges for dependencies
    for (const auto& dep : manifest.dependencies) {
        if (dep.optional) {
            continue;
        }

        Module* depModule = moduleRegistry_->findCompatibleModule(
            dep.symbolicName,
            dep.versionRange
        );

        if (depModule) {
            // Check if adding this edge would create a cycle
            if (!tempGraph.canAddEdge(module->getModuleId(), depModule->getModuleId())) {
                return false;
            }
            tempGraph.addEdge(module->getModuleId(), depModule->getModuleId());
        }
    }

    // Check if the graph has cycles
    return !tempGraph.hasCycle();
}

std::vector<Module*> DependencyResolver::getDependents(Module* module) const {
    if (!module) {
        return {};
    }

    auto dependentIds = graph_.getDependents(module->getModuleId());
    return moduleIdsToModules(dependentIds);
}

std::vector<Module*> DependencyResolver::getDependencies(Module* module) const {
    if (!module) {
        return {};
    }

    auto dependencyIds = graph_.getDependencies(module->getModuleId());
    return moduleIdsToModules(dependencyIds);
}

std::vector<Module*> DependencyResolver::moduleIdsToModules(
    const std::vector<uint64_t>& moduleIds) const {

    std::vector<Module*> modules;
    modules.reserve(moduleIds.size());

    for (uint64_t id : moduleIds) {
        Module* module = moduleRegistry_->getModule(id);
        if (module) {
            modules.push_back(module);
        }
    }

    return modules;
}

} // namespace cdmf
