#ifndef CDMF_SERVICE_EVENT_H
#define CDMF_SERVICE_EVENT_H

#include "service/service_reference.h"
#include "core/event.h"
#include <string>

namespace cdmf {

/**
 * @brief Service event types
 */
enum class ServiceEventType {
    REGISTERED,    // Service registered
    MODIFIED,      // Service properties modified
    UNREGISTERING  // Service about to be unregistered
};

/**
 * @brief Convert service event type to string
 */
inline const char* serviceEventTypeToString(ServiceEventType type) {
    switch (type) {
        case ServiceEventType::REGISTERED:    return "SERVICE_REGISTERED";
        case ServiceEventType::MODIFIED:      return "SERVICE_MODIFIED";
        case ServiceEventType::UNREGISTERING: return "SERVICE_UNREGISTERING";
        default:                              return "UNKNOWN";
    }
}

/**
 * @brief Service event
 *
 * Fired when services are registered, modified, or unregistered.
 * Extends the base Event class with service-specific information.
 *
 * Event properties:
 * - "service.id": Service ID (int)
 * - "service.interface": Interface name (string)
 * - "service.module.id": Module ID (int)
 * - "service.module.name": Module symbolic name (string)
 */
class ServiceEvent : public Event {
public:
    /**
     * @brief Construct service event
     *
     * @param type Event type
     * @param ref Service reference
     */
    ServiceEvent(ServiceEventType type, const ServiceReference& ref);

    /**
     * @brief Get event type
     * @return Service event type
     */
    ServiceEventType getType() const { return type_; }

    /**
     * @brief Get service reference
     * @return Reference to affected service
     */
    const ServiceReference& getServiceReference() const { return serviceRef_; }

private:
    ServiceEventType type_;
    ServiceReference serviceRef_;
};

} // namespace cdmf

#endif // CDMF_SERVICE_EVENT_H
