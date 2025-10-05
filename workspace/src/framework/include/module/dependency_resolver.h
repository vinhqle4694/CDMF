#ifndef CDMF_DEPENDENCY_RESOLVER_H
#define CDMF_DEPENDENCY_RESOLVER_H

#include "module/dependency_graph.h"
#include "module/module.h"
#include "module/module_registry.h"
#include <memory>
#include <vector>
#include <string>

namespace cdmf {

/**
 * @brief Resolves module dependencies and determines start/stop order
 *
 * Uses topological sorting (Kahn's algorithm) to determine module start order.
 * Uses DFS to detect circular dependencies.
 *
 * Features:
 * - Automatic dependency graph construction from module manifests
 * - Topological sort for start order
 * - Reverse topological order for stop order
 * - Cycle detection with detailed cycle reporting
 * - Dependency validation
 *
 * Time Complexity:
 * - buildGraph: O(V * D) where V = modules, D = avg dependencies per module
 * - getStartOrder/getStopOrder: O(V + E)
 * - detectCycles: O(V + E)
 */
class DependencyResolver {
public:
    /**
     * @brief Constructor
     * @param moduleRegistry Module registry to query
     */
    explicit DependencyResolver(ModuleRegistry* moduleRegistry);

    /**
     * @brief Destructor
     */
    ~DependencyResolver();

    // Prevent copying
    DependencyResolver(const DependencyResolver&) = delete;
    DependencyResolver& operator=(const DependencyResolver&) = delete;

    /**
     * @brief Build dependency graph from all installed modules
     *
     * Reads module manifests and constructs dependency graph.
     *
     * @throws std::runtime_error if circular dependencies detected
     */
    void buildGraph();

    /**
     * @brief Rebuild graph (after module installation/uninstallation)
     */
    void rebuildGraph();

    /**
     * @brief Get module start order (dependencies first)
     *
     * @return Vector of modules in start order
     * @throws std::runtime_error if graph has cycles
     */
    std::vector<Module*> getStartOrder() const;

    /**
     * @brief Get module stop order (reverse of start order)
     *
     * @return Vector of modules in stop order
     */
    std::vector<Module*> getStopOrder() const;

    /**
     * @brief Detect circular dependencies
     *
     * @return Vector of cycles (empty if acyclic)
     */
    std::vector<DependencyCycle> detectCycles() const;

    /**
     * @brief Check if graph has cycles
     *
     * @return true if cyclic
     */
    bool hasCycle() const;

    /**
     * @brief Validate that a module can be installed
     *
     * Checks if adding the module would create circular dependencies.
     *
     * @param module Module to validate
     * @return true if safe to install
     */
    bool validateModule(Module* module) const;

    /**
     * @brief Get modules that depend on a given module
     *
     * @param module Module to query
     * @return Vector of dependent modules
     */
    std::vector<Module*> getDependents(Module* module) const;

    /**
     * @brief Get modules that a given module depends on
     *
     * @param module Module to query
     * @return Vector of dependency modules
     */
    std::vector<Module*> getDependencies(Module* module) const;

    /**
     * @brief Get dependency graph (read-only)
     */
    const DependencyGraph& getGraph() const { return graph_; }

private:
    /**
     * @brief Add module to dependency graph
     */
    void addModuleToGraph(Module* module);

    /**
     * @brief Convert module IDs to module pointers
     */
    std::vector<Module*> moduleIdsToModules(const std::vector<uint64_t>& moduleIds) const;

private:
    ModuleRegistry* moduleRegistry_;
    DependencyGraph graph_;
};

} // namespace cdmf

#endif // CDMF_DEPENDENCY_RESOLVER_H
