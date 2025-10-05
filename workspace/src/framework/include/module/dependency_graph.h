#ifndef CDMF_DEPENDENCY_GRAPH_H
#define CDMF_DEPENDENCY_GRAPH_H

#include <cstdint>
#include <vector>
#include <map>
#include <unordered_set>
#include <string>

namespace cdmf {

// Forward declaration
class Module;

/**
 * @brief Represents a dependency graph node
 */
struct DependencyNode {
    uint64_t moduleId;
    std::string symbolicName;
    std::vector<uint64_t> dependencies;    // Outgoing edges (modules I depend on)
    std::vector<uint64_t> dependents;      // Incoming edges (modules that depend on me)
    int inDegree;                          // Number of incoming edges
    int outDegree;                         // Number of outgoing edges
};

/**
 * @brief Represents a circular dependency cycle
 */
struct DependencyCycle {
    std::vector<uint64_t> moduleIds;
    std::vector<std::string> symbolicNames;

    std::string toString() const;
};

/**
 * @brief Dependency graph for module relationships
 *
 * Maintains a directed acyclic graph (DAG) of module dependencies.
 * Provides algorithms for topological sorting and cycle detection.
 *
 * Time Complexity:
 * - addNode/addEdge: O(1)
 * - topologicalSort: O(V + E) using Kahn's algorithm
 * - detectCycles: O(V + E) using DFS
 *
 * Space Complexity:
 * - O(V + E) where V = nodes, E = edges
 */
class DependencyGraph {
public:
    /**
     * @brief Constructor
     */
    DependencyGraph();

    /**
     * @brief Destructor
     */
    ~DependencyGraph();

    // Graph construction

    /**
     * @brief Add a module node to the graph
     * @param moduleId Unique module ID
     * @param symbolicName Module symbolic name
     */
    void addNode(uint64_t moduleId, const std::string& symbolicName);

    /**
     * @brief Add a dependency edge
     * @param fromModuleId Module that depends on another
     * @param toModuleId Module being depended upon
     */
    void addEdge(uint64_t fromModuleId, uint64_t toModuleId);

    /**
     * @brief Remove a node and all associated edges
     * @param moduleId Module ID to remove
     */
    void removeNode(uint64_t moduleId);

    /**
     * @brief Clear all nodes and edges
     */
    void clear();

    // Graph queries

    /**
     * @brief Check if node exists
     */
    bool hasNode(uint64_t moduleId) const;

    /**
     * @brief Get in-degree (number of dependents)
     */
    int getInDegree(uint64_t moduleId) const;

    /**
     * @brief Get out-degree (number of dependencies)
     */
    int getOutDegree(uint64_t moduleId) const;

    /**
     * @brief Get modules that depend on this module
     */
    std::vector<uint64_t> getDependents(uint64_t moduleId) const;

    /**
     * @brief Get modules this module depends on
     */
    std::vector<uint64_t> getDependencies(uint64_t moduleId) const;

    /**
     * @brief Get all nodes
     */
    std::vector<DependencyNode> getAllNodes() const;

    /**
     * @brief Get node count
     */
    size_t getNodeCount() const;

    /**
     * @brief Get edge count
     */
    size_t getEdgeCount() const;

    // Algorithms

    /**
     * @brief Perform topological sort using Kahn's algorithm
     *
     * Returns modules in dependency order (dependencies before dependents).
     * Throws exception if graph contains cycles.
     *
     * Time Complexity: O(V + E)
     *
     * @return Vector of module IDs in topological order
     * @throws std::runtime_error if graph has cycles
     */
    std::vector<uint64_t> topologicalSort() const;

    /**
     * @brief Detect cycles using DFS
     *
     * Time Complexity: O(V + E)
     *
     * @return Vector of cycles found (empty if acyclic)
     */
    std::vector<DependencyCycle> detectCycles() const;

    /**
     * @brief Check if graph has any cycles
     *
     * Time Complexity: O(V + E)
     *
     * @return true if cyclic, false if acyclic
     */
    bool hasCycle() const;

    /**
     * @brief Validate that adding an edge would not create a cycle
     *
     * @param fromModuleId Source module
     * @param toModuleId Target module
     * @return true if edge can be added safely
     */
    bool canAddEdge(uint64_t fromModuleId, uint64_t toModuleId) const;

private:
    /**
     * @brief DFS helper for cycle detection
     */
    bool dfsHasCycle(uint64_t nodeId,
                     std::unordered_set<uint64_t>& visited,
                     std::unordered_set<uint64_t>& recursionStack) const;

    /**
     * @brief DFS helper for finding cycles
     */
    void dfsFindCycles(uint64_t nodeId,
                       std::unordered_set<uint64_t>& visited,
                       std::unordered_set<uint64_t>& recursionStack,
                       std::vector<uint64_t>& path,
                       std::vector<DependencyCycle>& cycles) const;

private:
    std::map<uint64_t, DependencyNode> nodes_;
    size_t edgeCount_;
};

} // namespace cdmf

#endif // CDMF_DEPENDENCY_GRAPH_H
