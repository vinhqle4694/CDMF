#ifndef CDMF_SERVICE_REGISTRY_H
#define CDMF_SERVICE_REGISTRY_H

#include "service/service_reference.h"
#include "service/service_registration.h"
#include "service/service_entry.h"
#include "service/service_event.h"
#include "utils/properties.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <shared_mutex>
#include <atomic>

namespace cdmf {

// Forward declarations
class Module;
class EventDispatcher;

/**
 * @brief Service registry
 *
 * Central registry for all services in the framework.
 * Manages service lifecycle, lookup, and event notifications.
 *
 * Responsibilities:
 * - Register/unregister services
 * - Service lookup by interface and filter
 * - Service property modification
 * - Service reference counting
 * - Service event notifications
 *
 * Thread safety:
 * - All operations are thread-safe
 * - Uses shared_mutex for efficient concurrent reads
 * - Service lookup doesn't block registrations
 *
 * Service lifecycle:
 * 1. REGISTERED: Service registered via registerService()
 * 2. MODIFIED: Properties changed via setServiceProperties()
 * 3. UNREGISTERING: Service about to be removed via unregisterService()
 */
class ServiceRegistry {
public:
    /**
     * @brief Construct service registry
     * @param eventDispatcher Event dispatcher for service events (may be null)
     */
    explicit ServiceRegistry(EventDispatcher* eventDispatcher = nullptr);

    /**
     * @brief Destructor
     */
    ~ServiceRegistry();

    // Prevent copying
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;

    // ==================================================================
    // Service Registration
    // ==================================================================

    /**
     * @brief Register a service
     *
     * @param interfaceName Service interface name (e.g., "com.example.ILogger")
     * @param service Pointer to service implementation
     * @param props Service properties (metadata)
     * @param module Module registering the service
     * @return Service registration handle
     * @throws std::invalid_argument if interfaceName is empty or service is null
     */
    ServiceRegistration registerService(const std::string& interfaceName,
                                        void* service,
                                        const Properties& props,
                                        Module* module);

    /**
     * @brief Unregister a service
     *
     * Fires SERVICE_UNREGISTERING event before removal.
     *
     * @param serviceId Service ID
     * @return true if service was unregistered, false if not found
     */
    bool unregisterService(uint64_t serviceId);

    /**
     * @brief Unregister all services for a module
     *
     * Called automatically when a module stops.
     *
     * @param module Module whose services to unregister
     * @return Number of services unregistered
     */
    int unregisterServices(Module* module);

    /**
     * @brief Set service properties
     *
     * Updates properties and fires SERVICE_MODIFIED event.
     *
     * @param serviceId Service ID
     * @param props New properties (merged with existing)
     * @throws std::runtime_error if service not found
     */
    void setServiceProperties(uint64_t serviceId, const Properties& props);

    // ==================================================================
    // Service Lookup
    // ==================================================================

    /**
     * @brief Get all service references for an interface
     *
     * @param interfaceName Interface name to match
     * @param filter LDAP-style filter for properties (optional, empty = all)
     * @return Vector of matching service references (sorted by ranking)
     */
    std::vector<ServiceReference> getServiceReferences(
        const std::string& interfaceName,
        const std::string& filter = "") const;

    /**
     * @brief Get a single service reference for an interface
     *
     * Returns the service with highest ranking.
     *
     * @param interfaceName Interface name
     * @return Service reference, or invalid reference if not found
     */
    ServiceReference getServiceReference(const std::string& interfaceName) const;

    /**
     * @brief Get service reference by ID
     *
     * @param serviceId Service ID
     * @return Service reference, or invalid reference if not found
     */
    ServiceReference getServiceReferenceById(uint64_t serviceId) const;

    /**
     * @brief Get all registered services
     * @return Vector of all service references
     */
    std::vector<ServiceReference> getAllServices() const;

    /**
     * @brief Get services registered by a module
     *
     * @param module Module to query
     * @return Vector of service references registered by module
     */
    std::vector<ServiceReference> getServicesByModule(Module* module) const;

    // ==================================================================
    // Service Usage Tracking
    // ==================================================================

    /**
     * @brief Get a service instance
     *
     * Increments usage count for the service.
     *
     * @param ref Service reference
     * @return Shared pointer to service, or nullptr if service unavailable
     */
    std::shared_ptr<void> getService(const ServiceReference& ref);

    /**
     * @brief Release a service instance
     *
     * Decrements usage count for the service.
     *
     * @param ref Service reference
     * @return true if service released, false if not in use
     */
    bool ungetService(const ServiceReference& ref);

    /**
     * @brief Get services in use by a module
     *
     * @param module Module to query
     * @return Vector of service references being used by module
     */
    std::vector<ServiceReference> getServicesInUse(Module* module) const;

    // ==================================================================
    // Statistics
    // ==================================================================

    /**
     * @brief Get total number of registered services
     * @return Service count
     */
    size_t getServiceCount() const;

    /**
     * @brief Get number of services for an interface
     *
     * @param interfaceName Interface name
     * @return Count of matching services
     */
    size_t getServiceCount(const std::string& interfaceName) const;

private:
    /**
     * @brief Generate unique service ID
     * @return New service ID
     */
    uint64_t generateServiceId();

    /**
     * @brief Fire service event
     *
     * @param type Event type
     * @param ref Service reference
     */
    void fireServiceEvent(ServiceEventType type, const ServiceReference& ref);

    /**
     * @brief Check if properties match LDAP filter
     *
     * @param props Properties to check
     * @param filter LDAP filter string
     * @return true if properties match filter
     */
    bool matchesFilter(const Properties& props, const std::string& filter) const;

    // Service storage
    std::map<uint64_t, std::shared_ptr<ServiceEntry>> servicesById_;
    std::map<std::string, std::vector<std::shared_ptr<ServiceEntry>>> servicesByInterface_;

    // Service usage tracking (module -> service IDs)
    std::map<Module*, std::vector<uint64_t>> serviceUsage_;

    // Synchronization
    mutable std::shared_mutex mutex_;

    // Service ID generation
    std::atomic<uint64_t> nextServiceId_;

    // Event dispatcher
    EventDispatcher* eventDispatcher_;
};

} // namespace cdmf

#endif // CDMF_SERVICE_REGISTRY_H
