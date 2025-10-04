#ifndef CDMF_SERVICE_LISTENER_H
#define CDMF_SERVICE_LISTENER_H

#include "service/service_event.h"

namespace cdmf {

/**
 * @brief Service listener interface
 *
 * Listens for service lifecycle events (registered, modified, unregistering).
 * Implementations should be lightweight as they may be called from multiple threads.
 *
 * Thread safety:
 * - Listener methods may be called from any thread
 * - Listener must be thread-safe if used with async event delivery
 * - Framework guarantees no concurrent calls to same listener instance
 *
 * Usage:
 * ```cpp
 * class MyServiceListener : public IServiceListener {
 * public:
 *     void serviceChanged(const ServiceEvent& event) override {
 *         switch (event.getType()) {
 *             case ServiceEventType::REGISTERED:
 *                 // Handle new service
 *                 break;
 *             case ServiceEventType::MODIFIED:
 *                 // Handle property change
 *                 break;
 *             case ServiceEventType::UNREGISTERING:
 *                 // Handle service removal
 *                 break;
 *         }
 *     }
 * };
 * ```
 */
class IServiceListener {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IServiceListener() = default;

    /**
     * @brief Called when a service event occurs
     *
     * @param event Service event containing affected service reference
     */
    virtual void serviceChanged(const ServiceEvent& event) = 0;
};

} // namespace cdmf

#endif // CDMF_SERVICE_LISTENER_H
