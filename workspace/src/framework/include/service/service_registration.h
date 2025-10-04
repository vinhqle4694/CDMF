#ifndef CDMF_SERVICE_REGISTRATION_H
#define CDMF_SERVICE_REGISTRATION_H

#include "service/service_reference.h"
#include "utils/properties.h"
#include <cstdint>
#include <memory>

namespace cdmf {

// Forward declaration
class ServiceRegistry;

/**
 * @brief Service registration handle
 *
 * Represents a registered service and allows the registering module
 * to modify or unregister the service.
 *
 * Thread safety:
 * - All operations are thread-safe
 * - Safe to use across threads
 * - Internally delegates to thread-safe ServiceRegistry
 *
 * Lifecycle:
 * - Created: When service is registered via IModuleContext::registerService()
 * - Valid: While service remains registered
 * - Invalid: After unregister() or when owning module stops
 *
 * Usage:
 * ```cpp
 * // Register service
 * ServiceRegistration reg = context->registerService(
 *     "com.example.ILogger",
 *     myLogger,
 *     Properties().set("level", "DEBUG")
 * );
 *
 * // Modify properties
 * reg.setProperties(Properties().set("level", "INFO"));
 *
 * // Unregister
 * reg.unregister();
 * ```
 */
class ServiceRegistration {
public:
    /**
     * @brief Default constructor - creates invalid registration
     */
    ServiceRegistration();

    /**
     * @brief Construct from service entry and registry
     * @param entry Service entry
     * @param registry Service registry (for operations)
     */
    ServiceRegistration(std::shared_ptr<ServiceEntry> entry,
                        ServiceRegistry* registry);

    /**
     * @brief Copy constructor
     */
    ServiceRegistration(const ServiceRegistration& other) = default;

    /**
     * @brief Copy assignment
     */
    ServiceRegistration& operator=(const ServiceRegistration& other) = default;

    /**
     * @brief Check if registration is valid
     * @return true if service is registered
     */
    bool isValid() const;

    /**
     * @brief Get service ID
     * @return Unique service ID (0 if invalid)
     */
    uint64_t getServiceId() const;

    /**
     * @brief Get service reference
     *
     * Allows the registering module to get a reference to its own service.
     *
     * @return Service reference (may be invalid if unregistered)
     */
    ServiceReference getReference() const;

    /**
     * @brief Set service properties
     *
     * Updates service properties and fires SERVICE_MODIFIED event.
     * Properties are merged with existing properties (new values override).
     *
     * @param props New properties
     * @throws std::runtime_error if service is not registered
     */
    void setProperties(const Properties& props);

    /**
     * @brief Unregister the service
     *
     * Removes the service from the framework and fires SERVICE_UNREGISTERING event.
     * After unregistering:
     * - Service is no longer discoverable
     * - All ServiceReferences become invalid
     * - Modules still holding service pointers must release them
     *
     * @note Safe to call multiple times (subsequent calls are no-op)
     * @note Automatically called when owning module stops
     */
    void unregister();

    /**
     * @brief Equality comparison
     */
    bool operator==(const ServiceRegistration& other) const;

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const ServiceRegistration& other) const;

private:
    std::shared_ptr<ServiceEntry> entry_;
    ServiceRegistry* registry_;
};

} // namespace cdmf

#endif // CDMF_SERVICE_REGISTRATION_H
