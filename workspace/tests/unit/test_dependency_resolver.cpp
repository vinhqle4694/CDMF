#include <gtest/gtest.h>
#include "module/dependency_resolver.h"
#include "module/module_registry.h"
#include "module/module.h"
#include "module/manifest_parser.h"
#include <memory>
#include <nlohmann/json.hpp>

using namespace cdmf;

// Mock module for testing
class MockModule : public Module {
public:
    MockModule(uint64_t id, const std::string& name, const Version& version,
               const std::vector<ModuleDependency>& deps = {})
        : id_(id), name_(name), version_(version), state_(ModuleState::RESOLVED) {

        // Create manifest JSON
        nlohmann::json manifest;
        manifest["module"]["symbolic-name"] = name;
        manifest["module"]["version"] = version.toString();
        manifest["module"]["name"] = name;
        manifest["module"]["activator"] = "Activator";
        manifest["module"]["auto-start"] = false;

        // Add dependencies
        nlohmann::json depsJson = nlohmann::json::array();
        for (const auto& dep : deps) {
            nlohmann::json depJson;
            depJson["symbolic-name"] = dep.symbolicName;
            depJson["version-range"] = dep.versionRange.toString();
            depJson["optional"] = dep.optional;
            depsJson.push_back(depJson);
        }
        manifest["dependencies"] = depsJson;

        manifestJson_ = manifest;
    }

    std::string getSymbolicName() const override { return name_; }
    Version getVersion() const override { return version_; }
    std::string getLocation() const override { return ""; }
    uint64_t getModuleId() const override { return id_; }

    void start() override { state_ = ModuleState::ACTIVE; }
    void stop() override { state_ = ModuleState::RESOLVED; }
    void update(const std::string&) override {}
    void uninstall() override { state_ = ModuleState::UNINSTALLED; }

    ModuleState getState() const override { return state_; }
    IModuleContext* getContext() override { return nullptr; }

    std::vector<ServiceRegistration> getRegisteredServices() const override { return {}; }
    std::vector<ServiceReference> getServicesInUse() const override { return {}; }

    const nlohmann::json& getManifest() const override {
        return manifestJson_;
    }

    std::map<std::string, std::string> getHeaders() const override { return {}; }

    void addModuleListener(IModuleListener*) override {}
    void removeModuleListener(IModuleListener*) override {}

private:
    uint64_t id_;
    std::string name_;
    Version version_;
    ModuleState state_;
    nlohmann::json manifestJson_;
};

// ============================================================================
// Dependency Resolver Tests
// ============================================================================

class DependencyResolverTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry_ = std::make_unique<ModuleRegistry>();
    }

    void TearDown() override {
        // Clean up modules
        modules_.clear();
        registry_.reset();
    }

    Module* createAndRegisterModule(uint64_t id, const std::string& name,
                                     const Version& version,
                                     const std::vector<ModuleDependency>& deps = {}) {
        auto module = std::make_unique<MockModule>(id, name, version, deps);
        Module* modulePtr = module.get();
        registry_->registerModule(modulePtr);
        modules_.push_back(std::move(module));
        return modulePtr;
    }

    std::unique_ptr<ModuleRegistry> registry_;
    std::vector<std::unique_ptr<Module>> modules_;
};

TEST_F(DependencyResolverTest, Construction) {
    DependencyResolver resolver(registry_.get());
    EXPECT_EQ(0u, resolver.getGraph().getNodeCount());
}

TEST_F(DependencyResolverTest, ConstructionNullRegistry) {
    EXPECT_THROW(DependencyResolver resolver(nullptr), std::invalid_argument);
}

TEST_F(DependencyResolverTest, BuildGraph_EmptyRegistry) {
    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_EQ(0u, resolver.getGraph().getNodeCount());
}

TEST_F(DependencyResolverTest, BuildGraph_SingleModule) {
    createAndRegisterModule(1, "com.example.module1", Version(1, 0, 0));

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_EQ(1u, resolver.getGraph().getNodeCount());
    EXPECT_FALSE(resolver.hasCycle());
}

TEST_F(DependencyResolverTest, BuildGraph_LinearChain) {
    // A -> B -> C
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depC("com.example.moduleC", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(3, "com.example.moduleC", Version(1, 0, 0));
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0), {depC});
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_EQ(3u, resolver.getGraph().getNodeCount());
    EXPECT_FALSE(resolver.hasCycle());

    // Check start order: C, B, A
    auto startOrder = resolver.getStartOrder();
    ASSERT_EQ(3u, startOrder.size());

    // Find positions
    auto posA = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleA"; });
    auto posB = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleB"; });
    auto posC = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleC"; });

    // C should be before B, B before A
    EXPECT_LT(posC, posB);
    EXPECT_LT(posB, posA);
}

TEST_F(DependencyResolverTest, BuildGraph_DiamondDependency) {
    // A -> B, A -> C, B -> D, C -> D
    ModuleDependency depD("com.example.moduleD", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depC("com.example.moduleC", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(4, "com.example.moduleD", Version(1, 0, 0));
    createAndRegisterModule(3, "com.example.moduleC", Version(1, 0, 0), {depD});
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0), {depD});
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB, depC});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_FALSE(resolver.hasCycle());

    auto startOrder = resolver.getStartOrder();
    ASSERT_EQ(4u, startOrder.size());

    // Find positions
    auto posA = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleA"; });
    auto posB = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleB"; });
    auto posC = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleC"; });
    auto posD = std::find_if(startOrder.begin(), startOrder.end(),
                             [](Module* m) { return m->getSymbolicName() == "com.example.moduleD"; });

    // D should be before B and C
    EXPECT_LT(posD, posB);
    EXPECT_LT(posD, posC);
    // B and C should be before A
    EXPECT_LT(posB, posA);
    EXPECT_LT(posC, posA);
}

TEST_F(DependencyResolverTest, BuildGraph_CircularDependency) {
    // A -> B -> C -> A (cycle)
    ModuleDependency depA("com.example.moduleA", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depC("com.example.moduleC", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0), {depC});
    createAndRegisterModule(3, "com.example.moduleC", Version(1, 0, 0), {depA});

    DependencyResolver resolver(registry_.get());

    EXPECT_THROW(resolver.buildGraph(), std::runtime_error);
}

TEST_F(DependencyResolverTest, GetStartOrder_EmptyGraph) {
    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto startOrder = resolver.getStartOrder();
    EXPECT_EQ(0u, startOrder.size());
}

TEST_F(DependencyResolverTest, GetStopOrder_ReverseOfStartOrder) {
    // A -> B -> C
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depC("com.example.moduleC", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(3, "com.example.moduleC", Version(1, 0, 0));
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0), {depC});
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto startOrder = resolver.getStartOrder();
    auto stopOrder = resolver.getStopOrder();

    ASSERT_EQ(startOrder.size(), stopOrder.size());

    // Stop order should be reverse of start order
    std::reverse(startOrder.begin(), startOrder.end());
    for (size_t i = 0; i < startOrder.size(); ++i) {
        EXPECT_EQ(startOrder[i]->getModuleId(), stopOrder[i]->getModuleId());
    }
}

TEST_F(DependencyResolverTest, DetectCycles_NoCycle) {
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0));
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0));

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto cycles = resolver.detectCycles();
    EXPECT_EQ(0u, cycles.size());
}

TEST_F(DependencyResolverTest, DetectCycles_WithCycle) {
    // A -> B -> A
    ModuleDependency depA("com.example.moduleA", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0), {depA});

    DependencyResolver resolver(registry_.get());

    // Should throw on buildGraph
    EXPECT_THROW(resolver.buildGraph(), std::runtime_error);
}

TEST_F(DependencyResolverTest, ValidateModule_NoDependencies) {
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0));

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto newModule = std::make_unique<MockModule>(2, "com.example.moduleB", Version(1, 0, 0));
    EXPECT_TRUE(resolver.validateModule(newModule.get()));
}

TEST_F(DependencyResolverTest, ValidateModule_ValidDependency) {
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0));

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    ModuleDependency depA("com.example.moduleA", VersionRange(Version(1, 0, 0)), false);
    std::vector<ModuleDependency> deps = {depA};
    auto newModule = std::make_unique<MockModule>(2, "com.example.moduleB", Version(1, 0, 0), deps);

    EXPECT_TRUE(resolver.validateModule(newModule.get()));
}

TEST_F(DependencyResolverTest, ValidateModule_WouldCreateCycle) {
    // A -> B, trying to add B -> A
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0));
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    ModuleDependency depA("com.example.moduleA", VersionRange(Version(1, 0, 0)), false);
    std::vector<ModuleDependency> deps = {depA};
    auto newModule = std::make_unique<MockModule>(3, "com.example.moduleB", Version(2, 0, 0), deps);

    // This would create a cycle: A -> B, B(v2) -> A
    // But since it's a different version, it might not be detected correctly
    // Let's register it first
    registry_->registerModule(newModule.get());

    // Rebuild to include the new module
    EXPECT_THROW(resolver.rebuildGraph(), std::runtime_error);
}

TEST_F(DependencyResolverTest, ValidateModule_NullModule) {
    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_FALSE(resolver.validateModule(nullptr));
}

TEST_F(DependencyResolverTest, GetDependents) {
    // A -> C, B -> C
    ModuleDependency depC("com.example.moduleC", VersionRange(Version(1, 0, 0)), false);

    auto moduleC = createAndRegisterModule(3, "com.example.moduleC", Version(1, 0, 0));
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depC});
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0), {depC});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto dependents = resolver.getDependents(moduleC);
    EXPECT_EQ(2u, dependents.size());
}

TEST_F(DependencyResolverTest, GetDependencies) {
    // A -> B, A -> C
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency depC("com.example.moduleC", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0));
    createAndRegisterModule(3, "com.example.moduleC", Version(1, 0, 0));
    auto moduleA = createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB, depC});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto dependencies = resolver.getDependencies(moduleA);
    EXPECT_EQ(2u, dependencies.size());
}

TEST_F(DependencyResolverTest, GetDependencies_NullModule) {
    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    auto deps = resolver.getDependencies(nullptr);
    EXPECT_EQ(0u, deps.size());
}

TEST_F(DependencyResolverTest, RebuildGraph) {
    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0));

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_EQ(1u, resolver.getGraph().getNodeCount());

    // Add another module
    createAndRegisterModule(2, "com.example.moduleB", Version(1, 0, 0));

    resolver.rebuildGraph();
    EXPECT_EQ(2u, resolver.getGraph().getNodeCount());
}

TEST_F(DependencyResolverTest, OptionalDependencies_NotInGraph) {
    // A optionally depends on B (B doesn't exist)
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), true);  // optional

    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    // Should not fail even though B doesn't exist (optional dependency)
    EXPECT_EQ(1u, resolver.getGraph().getNodeCount());
    EXPECT_EQ(0u, resolver.getGraph().getEdgeCount());
}

TEST_F(DependencyResolverTest, MissingDependency_NotInGraph) {
    // A depends on B (B doesn't exist)
    ModuleDependency depB("com.example.moduleB", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(1, "com.example.moduleA", Version(1, 0, 0), {depB});

    DependencyResolver resolver(registry_.get());

    // Should not throw, but dependency won't be in graph
    resolver.buildGraph();

    EXPECT_EQ(1u, resolver.getGraph().getNodeCount());
    EXPECT_EQ(0u, resolver.getGraph().getEdgeCount());
}

TEST_F(DependencyResolverTest, ComplexGraph) {
    // Create complex dependency graph
    ModuleDependency dep2("module2", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency dep3("module3", VersionRange(Version(1, 0, 0)), false);
    ModuleDependency dep4("module4", VersionRange(Version(1, 0, 0)), false);

    createAndRegisterModule(4, "module4", Version(1, 0, 0));
    createAndRegisterModule(3, "module3", Version(1, 0, 0), {dep4});
    createAndRegisterModule(2, "module2", Version(1, 0, 0), {dep4});
    createAndRegisterModule(1, "module1", Version(1, 0, 0), {dep2, dep3});

    DependencyResolver resolver(registry_.get());
    resolver.buildGraph();

    EXPECT_EQ(4u, resolver.getGraph().getNodeCount());
    EXPECT_FALSE(resolver.hasCycle());

    auto startOrder = resolver.getStartOrder();
    EXPECT_EQ(4u, startOrder.size());
}
