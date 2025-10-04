#ifndef CDMF_MODULE_CONTEXT_H
#define CDMF_MODULE_CONTEXT_H

#include "module/module_types.h"
#include "core/framework_properties.h"
#include "core/event.h"
#include "core/event_filter.h"
#include "core/event_listener.h"
#include "utils/properties.h"
#include "service/service_types.h"
#include <string>
#include <vector>
#include <memory>

namespace cdmf {

/**
 * @brief Module context interface
 *
 * Provides modules with access to framework services and operations.
 * Each active module receives a context instance during activation.
 *
 * The context allows modules to:
 * - Register and consume services
 * - Fire and listen to events
 * - Access framework properties
 * - Install other modules
 * - Query module information
 *
 * Context lifecycle:
 * - Created: When module transitions to STARTING
 * - Valid: While module is ACTIVE
 * - Destroyed: When module transitions to STOPPING
 *
 * @note All operations are only valid while module is ACTIVE
 * @note Context is automatically destroyed when module stops
 */
class IModuleContext {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IModuleContext() = default;

    // ==================================================================
    // Module Information
    // ==================================================================

    /**
     * @brief Get the module associated with this context
     * @return Pointer to owning module
     */
    virtual Module* getModule() = 0;

    /**
     * @brief Get framework properties (read-only)
     * @return Reference to framework configuration
     */
    virtual const FrameworkProperties& getProperties() const = 0;

    /**
     * @brief Get a specific framework property
     * @param key Property key
     * @return Property value, or empty string if not found
     */
    virtual std::string getProperty(const std::string& key) const = 0;

    // ==================================================================
    // Service Operations (PHASE_5 - Service Registry)
    // ==================================================================

    /**
     * @brief Register a service with the framework
     *
     * Services are automatically unregistered when the module stops.
     *
     * @param interfaceName Fully qualified interface name (e.g., "com.example.ILogger")
     * @param service Pointer to service implementation (must remain valid until unregistered)
     * @param props Service properties (optional metadata)
     * @return Service registration handle
     * @throws std::invalid_argument if interfaceName is empty or service is null
     */
    virtual ServiceRegistration registerService(
        const std::string& interfaceName,
        void* service,
        const Properties& props = Properties()) = 0;

    /**
     * @brief Get all service references matching an interface
     *
     * @param interfaceName Interface name to match
     * @param filter LDAP-style filter for service properties (optional)
     * @return Vector of matching service references (may be empty)
     */
    virtual std::vector<ServiceReference> getServiceReferences(
        const std::string& interfaceName,
        const std::string& filter = "") const = 0;

    /**
     * @brief Get a single service reference for an interface
     *
     * If multiple services match, returns the one with highest ranking.
     *
     * @param interfaceName Interface name
     * @return Service reference, or invalid reference if not found
     */
    virtual ServiceReference getServiceReference(
        const std::string& interfaceName) const = 0;

    /**
     * @brief Get a service instance from a reference
     *
     * Increments the reference count. Must call ungetService() when done.
     *
     * @param ref Service reference
     * @return Shared pointer to service, or nullptr if service unavailable
     */
    virtual std::shared_ptr<void> getService(const ServiceReference& ref) = 0;

    /**
     * @brief Release a service instance
     *
     * Decrements the reference count.
     *
     * @param ref Service reference
     * @return true if service released, false if not in use
     */
    virtual bool ungetService(const ServiceReference& ref) = 0;

    // ==================================================================
    // Event Operations (PHASE_3 - Event Dispatcher)
    // ==================================================================

    /**
     * @brief Add an event listener
     *
     * Listeners are automatically removed when the module stops.
     *
     * @param listener Event listener to add
     * @param filter Event filter (optional, empty filter matches all events)
     * @param priority Listener priority (higher = executed first, default 0)
     * @param synchronous Execute synchronously (default false - async via thread pool)
     */
    virtual void addEventListener(
        IEventListener* listener,
        const EventFilter& filter = EventFilter(),
        int priority = 0,
        bool synchronous = false) = 0;

    /**
     * @brief Remove an event listener
     * @param listener Event listener to remove
     */
    virtual void removeEventListener(IEventListener* listener) = 0;

    /**
     * @brief Fire an event asynchronously
     *
     * Event is queued and delivered to matching listeners on worker threads.
     *
     * @param event Event to fire
     */
    virtual void fireEvent(const Event& event) = 0;

    /**
     * @brief Fire an event synchronously
     *
     * Event is delivered immediately to all matching listeners on calling thread.
     *
     * @param event Event to fire
     */
    virtual void fireEventSync(const Event& event) = 0;

    // ==================================================================
    // Module Operations
    // ==================================================================

    /**
     * @brief Install a module from a location
     *
     * @param location Path to module shared library
     * @return Pointer to installed module (in INSTALLED state)
     * @throws ModuleException if installation fails
     */
    virtual Module* installModule(const std::string& location) = 0;

    /**
     * @brief Get all installed modules
     * @return Vector of all modules in the framework
     */
    virtual std::vector<Module*> getModules() const = 0;

    /**
     * @brief Get a module by ID
     * @param moduleId Module ID
     * @return Pointer to module, or nullptr if not found
     */
    virtual Module* getModule(uint64_t moduleId) const = 0;

    /**
     * @brief Get a module by symbolic name
     *
     * If multiple versions exist, returns the highest version.
     *
     * @param symbolicName Module symbolic name
     * @return Pointer to module, or nullptr if not found
     */
    virtual Module* getModule(const std::string& symbolicName) const = 0;
};

} // namespace cdmf

#endif // CDMF_MODULE_CONTEXT_H
