#ifndef CDMF_FRAMEWORK_H
#define CDMF_FRAMEWORK_H

#include "core/framework_types.h"
#include "core/framework_properties.h"
#include "module/module_types.h"
#include "utils/version.h"
#include <memory>
#include <vector>
#include <string>

namespace cdmf {

// Forward declarations
class IModuleContext;
class Module;

/**
 * @brief Framework interface - Main entry point for CDMF
 *
 * The Framework manages the complete lifecycle of the modular system:
 * - Module loading, installation, and lifecycle management
 * - Service registry for inter-module communication
 * - Event dispatching system
 * - Security and sandboxing
 * - IPC infrastructure for out-of-process modules
 *
 * Lifecycle states:
 * CREATED -> init() -> STARTING -> start() -> ACTIVE -> stop() -> STOPPING -> STOPPED
 *
 * Usage:
 * @code
 * FrameworkProperties props;
 * props.set("framework.id", "my-app");
 * auto framework = createFramework(props);
 * framework->init();
 * framework->start();
 * // ... use framework ...
 * framework->stop(5000);
 * framework->waitForStop();
 * @endcode
 */
class Framework {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~Framework() = default;

    // ==================================================================
    // Lifecycle Management
    // ==================================================================

    /**
     * @brief Initialize the framework
     *
     * Initializes all framework subsystems:
     * - Platform abstraction layer
     * - Module registry
     * - Service registry
     * - Event dispatcher
     * - Security manager (if enabled)
     * - IPC infrastructure (if enabled)
     *
     * State: CREATED -> STARTING -> ACTIVE
     *
     * @throws FrameworkException if initialization fails
     */
    virtual void init() = 0;

    /**
     * @brief Start the framework
     *
     * Starts all framework services and installs auto-start modules.
     *
     * State: ACTIVE (if already initialized)
     *
     * @throws FrameworkException if start fails
     */
    virtual void start() = 0;

    /**
     * @brief Stop the framework with timeout
     *
     * Initiates graceful shutdown:
     * 1. Stop all active modules (in reverse dependency order)
     * 2. Unregister all services
     * 3. Stop event dispatcher
     * 4. Release all resources
     *
     * State: ACTIVE -> STOPPING -> STOPPED
     *
     * @param timeout_ms Maximum wait time in milliseconds (0 = wait forever)
     * @throws FrameworkException if stop fails or times out
     */
    virtual void stop(int timeout_ms = 0) = 0;

    /**
     * @brief Wait for framework to completely stop
     *
     * Blocks until framework reaches STOPPED state.
     */
    virtual void waitForStop() = 0;

    /**
     * @brief Get current framework state
     * @return Current lifecycle state
     */
    virtual FrameworkState getState() const = 0;

    // ==================================================================
    // Module Management
    // ==================================================================

    /**
     * @brief Install a module from a shared library path
     *
     * Loads the module, parses manifest, validates signatures (if security enabled),
     * and registers it with the framework.
     *
     * State: Module is initially in INSTALLED state after successful installation.
     *
     * @param path Path to module shared library (.so, .dll, .dylib)
     * @return Pointer to installed module
     * @throws ModuleException if installation fails
     * @throws SecurityException if signature verification fails
     */
    virtual Module* installModule(const std::string& path) = 0;

    /**
     * @brief Install a module with explicit manifest path
     *
     * Loads the module from library path, parses manifest from specified path,
     * and registers it with the framework.
     *
     * @param libraryPath Path to module shared library (.so, .dll, .dylib)
     * @param manifestPath Path to module manifest JSON file
     * @return Pointer to installed module
     * @throws ModuleException if installation fails
     */
    virtual Module* installModule(const std::string& libraryPath, const std::string& manifestPath) = 0;

    /**
     * @brief Update a module to a new version
     *
     * Stops the old module, installs new version, and starts it (if it was active).
     *
     * @param module Module to update
     * @param newPath Path to new module version
     * @throws ModuleException if update fails
     */
    virtual void updateModule(Module* module, const std::string& newPath) = 0;

    /**
     * @brief Uninstall a module
     *
     * Stops the module (if active), unregisters all services, and removes from framework.
     *
     * @param module Module to uninstall
     * @throws ModuleException if uninstall fails
     */
    virtual void uninstallModule(Module* module) = 0;

    /**
     * @brief Get all installed modules
     * @return Vector of all modules (in any state)
     */
    virtual std::vector<Module*> getModules() const = 0;

    /**
     * @brief Get a module by symbolic name and version
     *
     * @param symbolicName Module symbolic name
     * @param version Specific version to find
     * @return Pointer to module, or nullptr if not found
     */
    virtual Module* getModule(const std::string& symbolicName, const Version& version) const = 0;

    /**
     * @brief Get a module by symbolic name (highest version)
     *
     * @param symbolicName Module symbolic name
     * @return Pointer to module with highest version, or nullptr if not found
     */
    virtual Module* getModule(const std::string& symbolicName) const = 0;

    // ==================================================================
    // Framework Context and Properties
    // ==================================================================

    /**
     * @brief Get the framework's module context
     *
     * Provides access to framework-level services and operations.
     *
     * @return Pointer to framework context (system module context)
     */
    virtual IModuleContext* getContext() = 0;

    /**
     * @brief Get framework properties
     * @return Reference to framework configuration
     */
    virtual FrameworkProperties& getProperties() = 0;

    /**
     * @brief Get framework properties (const)
     * @return Const reference to framework configuration
     */
    virtual const FrameworkProperties& getProperties() const = 0;

    // ==================================================================
    // Framework Event Listeners
    // ==================================================================

    /**
     * @brief Add a framework listener
     *
     * Framework listeners receive notifications about:
     * - Framework lifecycle events (STARTED, STOPPED)
     * - Module events (INSTALLED, STARTED, STOPPED, UNINSTALLED)
     * - Service events (REGISTERED, UNREGISTERED, MODIFIED)
     *
     * @param listener Framework event listener
     */
    virtual void addFrameworkListener(IFrameworkListener* listener) = 0;

    /**
     * @brief Remove a framework listener
     * @param listener Framework event listener to remove
     */
    virtual void removeFrameworkListener(IFrameworkListener* listener) = 0;
};

// Forward declaration - defined in event_listener.h
class IFrameworkListener;

// ==================================================================
// Factory Functions
// ==================================================================

/**
 * @brief Create a new framework instance
 *
 * Creates and configures a new CDMF framework instance with the specified properties.
 *
 * @param properties Framework configuration
 * @return Unique pointer to framework instance
 * @throws FrameworkException if creation fails
 */
std::unique_ptr<Framework> createFramework(const FrameworkProperties& properties);

/**
 * @brief Create a framework with default properties
 *
 * Uses sensible defaults for all configuration options.
 *
 * @return Unique pointer to framework instance
 */
std::unique_ptr<Framework> createFramework();

} // namespace cdmf

#endif // CDMF_FRAMEWORK_H
