#include "core/framework.h"
#include "core/event.h"
#include "core/event_listener.h"
#include "core/event_dispatcher.h"
#include "module/module_registry.h"
#include "module/module_impl.h"
#include "module/module_handle.h"
#include "module/manifest_parser.h"
#include "module/module_reloader.h"
#include "module/dependency_resolver.h"
#include "service/service_registry.h"
#include "platform/platform_abstraction.h"
#include "utils/log.h"
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace cdmf {

// Forward declaration
class FrameworkImpl;

/**
 * @brief Framework context implementation
 *
 * Provides the system module context for framework-level operations.
 * This is the context returned by Framework::getContext().
 */
class FrameworkContext : public IModuleContext {
public:
    explicit FrameworkContext(FrameworkImpl* framework)
        : framework_(framework) {}

    // Module Information
    Module* getModule() override {
        return nullptr; // Framework context doesn't have an associated module
    }

    const FrameworkProperties& getProperties() const override;

    std::string getProperty(const std::string& key) const override;

    // Service Operations
    ServiceRegistration registerService(
        const std::string& interfaceName,
        void* service,
        const Properties& props = Properties()) override;

    std::vector<ServiceReference> getServiceReferences(
        const std::string& interfaceName,
        const std::string& filter = "") const override;

    ServiceReference getServiceReference(
        const std::string& interfaceName) const override;

    std::shared_ptr<void> getService(const ServiceReference& ref) override;

    bool ungetService(const ServiceReference& ref) override;

    // Event Operations
    void addEventListener(
        IEventListener* listener,
        const EventFilter& filter = EventFilter(),
        int priority = 0,
        bool synchronous = false) override;

    void removeEventListener(IEventListener* listener) override;

    void fireEvent(const Event& event) override;

    void fireEventSync(const Event& event) override;

    // Module Operations
    Module* installModule(const std::string& location) override;

    std::vector<Module*> getModules() const override;

    Module* getModule(uint64_t moduleId) const override;

    Module* getModule(const std::string& symbolicName) const override;

private:
    FrameworkImpl* framework_;
};

/**
 * @brief Framework implementation
 *
 * Concrete implementation of the Framework interface.
 * Manages all framework subsystems and module lifecycle.
 */
class FrameworkImpl : public Framework {
public:
    /**
     * @brief Construct framework with properties
     */
    explicit FrameworkImpl(const FrameworkProperties& properties)
        : properties_(properties)
        , state_(FrameworkState::CREATED)
        , stopRequested_(false)
        , moduleReloader_(nullptr)
        , dependencyResolver_(nullptr) {

        LOGI("Creating CDMF Framework");
    }

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~FrameworkImpl() override {
        if (state_ == FrameworkState::ACTIVE) {
            LOGW("Framework destroyed while still ACTIVE - forcing shutdown");
            try {
                stop(5000);
            } catch (...) {
                LOGE("Exception during framework shutdown in destructor");
            }
        }
    }

    // ==================================================================
    // Lifecycle Management
    // ==================================================================

    void init() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != FrameworkState::CREATED) {
            throw std::runtime_error("Framework already initialized");
        }

        LOGI("Initializing framework subsystems...");
        state_ = FrameworkState::STARTING;

        try {
            // Initialize platform abstraction
            LOGI("  - Platform abstraction layer");
            platformAbstraction_ = std::make_unique<platform::PlatformAbstraction>();

            // Initialize event dispatcher
            size_t threadPoolSize = properties_.getInt("framework.event.thread.pool.size", 8);
            LOGI_FMT("  - Event dispatcher (thread pool size: " << threadPoolSize << ")");
            eventDispatcher_ = std::make_unique<EventDispatcher>(threadPoolSize);
            eventDispatcher_->start();

            // Initialize service registry
            LOGI("  - Service registry");
            serviceRegistry_ = std::make_unique<ServiceRegistry>();

            // Initialize module registry
            LOGI("  - Module registry");
            moduleRegistry_ = std::make_unique<ModuleRegistry>();

            // Initialize dependency resolver
            LOGI("  - Dependency resolver");
            dependencyResolver_ = std::make_unique<DependencyResolver>(moduleRegistry_.get());

            // Create framework context (system module context)
            LOGI("  - Framework context");
            createFrameworkContext();

            // Initialize module reloader
            int pollInterval = properties_.getInt("framework.modules.reload.poll.interval", 1000);
            LOGI_FMT("  - Module reloader (poll interval: " << pollInterval << "ms)");
            moduleReloader_ = std::make_unique<ModuleReloader>(this, pollInterval);

            // Check if auto-reload is enabled
            bool autoReloadEnabled = properties_.getBool("framework.modules.auto.reload", false);
            moduleReloader_->setEnabled(autoReloadEnabled);

            if (autoReloadEnabled) {
                moduleReloader_->start();
                LOGI("  - Auto-reload enabled and started");
            } else {
                LOGI("  - Auto-reload disabled");
            }

            state_ = FrameworkState::ACTIVE;
            LOGI("Framework initialization complete");

            // Fire framework started event
            fireFrameworkEvent(FrameworkEventType::STARTED, nullptr, "Framework initialized");

        } catch (const std::exception& e) {
            state_ = FrameworkState::STOPPED;
            LOGE_FMT("Framework initialization failed: " << e.what());
            throw;
        }
    }

    void start() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != FrameworkState::ACTIVE) {
            if (state_ == FrameworkState::CREATED) {
                // Auto-initialize if not already done
                mutex_.unlock();
                init();
                mutex_.lock();
            } else {
                throw std::runtime_error("Framework not in ACTIVE state");
            }
        }

        LOGI("Framework started and ready");
    }

    void stop(int timeout_ms) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (state_ != FrameworkState::ACTIVE) {
                LOGW("Framework not active, ignoring stop request");
                return;
            }

            LOGI("Stopping framework...");
            state_ = FrameworkState::STOPPING;
            stopRequested_ = true;
        }

        // Fire framework stopping event
        fireFrameworkEvent(FrameworkEventType::STOPPED, nullptr, "Framework stopping");

        try {
            // Stop module reloader first
            LOGI("  - Stopping module reloader");
            if (moduleReloader_) {
                moduleReloader_->stop();
                moduleReloader_.reset();
            }

            // Stop all active modules (in reverse dependency order)
            LOGI("  - Stopping all active modules");
            stopAllModules(timeout_ms);

            // Stop event dispatcher
            LOGI("  - Stopping event dispatcher");
            if (eventDispatcher_) {
                eventDispatcher_->stop();
            }

            // Clear service registry
            LOGI("  - Clearing service registry");
            serviceRegistry_.reset();

            // Clear dependency resolver
            LOGI("  - Clearing dependency resolver");
            dependencyResolver_.reset();

            // Clear module registry
            LOGI("  - Clearing module registry");
            moduleRegistry_.reset();

            // Cleanup framework context
            LOGI("  - Cleaning up framework context");
            frameworkContext_.reset();

            // Cleanup platform abstraction
            LOGI("  - Cleaning up platform abstraction");
            platformAbstraction_.reset();

            {
                std::lock_guard<std::mutex> lock(mutex_);
                state_ = FrameworkState::STOPPED;
            }

            LOGI("Framework stopped successfully");

            // Notify waiters
            stopCondition_.notify_all();

        } catch (const std::exception& e) {
            LOGE_FMT("Error during framework shutdown: " << e.what());
            {
                std::lock_guard<std::mutex> lock(mutex_);
                state_ = FrameworkState::STOPPED;
            }
            stopCondition_.notify_all();
            throw;
        }
    }

    void waitForStop() override {
        std::unique_lock<std::mutex> lock(mutex_);
        stopCondition_.wait(lock, [this] { return state_ == FrameworkState::STOPPED; });
    }

    FrameworkState getState() const override {
        return state_;
    }

    // ==================================================================
    // Module Management
    // ==================================================================

    Module* installModule(const std::string& path) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != FrameworkState::ACTIVE) {
            throw std::runtime_error("Framework not active");
        }

        LOGI_FMT("Installing module from: " << path);

        try {
            // Derive manifest path from library path
            // ./modules/config_service_module.so -> ./modules/config_service_module.manifest.json
            std::string manifestPath = path;
            size_t lastDot = manifestPath.find_last_of('.');
            if (lastDot != std::string::npos) {
                manifestPath = manifestPath.substr(0, lastDot);
            }
            manifestPath += ".manifest.json";

            LOGI_FMT("Reading manifest from: " << manifestPath);

            // Parse manifest first
            auto manifest = ManifestParser::parseFile(manifestPath);

            // Create module handle (this will load the shared library)
            auto moduleHandle = std::make_unique<ModuleHandle>(path);

            // Generate module ID
            uint64_t moduleId = moduleRegistry_->generateModuleId();

            // Create module instance
            auto module = std::make_unique<ModuleImpl>(
                moduleId,
                std::move(moduleHandle),
                manifest,
                this
            );

            Module* modulePtr = module.get();

            // Validate module dependencies before installation
            if (dependencyResolver_) {
                if (!dependencyResolver_->validateModule(modulePtr)) {
                    auto cycles = dependencyResolver_->detectCycles();
                    std::string cycleInfo;
                    for (const auto& cycle : cycles) {
                        cycleInfo += cycle.toString() + "; ";
                    }
                    throw std::runtime_error("Module creates circular dependency: " + cycleInfo);
                }
            }

            // Store module ownership
            modules_[moduleId] = std::move(module);

            // Register with module registry
            moduleRegistry_->registerModule(modulePtr);

            // Rebuild dependency graph
            if (dependencyResolver_) {
                try {
                    dependencyResolver_->rebuildGraph();
                } catch (const std::exception& e) {
                    // If dependency graph building fails, unregister the module
                    moduleRegistry_->unregisterModule(moduleId);
                    modules_.erase(moduleId);
                    throw std::runtime_error(std::string("Failed to build dependency graph: ") + e.what());
                }
            }

            LOGI_FMT("Module installed: " << modulePtr->getSymbolicName()
                     << " v" << modulePtr->getVersion().toString());

            // Resolve module dependencies
            if (manifest.dependencies.empty()) {
                LOGI_FMT("  Auto-resolving module (no dependencies)");
                // Transition from INSTALLED to RESOLVED
                static_cast<ModuleImpl*>(modulePtr)->transitionTo(ModuleState::RESOLVED);
            } else {
                // Check if all dependencies are satisfied
                bool allDependenciesSatisfied = true;
                std::string missingDeps;

                for (const auto& dep : manifest.dependencies) {
                    Module* depModule = moduleRegistry_->findCompatibleModule(
                        dep.symbolicName,
                        dep.versionRange
                    );

                    if (!depModule) {
                        if (!dep.optional) {
                            allDependenciesSatisfied = false;
                            if (!missingDeps.empty()) {
                                missingDeps += ", ";
                            }
                            missingDeps += dep.symbolicName + " " + dep.versionRange.toString();
                        }
                    } else {
                        LOGI_FMT("    Dependency satisfied: " << dep.symbolicName
                                 << " " << dep.versionRange.toString()
                                 << " -> found " << depModule->getSymbolicName()
                                 << " v" << depModule->getVersion().toString());
                    }
                }

                if (allDependenciesSatisfied) {
                    LOGI_FMT("  All dependencies satisfied, resolving module");
                    static_cast<ModuleImpl*>(modulePtr)->transitionTo(ModuleState::RESOLVED);
                } else {
                    LOGW_FMT("  Module has unsatisfied dependencies: " << missingDeps);
                    LOGW("  Module remains in INSTALLED state until dependencies are resolved");
                }
            }

            // Auto-start module if enabled (both globally and in manifest)
            bool globalAutoStart = properties_.getBool("framework.modules.auto.start", true);
            if (globalAutoStart && manifest.autoStart && modulePtr->getState() == ModuleState::RESOLVED) {
                try {
                    LOGI_FMT("  Auto-starting module (auto-start enabled in manifest)");
                    modulePtr->start();
                    LOGI("  Module auto-started successfully");
                } catch (const std::exception& e) {
                    LOGW_FMT("  Failed to auto-start module: " << e.what());
                    LOGW("  Module installed but not started");
                    // Don't fail installation if auto-start fails
                }
            }

            // Fire module installed event
            fireFrameworkEvent(FrameworkEventType::MODULE_INSTALLED, modulePtr, "Module installed");

            // Register module with reloader for auto-reload monitoring
            if (moduleReloader_ && moduleReloader_->isEnabled()) {
                moduleReloader_->registerModule(modulePtr, path, manifestPath);
            }

            return modulePtr;

        } catch (const std::exception& e) {
            LOGE_FMT("Failed to install module: " << e.what());
            throw;
        }
    }

    Module* installModule(const std::string& libraryPath, const std::string& manifestPath) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != FrameworkState::ACTIVE) {
            throw std::runtime_error("Framework not active");
        }

        LOGI_FMT("Installing module from: " << libraryPath);
        LOGI_FMT("Using manifest from: " << manifestPath);

        try {
            // Parse manifest from specified path
            auto manifest = ManifestParser::parseFile(manifestPath);

            // Create module handle (this will load the shared library)
            auto moduleHandle = std::make_unique<ModuleHandle>(libraryPath);

            // Generate module ID
            uint64_t moduleId = moduleRegistry_->generateModuleId();

            // Create module instance
            auto module = std::make_unique<ModuleImpl>(
                moduleId,
                std::move(moduleHandle),
                manifest,
                this
            );

            Module* modulePtr = module.get();

            // Validate module dependencies before installation
            if (dependencyResolver_) {
                if (!dependencyResolver_->validateModule(modulePtr)) {
                    auto cycles = dependencyResolver_->detectCycles();
                    std::string cycleInfo;
                    for (const auto& cycle : cycles) {
                        cycleInfo += cycle.toString() + "; ";
                    }
                    throw std::runtime_error("Module creates circular dependency: " + cycleInfo);
                }
            }

            // Store module ownership
            modules_[moduleId] = std::move(module);

            // Register with module registry
            moduleRegistry_->registerModule(modulePtr);

            // Rebuild dependency graph
            if (dependencyResolver_) {
                try {
                    dependencyResolver_->rebuildGraph();
                } catch (const std::exception& e) {
                    // If dependency graph building fails, unregister the module
                    moduleRegistry_->unregisterModule(moduleId);
                    modules_.erase(moduleId);
                    throw std::runtime_error(std::string("Failed to build dependency graph: ") + e.what());
                }
            }

            LOGI_FMT("Module installed: " << modulePtr->getSymbolicName()
                     << " v" << modulePtr->getVersion().toString());

            // Resolve module dependencies
            if (manifest.dependencies.empty()) {
                LOGI_FMT("  Auto-resolving module (no dependencies)");
                // Transition from INSTALLED to RESOLVED
                static_cast<ModuleImpl*>(modulePtr)->transitionTo(ModuleState::RESOLVED);
            } else {
                // Check if all dependencies are satisfied
                bool allDependenciesSatisfied = true;
                std::string missingDeps;

                for (const auto& dep : manifest.dependencies) {
                    Module* depModule = moduleRegistry_->findCompatibleModule(
                        dep.symbolicName,
                        dep.versionRange
                    );

                    if (!depModule) {
                        if (!dep.optional) {
                            allDependenciesSatisfied = false;
                            if (!missingDeps.empty()) {
                                missingDeps += ", ";
                            }
                            missingDeps += dep.symbolicName + " " + dep.versionRange.toString();
                        }
                    } else {
                        LOGI_FMT("    Dependency satisfied: " << dep.symbolicName
                                 << " " << dep.versionRange.toString()
                                 << " -> found " << depModule->getSymbolicName()
                                 << " v" << depModule->getVersion().toString());
                    }
                }

                if (allDependenciesSatisfied) {
                    LOGI_FMT("  All dependencies satisfied, resolving module");
                    static_cast<ModuleImpl*>(modulePtr)->transitionTo(ModuleState::RESOLVED);
                } else {
                    LOGW_FMT("  Module has unsatisfied dependencies: " << missingDeps);
                    LOGW("  Module remains in INSTALLED state until dependencies are resolved");
                }
            }

            // Auto-start module if enabled (both globally and in manifest)
            bool globalAutoStart = properties_.getBool("framework.modules.auto.start", true);
            if (globalAutoStart && manifest.autoStart && modulePtr->getState() == ModuleState::RESOLVED) {
                try {
                    LOGI_FMT("  Auto-starting module (auto-start enabled in manifest)");
                    modulePtr->start();
                    LOGI("  Module auto-started successfully");
                } catch (const std::exception& e) {
                    LOGW_FMT("  Failed to auto-start module: " << e.what());
                    LOGW("  Module installed but not started");
                    // Don't fail installation if auto-start fails
                }
            }

            // Fire module installed event
            fireFrameworkEvent(FrameworkEventType::MODULE_INSTALLED, modulePtr, "Module installed");

            // Register module with reloader for auto-reload monitoring
            if (moduleReloader_ && moduleReloader_->isEnabled()) {
                moduleReloader_->registerModule(modulePtr, libraryPath, manifestPath);
            }

            return modulePtr;

        } catch (const std::exception& e) {
            LOGE_FMT("Failed to install module: " << e.what());
            throw;
        }
    }

    void updateModule(Module* module, const std::string& newPath) override {
        if (!module) {
            throw std::invalid_argument("Module cannot be null");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        LOGI_FMT("Updating module: " << module->getSymbolicName());

        // Stop module if active
        bool wasActive = (module->getState() == ModuleState::ACTIVE);
        if (wasActive) {
            module->stop();
        }

        // Get the module implementation to access manifest
        ModuleImpl* moduleImpl = static_cast<ModuleImpl*>(module);

        // Check if we need to reload the library
        // Only reload if the library file path is different from current path
        std::string currentPath = module->getLocation();
        bool needsLibraryReload = (newPath != currentPath);

        if (needsLibraryReload) {
            // Update module (reloads the library)
            LOGI_FMT("  Reloading library from: " << newPath);
            module->update(newPath);
        } else {
            LOGI_FMT("  Library unchanged, skipping reload");
        }

        // Re-read the manifest from disk to get updated dependencies
        // Get manifest path from module reloader
        LOGI("  Getting manifest path from module reloader...");
        std::string manifestPath;
        if (moduleReloader_) {
            manifestPath = moduleReloader_->getManifestPath(module);
            LOGI_FMT("  Got manifest path: " << manifestPath);
        }

        LOGI("  Parsing manifest...");
        ModuleManifest manifest;
        try {
            if (!manifestPath.empty()) {
                // Re-parse manifest from file to get latest changes
                LOGI_FMT("  Reading manifest file: " << manifestPath);
                manifest = ManifestParser::parseFile(manifestPath);
                LOGI_FMT("  Re-loaded manifest from: " << manifestPath);
            } else {
                // Fallback to in-memory manifest if path not available
                LOGI("  Using in-memory manifest");
                auto manifestJson = moduleImpl->getManifest();
                manifest = ManifestParser::parse(manifestJson);
                LOGW("  Using in-memory manifest (path not available)");
            }
        } catch (const std::exception& e) {
            LOGE_FMT("  Failed to parse manifest: " << e.what());
            throw;
        }

        // Rebuild dependency graph
        if (dependencyResolver_) {
            try {
                dependencyResolver_->rebuildGraph();
            } catch (const std::exception& e) {
                LOGW_FMT("Failed to rebuild dependency graph: " << e.what());
            }
        }

        // Re-resolve dependencies after update
        if (manifest.dependencies.empty()) {
            LOGI_FMT("  Auto-resolving module (no dependencies)");
            moduleImpl->transitionTo(ModuleState::RESOLVED);
        } else {
            // Check if all dependencies are satisfied
            bool allDependenciesSatisfied = true;
            std::string missingDeps;

            for (const auto& dep : manifest.dependencies) {
                Module* depModule = moduleRegistry_->findCompatibleModule(
                    dep.symbolicName,
                    dep.versionRange
                );

                if (!depModule) {
                    if (!dep.optional) {
                        allDependenciesSatisfied = false;
                        if (!missingDeps.empty()) {
                            missingDeps += ", ";
                        }
                        missingDeps += dep.symbolicName + " " + dep.versionRange.toString();
                    }
                } else {
                    LOGI_FMT("    Dependency satisfied: " << dep.symbolicName
                             << " " << dep.versionRange.toString()
                             << " -> found " << depModule->getSymbolicName()
                             << " v" << depModule->getVersion().toString());
                }
            }

            if (allDependenciesSatisfied) {
                LOGI_FMT("  All dependencies satisfied, resolving module");
                moduleImpl->transitionTo(ModuleState::RESOLVED);
            } else {
                LOGW_FMT("  Module has unsatisfied dependencies: " << missingDeps);
                LOGW("  Module remains in INSTALLED state until dependencies are resolved");
            }
        }

        // Restart if it was active and is now resolved
        LOGI_FMT("  Checking if module needs restart (wasActive=" << wasActive
                 << ", currentState=" << static_cast<int>(module->getState()) << ")");
        if (wasActive && module->getState() == ModuleState::RESOLVED) {
            LOGI("  Restarting module...");
            module->start();
            LOGI("  Module restarted");
        }

        LOGI("  Firing MODULE_UPDATED event...");
        fireFrameworkEvent(FrameworkEventType::MODULE_UPDATED, module, "Module updated");
        LOGI("  Module update complete");
    }

    void uninstallModule(Module* module) override {
        if (!module) {
            throw std::invalid_argument("Module cannot be null");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        LOGI_FMT("Uninstalling module: " << module->getSymbolicName());

        // Unregister from module reloader
        if (moduleReloader_) {
            moduleReloader_->unregisterModule(module);
        }

        // Uninstall the module (this will stop it if active)
        module->uninstall();

        uint64_t moduleId = module->getModuleId();

        // Remove from registry
        moduleRegistry_->unregisterModule(moduleId);

        // Rebuild dependency graph
        if (dependencyResolver_) {
            try {
                dependencyResolver_->rebuildGraph();
            } catch (const std::exception& e) {
                LOGW_FMT("Failed to rebuild dependency graph after uninstall: " << e.what());
            }
        }

        // Remove from ownership storage
        modules_.erase(moduleId);

        fireFrameworkEvent(FrameworkEventType::MODULE_UNINSTALLED, module, "Module uninstalled");
    }

    std::vector<Module*> getModules() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return moduleRegistry_->getAllModules();
    }

    Module* getModule(const std::string& symbolicName, const Version& version) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return moduleRegistry_->getModule(symbolicName, version);
    }

    Module* getModule(const std::string& symbolicName) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return moduleRegistry_->getModule(symbolicName);
    }

    // ==================================================================
    // Context and Properties
    // ==================================================================

    IModuleContext* getContext() override {
        return frameworkContext_.get();
    }

    FrameworkProperties& getProperties() override {
        return properties_;
    }

    const FrameworkProperties& getProperties() const override {
        return properties_;
    }

    // ==================================================================
    // Framework Listeners
    // ==================================================================

    void addFrameworkListener(IFrameworkListener* listener) override {
        if (!listener) return;

        std::lock_guard<std::mutex> lock(listenerMutex_);
        listeners_.push_back(listener);
    }

    void removeFrameworkListener(IFrameworkListener* listener) override {
        if (!listener) return;

        std::lock_guard<std::mutex> lock(listenerMutex_);
        listeners_.erase(
            std::remove(listeners_.begin(), listeners_.end(), listener),
            listeners_.end()
        );
    }

    // ==================================================================
    // Internal API (for ModuleImpl and framework components)
    // ==================================================================

    EventDispatcher* getEventDispatcher() {
        return eventDispatcher_.get();
    }

    ServiceRegistry* getServiceRegistry() {
        return serviceRegistry_.get();
    }

    ModuleRegistry* getModuleRegistry() {
        return moduleRegistry_.get();
    }

    DependencyResolver* getDependencyResolver() {
        return dependencyResolver_.get();
    }

private:
    /**
     * @brief Create the framework context (system module context)
     */
    void createFrameworkContext() {
        frameworkContext_ = std::make_unique<FrameworkContext>(this);
    }

    /**
     * @brief Stop all active modules in reverse dependency order
     */
    void stopAllModules(int timeout_ms) {
        std::vector<Module*> modules;

        // Get modules in reverse dependency order (dependents stop first)
        if (dependencyResolver_) {
            try {
                modules = dependencyResolver_->getStopOrder();
                LOGI("Stopping modules in dependency order (dependents first)");
            } catch (const std::exception& e) {
                LOGW_FMT("Failed to get dependency-based stop order: " << e.what()
                         << " - using reverse installation order");
                modules = moduleRegistry_->getAllModules();
                std::reverse(modules.begin(), modules.end());
            }
        } else {
            // Fallback: stop in reverse installation order
            modules = moduleRegistry_->getAllModules();
            std::reverse(modules.begin(), modules.end());
        }

        for (auto* module : modules) {
            if (module->getState() == ModuleState::ACTIVE) {
                try {
                    LOGI_FMT("  - Stopping module: " << module->getSymbolicName());
                    module->stop();
                } catch (const std::exception& e) {
                    LOGE_FMT("Failed to stop module " << module->getSymbolicName()
                             << ": " << e.what());
                }
            }
        }
    }

    /**
     * @brief Fire a framework event to all listeners
     */
    void fireFrameworkEvent(FrameworkEventType type, Module* module, const std::string& message) {
        std::vector<IFrameworkListener*> listenersCopy;
        {
            std::lock_guard<std::mutex> lock(listenerMutex_);
            listenersCopy = listeners_;
        }

        // Create event
        Event event(frameworkEventTypeToString(type), this);
        event.setProperty("eventType", static_cast<int>(type));
        event.setProperty("message", message);
        if (module) {
            event.setProperty("module", module);
        }

        for (auto* listener : listenersCopy) {
            try {
                listener->frameworkEvent(event);
            } catch (const std::exception& e) {
                LOGE_FMT("Framework listener threw exception: " << e.what());
            }
        }
    }

private:
    // Configuration
    FrameworkProperties properties_;

    // State management
    std::atomic<FrameworkState> state_;
    std::atomic<bool> stopRequested_;
    mutable std::mutex mutex_;
    std::condition_variable stopCondition_;

    // Module ownership storage
    std::map<uint64_t, std::unique_ptr<Module>> modules_;

    // Framework subsystems
    std::unique_ptr<platform::PlatformAbstraction> platformAbstraction_;
    std::unique_ptr<EventDispatcher> eventDispatcher_;
    std::unique_ptr<ServiceRegistry> serviceRegistry_;
    std::unique_ptr<ModuleRegistry> moduleRegistry_;
    std::unique_ptr<DependencyResolver> dependencyResolver_;
    std::unique_ptr<IModuleContext> frameworkContext_;
    std::unique_ptr<ModuleReloader> moduleReloader_;

    // Framework listeners
    std::vector<IFrameworkListener*> listeners_;
    mutable std::mutex listenerMutex_;
};

// ==================================================================
// FrameworkContext Implementation
// ==================================================================

const FrameworkProperties& FrameworkContext::getProperties() const {
    return framework_->getProperties();
}

std::string FrameworkContext::getProperty(const std::string& key) const {
    return framework_->getProperties().getString(key);
}

ServiceRegistration FrameworkContext::registerService(
    const std::string& interfaceName,
    void* service,
    const Properties& props) {

    return framework_->getServiceRegistry()->registerService(
        interfaceName,
        service,
        props,
        nullptr  // Framework context has no owning module
    );
}

std::vector<ServiceReference> FrameworkContext::getServiceReferences(
    const std::string& interfaceName,
    const std::string& filter) const {

    return framework_->getServiceRegistry()->getServiceReferences(
        interfaceName,
        filter
    );
}

ServiceReference FrameworkContext::getServiceReference(
    const std::string& interfaceName) const {

    return framework_->getServiceRegistry()->getServiceReference(interfaceName);
}

std::shared_ptr<void> FrameworkContext::getService(const ServiceReference& ref) {
    return framework_->getServiceRegistry()->getService(ref);
}

bool FrameworkContext::ungetService(const ServiceReference& ref) {
    return framework_->getServiceRegistry()->ungetService(ref);
}

void FrameworkContext::addEventListener(
    IEventListener* listener,
    const EventFilter& filter,
    int priority,
    bool synchronous) {

    framework_->getEventDispatcher()->addEventListener(
        listener,
        filter,
        priority
    );
}

void FrameworkContext::removeEventListener(IEventListener* listener) {
    framework_->getEventDispatcher()->removeEventListener(listener);
}

void FrameworkContext::fireEvent(const Event& event) {
    framework_->getEventDispatcher()->fireEvent(event);
}

void FrameworkContext::fireEventSync(const Event& event) {
    framework_->getEventDispatcher()->fireEventSync(event);
}

Module* FrameworkContext::installModule(const std::string& location) {
    return framework_->installModule(location);
}

std::vector<Module*> FrameworkContext::getModules() const {
    return framework_->getModules();
}

Module* FrameworkContext::getModule(uint64_t moduleId) const {
    return framework_->getModuleRegistry()->getModule(moduleId);
}

Module* FrameworkContext::getModule(const std::string& symbolicName) const {
    return framework_->getModule(symbolicName);
}

// ==================================================================
// Factory Functions Implementation
// ==================================================================

std::unique_ptr<Framework> createFramework(const FrameworkProperties& properties) {
    return std::make_unique<FrameworkImpl>(properties);
}

std::unique_ptr<Framework> createFramework() {
    FrameworkProperties defaultProps;

    // Set default properties
    defaultProps.set("framework.id", "cdmf-default");
    defaultProps.set("framework.version", "1.0.0");
    defaultProps.set("framework.event.thread.pool.size", "8");
    defaultProps.set("framework.event.queue.size", "10000");

    return std::make_unique<FrameworkImpl>(defaultProps);
}

} // namespace cdmf
