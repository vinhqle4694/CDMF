#ifndef CDMF_MODULE_H
#define CDMF_MODULE_H

#include "module/module_types.h"
#include "module/module_context.h"
#include "utils/version.h"
#include "core/event_listener.h"
#include "service/service_types.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace cdmf {

/**
 * @brief Module interface
 *
 * Represents an installed module in the CDMF framework. Modules are the
 * fundamental unit of modularity, packaging code and resources as a deployable unit.
 *
 * Module lifecycle states:
 * 1. INSTALLED - Loaded into memory, manifest parsed
 * 2. RESOLVED - Dependencies satisfied, ready to start
 * 3. STARTING - In the process of starting (activator.start() executing)
 * 4. ACTIVE - Fully operational, services registered
 * 5. STOPPING - In the process of stopping (activator.stop() executing)
 * 6. UNINSTALLED - Removed from framework (terminal state)
 *
 * Module identity:
 * - Symbolic name: Unique identifier (e.g., "com.example.mymodule")
 * - Version: Semantic version (e.g., 1.2.3)
 * - Module ID: Framework-assigned unique numeric ID
 * - Location: Filesystem path to module shared library
 *
 * Module operations:
 * - start(): Transition RESOLVED → ACTIVE
 * - stop(): Transition ACTIVE → RESOLVED
 * - update(): Replace with new version
 * - uninstall(): Transition to UNINSTALLED (terminal)
 *
 * @note All operations are thread-safe
 * @note Module instances are managed by the framework, not user code
 */
class Module {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~Module() = default;

    // ==================================================================
    // Identity
    // ==================================================================

    /**
     * @brief Get module symbolic name
     *
     * The symbolic name uniquely identifies the module type.
     * Multiple versions of the same module share the same symbolic name.
     *
     * @return Symbolic name (e.g., "com.example.mymodule")
     */
    virtual std::string getSymbolicName() const = 0;

    /**
     * @brief Get module version
     * @return Semantic version
     */
    virtual Version getVersion() const = 0;

    /**
     * @brief Get module location
     *
     * The location is the filesystem path to the module's shared library.
     *
     * @return Absolute path to module shared library
     */
    virtual std::string getLocation() const = 0;

    /**
     * @brief Get module ID
     *
     * The module ID is a unique numeric identifier assigned by the framework
     * when the module is installed. Module IDs are never reused.
     *
     * @return Unique module ID (> 0)
     */
    virtual uint64_t getModuleId() const = 0;

    // ==================================================================
    // Lifecycle Operations
    // ==================================================================

    /**
     * @brief Start the module
     *
     * Transitions the module from RESOLVED to ACTIVE:
     * 1. Transition to STARTING
     * 2. Create module activator
     * 3. Create module context
     * 4. Call activator->start(context)
     * 5. Transition to ACTIVE
     *
     * If activator.start() throws, module transitions back to RESOLVED.
     *
     * @throws ModuleException if module is not in RESOLVED state
     * @throws ModuleException if activator.start() fails
     * @throws std::runtime_error if activator creation fails
     *
     * @note Thread-safe
     * @note Idempotent - calling on ACTIVE module is a no-op
     */
    virtual void start() = 0;

    /**
     * @brief Stop the module
     *
     * Transitions the module from ACTIVE to RESOLVED:
     * 1. Transition to STOPPING
     * 2. Call activator->stop(context)
     * 3. Unregister all services
     * 4. Remove all event listeners
     * 5. Destroy module context
     * 6. Destroy activator
     * 7. Transition to RESOLVED
     *
     * If activator.stop() throws, error is logged but module still stops.
     *
     * @note Thread-safe
     * @note Idempotent - calling on RESOLVED module is a no-op
     */
    virtual void stop() = 0;

    /**
     * @brief Update the module to a new version
     *
     * Replaces the module with a new version from the specified location:
     * 1. Stop the module (if ACTIVE)
     * 2. Unload old shared library
     * 3. Load new shared library from location
     * 4. Parse new manifest
     * 5. Re-resolve dependencies
     * 6. Start the module (if auto-start enabled)
     *
     * @param location Path to new module shared library
     * @throws ModuleException if update fails
     * @throws std::invalid_argument if location is invalid
     *
     * @note Thread-safe
     * @note Module ID and symbolic name remain unchanged
     */
    virtual void update(const std::string& location) = 0;

    /**
     * @brief Uninstall the module
     *
     * Permanently removes the module from the framework:
     * 1. Stop the module (if ACTIVE)
     * 2. Unregister all services
     * 3. Remove all event listeners
     * 4. Unload shared library
     * 5. Transition to UNINSTALLED
     * 6. Remove from module registry
     *
     * After uninstall, the Module object is invalid and must not be used.
     *
     * @throws ModuleException if uninstall fails
     *
     * @note Thread-safe
     * @note After uninstall, all references to this module become invalid
     */
    virtual void uninstall() = 0;

    // ==================================================================
    // State
    // ==================================================================

    /**
     * @brief Get current module state
     * @return Current lifecycle state
     */
    virtual ModuleState getState() const = 0;

    /**
     * @brief Check if module is active
     * @return true if state == ACTIVE
     */
    virtual bool isActive() const {
        return getState() == ModuleState::ACTIVE;
    }

    /**
     * @brief Check if module is resolved
     * @return true if state == RESOLVED
     */
    virtual bool isResolved() const {
        return getState() == ModuleState::RESOLVED;
    }

    // ==================================================================
    // Context
    // ==================================================================

    /**
     * @brief Get module context
     *
     * The context is only available while module is ACTIVE.
     *
     * @return Pointer to module context, or nullptr if module not ACTIVE
     */
    virtual IModuleContext* getContext() = 0;

    // ==================================================================
    // Services (PHASE_5)
    // ==================================================================

    /**
     * @brief Get services registered by this module
     * @return Vector of service registrations
     */
    virtual std::vector<ServiceRegistration> getRegisteredServices() const = 0;

    /**
     * @brief Get services in use by this module
     * @return Vector of service references
     */
    virtual std::vector<ServiceReference> getServicesInUse() const = 0;

    // ==================================================================
    // Metadata
    // ==================================================================

    /**
     * @brief Get module manifest (JSON)
     *
     * The manifest contains module metadata:
     * - symbolic-name, version, name, description, vendor
     * - dependencies (required modules)
     * - exported/imported packages
     * - services (provides/requires)
     * - security permissions
     *
     * @return Reference to parsed manifest JSON
     */
    virtual const nlohmann::json& getManifest() const = 0;

    /**
     * @brief Get module manifest headers as key-value pairs
     *
     * Convenience method to access manifest fields as strings.
     *
     * @return Map of header key → value
     */
    virtual std::map<std::string, std::string> getHeaders() const = 0;

    /**
     * @brief Get a specific manifest header value
     * @param key Header key
     * @return Header value, or empty string if not found
     */
    virtual std::string getHeader(const std::string& key) const {
        auto headers = getHeaders();
        auto it = headers.find(key);
        return (it != headers.end()) ? it->second : "";
    }

    // ==================================================================
    // Listeners (Module-specific events)
    // ==================================================================

    /**
     * @brief Add a module listener
     *
     * Module listeners receive events specific to this module:
     * - MODULE_STARTING, MODULE_STARTED
     * - MODULE_STOPPING, MODULE_STOPPED
     * - MODULE_UPDATED, MODULE_UNINSTALLED
     *
     * @param listener Module listener to add
     *
     * @note Listener is automatically removed when module uninstalls
     */
    virtual void addModuleListener(IModuleListener* listener) = 0;

    /**
     * @brief Remove a module listener
     * @param listener Module listener to remove
     */
    virtual void removeModuleListener(IModuleListener* listener) = 0;
};

/**
 * @brief Module exception
 *
 * Thrown when module operations fail.
 */
class ModuleException : public std::runtime_error {
public:
    explicit ModuleException(const std::string& message)
        : std::runtime_error(message) {}

    ModuleException(const std::string& message, ModuleState state)
        : std::runtime_error(message + " (state: " +
                            std::string(moduleStateToString(state)) + ")") {}
};

} // namespace cdmf

#endif // CDMF_MODULE_H
