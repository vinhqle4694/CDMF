#include <gtest/gtest.h>
#include "module/dependency_graph.h"
#include <algorithm>

using namespace cdmf;

// ============================================================================
// Dependency Graph Tests
// ============================================================================

TEST(DependencyGraphTest, Construction) {
    DependencyGraph graph;
    EXPECT_EQ(0u, graph.getNodeCount());
    EXPECT_EQ(0u, graph.getEdgeCount());
}

TEST(DependencyGraphTest, AddNode) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");

    EXPECT_EQ(1u, graph.getNodeCount());
    EXPECT_TRUE(graph.hasNode(1));
    EXPECT_EQ(0, graph.getInDegree(1));
    EXPECT_EQ(0, graph.getOutDegree(1));
}

TEST(DependencyGraphTest, AddMultipleNodes) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");
    graph.addNode(3, "com.example.module3");

    EXPECT_EQ(3u, graph.getNodeCount());
    EXPECT_TRUE(graph.hasNode(1));
    EXPECT_TRUE(graph.hasNode(2));
    EXPECT_TRUE(graph.hasNode(3));
    EXPECT_FALSE(graph.hasNode(999));
}

TEST(DependencyGraphTest, AddEdge) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");

    // Module 1 depends on Module 2
    graph.addEdge(1, 2);

    EXPECT_EQ(1u, graph.getEdgeCount());
    EXPECT_EQ(1, graph.getOutDegree(1));  // Module 1 has 1 dependency
    EXPECT_EQ(0, graph.getInDegree(1));   // No one depends on Module 1
    EXPECT_EQ(0, graph.getOutDegree(2));  // Module 2 has no dependencies
    EXPECT_EQ(1, graph.getInDegree(2));   // Module 1 depends on Module 2
}

TEST(DependencyGraphTest, AddEdgeInvalidNodes) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");

    // Try to add edge to non-existent node
    EXPECT_THROW(graph.addEdge(1, 999), std::invalid_argument);
    EXPECT_THROW(graph.addEdge(999, 1), std::invalid_argument);
}

TEST(DependencyGraphTest, AddDuplicateEdge) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");

    graph.addEdge(1, 2);
    graph.addEdge(1, 2);  // Duplicate edge

    // Should only count as one edge
    EXPECT_EQ(1u, graph.getEdgeCount());
    EXPECT_EQ(1, graph.getOutDegree(1));
}

TEST(DependencyGraphTest, RemoveNode) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");
    graph.addEdge(1, 2);

    graph.removeNode(1);

    EXPECT_EQ(1u, graph.getNodeCount());
    EXPECT_FALSE(graph.hasNode(1));
    EXPECT_TRUE(graph.hasNode(2));
    EXPECT_EQ(0u, graph.getEdgeCount());
}

TEST(DependencyGraphTest, Clear) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");
    graph.addEdge(1, 2);

    graph.clear();

    EXPECT_EQ(0u, graph.getNodeCount());
    EXPECT_EQ(0u, graph.getEdgeCount());
}

TEST(DependencyGraphTest, GetDependencies) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");
    graph.addNode(3, "com.example.module3");

    // Module 1 depends on Module 2 and 3
    graph.addEdge(1, 2);
    graph.addEdge(1, 3);

    auto deps = graph.getDependencies(1);
    EXPECT_EQ(2u, deps.size());
    EXPECT_TRUE(std::find(deps.begin(), deps.end(), 2) != deps.end());
    EXPECT_TRUE(std::find(deps.begin(), deps.end(), 3) != deps.end());
}

TEST(DependencyGraphTest, GetDependents) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");
    graph.addNode(2, "com.example.module2");
    graph.addNode(3, "com.example.module3");

    // Module 1 and 2 depend on Module 3
    graph.addEdge(1, 3);
    graph.addEdge(2, 3);

    auto dependents = graph.getDependents(3);
    EXPECT_EQ(2u, dependents.size());
    EXPECT_TRUE(std::find(dependents.begin(), dependents.end(), 1) != dependents.end());
    EXPECT_TRUE(std::find(dependents.begin(), dependents.end(), 2) != dependents.end());
}

// ============================================================================
// Topological Sort Tests (Kahn's Algorithm)
// ============================================================================

TEST(DependencyGraphTest, TopologicalSort_EmptyGraph) {
    DependencyGraph graph;
    auto sorted = graph.topologicalSort();
    EXPECT_EQ(0u, sorted.size());
}

TEST(DependencyGraphTest, TopologicalSort_SingleNode) {
    DependencyGraph graph;
    graph.addNode(1, "com.example.module1");

    auto sorted = graph.topologicalSort();
    ASSERT_EQ(1u, sorted.size());
    EXPECT_EQ(1u, sorted[0]);
}

TEST(DependencyGraphTest, TopologicalSort_LinearChain) {
    // A -> B -> C
    // Expected order: C, B, A
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");

    graph.addEdge(1, 2);  // A depends on B
    graph.addEdge(2, 3);  // B depends on C

    auto sorted = graph.topologicalSort();
    ASSERT_EQ(3u, sorted.size());

    // C should come before B, B before A
    auto posC = std::find(sorted.begin(), sorted.end(), 3);
    auto posB = std::find(sorted.begin(), sorted.end(), 2);
    auto posA = std::find(sorted.begin(), sorted.end(), 1);

    EXPECT_LT(posC, posB);
    EXPECT_LT(posB, posA);
}

TEST(DependencyGraphTest, TopologicalSort_Diamond) {
    // A -> B, A -> C, B -> D, C -> D
    // Expected: D before B and C, B and C before A
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");
    graph.addNode(4, "D");

    graph.addEdge(1, 2);  // A depends on B
    graph.addEdge(1, 3);  // A depends on C
    graph.addEdge(2, 4);  // B depends on D
    graph.addEdge(3, 4);  // C depends on D

    auto sorted = graph.topologicalSort();
    ASSERT_EQ(4u, sorted.size());

    // D should come before B and C
    auto posD = std::find(sorted.begin(), sorted.end(), 4);
    auto posB = std::find(sorted.begin(), sorted.end(), 2);
    auto posC = std::find(sorted.begin(), sorted.end(), 3);
    auto posA = std::find(sorted.begin(), sorted.end(), 1);

    EXPECT_LT(posD, posB);
    EXPECT_LT(posD, posC);
    EXPECT_LT(posB, posA);
    EXPECT_LT(posC, posA);
}

TEST(DependencyGraphTest, TopologicalSort_ComplexGraph) {
    // Complex dependency graph
    DependencyGraph graph;
    for (int i = 1; i <= 6; ++i) {
        graph.addNode(i, "Module" + std::to_string(i));
    }

    graph.addEdge(1, 2);
    graph.addEdge(1, 3);
    graph.addEdge(2, 4);
    graph.addEdge(3, 4);
    graph.addEdge(4, 5);
    graph.addEdge(3, 6);

    auto sorted = graph.topologicalSort();
    EXPECT_EQ(6u, sorted.size());

    // Verify dependencies are satisfied
    std::map<uint64_t, size_t> positions;
    for (size_t i = 0; i < sorted.size(); ++i) {
        positions[sorted[i]] = i;
    }

    // Module 2 should come before Module 1
    EXPECT_LT(positions[2], positions[1]);
    // Module 3 should come before Module 1
    EXPECT_LT(positions[3], positions[1]);
    // Module 4 should come before Module 2 and 3
    EXPECT_LT(positions[4], positions[2]);
    EXPECT_LT(positions[4], positions[3]);
    // Module 5 should come before Module 4
    EXPECT_LT(positions[5], positions[4]);
    // Module 6 should come before Module 3
    EXPECT_LT(positions[6], positions[3]);
}

// ============================================================================
// Cycle Detection Tests
// ============================================================================

TEST(DependencyGraphTest, HasCycle_NoCycle) {
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");

    graph.addEdge(1, 2);
    graph.addEdge(2, 3);

    EXPECT_FALSE(graph.hasCycle());
}

TEST(DependencyGraphTest, HasCycle_SelfLoop) {
    DependencyGraph graph;
    graph.addNode(1, "A");

    graph.addEdge(1, 1);  // A depends on A (self-loop)

    EXPECT_TRUE(graph.hasCycle());
}

TEST(DependencyGraphTest, HasCycle_TwoNodeCycle) {
    // A -> B -> A
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");

    graph.addEdge(1, 2);  // A depends on B
    graph.addEdge(2, 1);  // B depends on A

    EXPECT_TRUE(graph.hasCycle());
}

TEST(DependencyGraphTest, HasCycle_ThreeNodeCycle) {
    // A -> B -> C -> A
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");

    graph.addEdge(1, 2);  // A depends on B
    graph.addEdge(2, 3);  // B depends on C
    graph.addEdge(3, 1);  // C depends on A

    EXPECT_TRUE(graph.hasCycle());
}

TEST(DependencyGraphTest, DetectCycles_NoCycle) {
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addEdge(1, 2);

    auto cycles = graph.detectCycles();
    EXPECT_EQ(0u, cycles.size());
}

TEST(DependencyGraphTest, DetectCycles_SingleCycle) {
    // A -> B -> C -> A
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");

    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addEdge(3, 1);

    auto cycles = graph.detectCycles();
    EXPECT_GE(cycles.size(), 1u);

    // Check that cycle contains all three nodes
    if (!cycles.empty()) {
        const auto& cycle = cycles[0];
        EXPECT_EQ(3u, cycle.moduleIds.size());
        EXPECT_EQ(3u, cycle.symbolicNames.size());
    }
}

TEST(DependencyGraphTest, DetectCycles_ToString) {
    DependencyGraph graph;
    graph.addNode(1, "ModuleA");
    graph.addNode(2, "ModuleB");

    graph.addEdge(1, 2);
    graph.addEdge(2, 1);

    auto cycles = graph.detectCycles();
    ASSERT_GE(cycles.size(), 1u);

    std::string cycleStr = cycles[0].toString();
    EXPECT_FALSE(cycleStr.empty());
    // Should contain both module names
    EXPECT_NE(cycleStr.find("Module"), std::string::npos);
}

TEST(DependencyGraphTest, TopologicalSort_ThrowsOnCycle) {
    // A -> B -> A (cycle)
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");

    graph.addEdge(1, 2);
    graph.addEdge(2, 1);

    EXPECT_THROW(graph.topologicalSort(), std::runtime_error);
}

TEST(DependencyGraphTest, CanAddEdge_Valid) {
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");

    EXPECT_TRUE(graph.canAddEdge(1, 2));
}

TEST(DependencyGraphTest, CanAddEdge_WouldCreateCycle) {
    // A -> B, trying to add B -> A would create cycle
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");

    graph.addEdge(1, 2);

    EXPECT_FALSE(graph.canAddEdge(2, 1));
}

TEST(DependencyGraphTest, CanAddEdge_ExistingEdge) {
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");

    graph.addEdge(1, 2);

    // Can add edge that already exists (no cycle created)
    EXPECT_TRUE(graph.canAddEdge(1, 2));
}

TEST(DependencyGraphTest, CanAddEdge_InvalidNodes) {
    DependencyGraph graph;
    graph.addNode(1, "A");

    EXPECT_FALSE(graph.canAddEdge(1, 999));
    EXPECT_FALSE(graph.canAddEdge(999, 1));
}

TEST(DependencyGraphTest, GetAllNodes) {
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");

    auto nodes = graph.getAllNodes();
    EXPECT_EQ(3u, nodes.size());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(DependencyGraphTest, LargeGraph) {
    // Create a graph with many nodes
    DependencyGraph graph;
    const int nodeCount = 100;

    for (int i = 0; i < nodeCount; ++i) {
        graph.addNode(i, "Module" + std::to_string(i));
    }

    // Create linear dependency chain
    for (int i = 0; i < nodeCount - 1; ++i) {
        graph.addEdge(i, i + 1);
    }

    EXPECT_EQ(nodeCount, graph.getNodeCount());
    EXPECT_EQ(nodeCount - 1, graph.getEdgeCount());
    EXPECT_FALSE(graph.hasCycle());

    auto sorted = graph.topologicalSort();
    EXPECT_EQ(nodeCount, sorted.size());
}

TEST(DependencyGraphTest, DisconnectedComponents) {
    // Graph with disconnected components
    DependencyGraph graph;
    graph.addNode(1, "A");
    graph.addNode(2, "B");
    graph.addNode(3, "C");
    graph.addNode(4, "D");

    // A -> B (one component)
    graph.addEdge(1, 2);

    // C -> D (another component)
    graph.addEdge(3, 4);

    EXPECT_FALSE(graph.hasCycle());

    auto sorted = graph.topologicalSort();
    EXPECT_EQ(4u, sorted.size());
}
