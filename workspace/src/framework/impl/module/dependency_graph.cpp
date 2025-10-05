#include "module/dependency_graph.h"
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace cdmf {

// ==================================================================
// DependencyCycle Implementation
// ==================================================================

std::string DependencyCycle::toString() const {
    if (symbolicNames.empty()) {
        return "Empty cycle";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < symbolicNames.size(); ++i) {
        if (i > 0) {
            oss << " -> ";
        }
        oss << symbolicNames[i];
    }

    // Close the cycle by showing it loops back
    if (!symbolicNames.empty()) {
        oss << " -> " << symbolicNames[0];
    }

    return oss.str();
}

// ==================================================================
// DependencyGraph Implementation
// ==================================================================

DependencyGraph::DependencyGraph()
    : edgeCount_(0) {
}

DependencyGraph::~DependencyGraph() {
}

void DependencyGraph::addNode(uint64_t moduleId, const std::string& symbolicName) {
    if (nodes_.find(moduleId) != nodes_.end()) {
        // Node already exists, update symbolic name if needed
        nodes_[moduleId].symbolicName = symbolicName;
        return;
    }

    DependencyNode node;
    node.moduleId = moduleId;
    node.symbolicName = symbolicName;
    node.inDegree = 0;
    node.outDegree = 0;

    nodes_[moduleId] = node;
}

void DependencyGraph::addEdge(uint64_t fromModuleId, uint64_t toModuleId) {
    // Ensure both nodes exist
    if (nodes_.find(fromModuleId) == nodes_.end()) {
        throw std::invalid_argument("Source module not found in graph");
    }
    if (nodes_.find(toModuleId) == nodes_.end()) {
        throw std::invalid_argument("Target module not found in graph");
    }

    // Check if edge already exists
    auto& fromNode = nodes_[fromModuleId];
    auto it = std::find(fromNode.dependencies.begin(), fromNode.dependencies.end(), toModuleId);
    if (it != fromNode.dependencies.end()) {
        // Edge already exists
        return;
    }

    // Add edge: fromModule depends on toModule
    fromNode.dependencies.push_back(toModuleId);
    fromNode.outDegree++;

    // Add reverse edge: toModule is depended on by fromModule
    auto& toNode = nodes_[toModuleId];
    toNode.dependents.push_back(fromModuleId);
    toNode.inDegree++;

    edgeCount_++;
}

void DependencyGraph::removeNode(uint64_t moduleId) {
    auto it = nodes_.find(moduleId);
    if (it == nodes_.end()) {
        return; // Node doesn't exist
    }

    const DependencyNode& node = it->second;

    // Remove outgoing edges (this node depends on others)
    for (uint64_t depId : node.dependencies) {
        auto depIt = nodes_.find(depId);
        if (depIt != nodes_.end()) {
            auto& depNode = depIt->second;
            // Remove this node from the dependent's dependents list
            auto pos = std::find(depNode.dependents.begin(), depNode.dependents.end(), moduleId);
            if (pos != depNode.dependents.end()) {
                depNode.dependents.erase(pos);
                depNode.inDegree--;
                edgeCount_--;
            }
        }
    }

    // Remove incoming edges (others depend on this node)
    for (uint64_t dependentId : node.dependents) {
        auto depIt = nodes_.find(dependentId);
        if (depIt != nodes_.end()) {
            auto& depNode = depIt->second;
            // Remove this node from the dependent's dependencies list
            auto pos = std::find(depNode.dependencies.begin(), depNode.dependencies.end(), moduleId);
            if (pos != depNode.dependencies.end()) {
                depNode.dependencies.erase(pos);
                depNode.outDegree--;
                edgeCount_--;
            }
        }
    }

    // Remove the node
    nodes_.erase(it);
}

void DependencyGraph::clear() {
    nodes_.clear();
    edgeCount_ = 0;
}

bool DependencyGraph::hasNode(uint64_t moduleId) const {
    return nodes_.find(moduleId) != nodes_.end();
}

int DependencyGraph::getInDegree(uint64_t moduleId) const {
    auto it = nodes_.find(moduleId);
    if (it == nodes_.end()) {
        return -1;
    }
    return it->second.inDegree;
}

int DependencyGraph::getOutDegree(uint64_t moduleId) const {
    auto it = nodes_.find(moduleId);
    if (it == nodes_.end()) {
        return -1;
    }
    return it->second.outDegree;
}

std::vector<uint64_t> DependencyGraph::getDependents(uint64_t moduleId) const {
    auto it = nodes_.find(moduleId);
    if (it == nodes_.end()) {
        return {};
    }
    return it->second.dependents;
}

std::vector<uint64_t> DependencyGraph::getDependencies(uint64_t moduleId) const {
    auto it = nodes_.find(moduleId);
    if (it == nodes_.end()) {
        return {};
    }
    return it->second.dependencies;
}

std::vector<DependencyNode> DependencyGraph::getAllNodes() const {
    std::vector<DependencyNode> result;
    result.reserve(nodes_.size());

    for (const auto& pair : nodes_) {
        result.push_back(pair.second);
    }

    return result;
}

size_t DependencyGraph::getNodeCount() const {
    return nodes_.size();
}

size_t DependencyGraph::getEdgeCount() const {
    return edgeCount_;
}

// ==================================================================
// Topological Sort (Kahn's Algorithm)
// ==================================================================

std::vector<uint64_t> DependencyGraph::topologicalSort() const {
    std::vector<uint64_t> result;
    result.reserve(nodes_.size());

    // Copy out-degrees (we'll modify them)
    // We start with nodes that have no dependencies (outDegree = 0)
    std::map<uint64_t, int> outDegrees;
    for (const auto& [id, node] : nodes_) {
        outDegrees[id] = node.outDegree;
    }

    // Queue of nodes with zero out-degree (no dependencies)
    std::vector<uint64_t> queue;
    for (const auto& [id, degree] : outDegrees) {
        if (degree == 0) {
            queue.push_back(id);
        }
    }

    // Process queue
    while (!queue.empty()) {
        // Remove node with zero out-degree
        uint64_t nodeId = queue.back();
        queue.pop_back();
        result.push_back(nodeId);

        // Reduce out-degree of dependents (nodes that depend on this one)
        const auto& node = nodes_.at(nodeId);
        for (uint64_t dependentId : node.dependents) {
            outDegrees[dependentId]--;
            if (outDegrees[dependentId] == 0) {
                queue.push_back(dependentId);
            }
        }
    }

    // If not all nodes processed, graph has cycle
    if (result.size() != nodes_.size()) {
        auto cycles = detectCycles();
        std::ostringstream oss;
        oss << "Dependency graph contains cycles: ";
        for (size_t i = 0; i < cycles.size(); ++i) {
            if (i > 0) oss << "; ";
            oss << cycles[i].toString();
        }
        throw std::runtime_error(oss.str());
    }

    return result;
}

// ==================================================================
// Cycle Detection (DFS)
// ==================================================================

bool DependencyGraph::dfsHasCycle(
    uint64_t nodeId,
    std::unordered_set<uint64_t>& visited,
    std::unordered_set<uint64_t>& recursionStack) const {

    visited.insert(nodeId);
    recursionStack.insert(nodeId);

    const auto& node = nodes_.at(nodeId);
    for (uint64_t depId : node.dependencies) {
        // Not visited: recurse
        if (visited.find(depId) == visited.end()) {
            if (dfsHasCycle(depId, visited, recursionStack)) {
                return true;
            }
        }
        // In recursion stack: cycle detected
        else if (recursionStack.find(depId) != recursionStack.end()) {
            return true;
        }
    }

    recursionStack.erase(nodeId);
    return false;
}

bool DependencyGraph::hasCycle() const {
    std::unordered_set<uint64_t> visited;
    std::unordered_set<uint64_t> recursionStack;

    for (const auto& [nodeId, node] : nodes_) {
        if (visited.find(nodeId) == visited.end()) {
            if (dfsHasCycle(nodeId, visited, recursionStack)) {
                return true;
            }
        }
    }

    return false;
}

void DependencyGraph::dfsFindCycles(
    uint64_t nodeId,
    std::unordered_set<uint64_t>& visited,
    std::unordered_set<uint64_t>& recursionStack,
    std::vector<uint64_t>& path,
    std::vector<DependencyCycle>& cycles) const {

    visited.insert(nodeId);
    recursionStack.insert(nodeId);
    path.push_back(nodeId);

    const auto& node = nodes_.at(nodeId);
    for (uint64_t depId : node.dependencies) {
        // Not visited: recurse
        if (visited.find(depId) == visited.end()) {
            dfsFindCycles(depId, visited, recursionStack, path, cycles);
        }
        // In recursion stack: cycle detected
        else if (recursionStack.find(depId) != recursionStack.end()) {
            // Extract the cycle from path
            DependencyCycle cycle;

            // Find where the cycle starts in the path
            auto cycleStart = std::find(path.begin(), path.end(), depId);
            if (cycleStart != path.end()) {
                for (auto it = cycleStart; it != path.end(); ++it) {
                    cycle.moduleIds.push_back(*it);
                    const auto& cycleNode = nodes_.at(*it);
                    cycle.symbolicNames.push_back(cycleNode.symbolicName);
                }
                cycles.push_back(cycle);
            }
        }
    }

    path.pop_back();
    recursionStack.erase(nodeId);
}

std::vector<DependencyCycle> DependencyGraph::detectCycles() const {
    std::vector<DependencyCycle> cycles;
    std::unordered_set<uint64_t> visited;
    std::unordered_set<uint64_t> recursionStack;
    std::vector<uint64_t> path;

    for (const auto& [nodeId, node] : nodes_) {
        if (visited.find(nodeId) == visited.end()) {
            dfsFindCycles(nodeId, visited, recursionStack, path, cycles);
        }
    }

    return cycles;
}

bool DependencyGraph::canAddEdge(uint64_t fromModuleId, uint64_t toModuleId) const {
    // Check if adding this edge would create a cycle
    // An edge from A to B creates a cycle if there's already a path from B to A

    if (nodes_.find(fromModuleId) == nodes_.end() ||
        nodes_.find(toModuleId) == nodes_.end()) {
        return false;
    }

    // If edge already exists, it's safe (no new cycle)
    const auto& fromNode = nodes_.at(fromModuleId);
    auto it = std::find(fromNode.dependencies.begin(), fromNode.dependencies.end(), toModuleId);
    if (it != fromNode.dependencies.end()) {
        return true;
    }

    // Check if there's a path from toModuleId to fromModuleId
    // If yes, adding edge fromModuleId -> toModuleId creates a cycle
    std::unordered_set<uint64_t> visited;
    std::vector<uint64_t> stack;
    stack.push_back(toModuleId);
    visited.insert(toModuleId);

    while (!stack.empty()) {
        uint64_t current = stack.back();
        stack.pop_back();

        if (current == fromModuleId) {
            // Found path from toModuleId to fromModuleId
            return false;
        }

        const auto& currentNode = nodes_.at(current);
        for (uint64_t depId : currentNode.dependencies) {
            if (visited.find(depId) == visited.end()) {
                visited.insert(depId);
                stack.push_back(depId);
            }
        }
    }

    return true;
}

} // namespace cdmf
