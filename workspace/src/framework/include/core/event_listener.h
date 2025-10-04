#ifndef CDMF_EVENT_LISTENER_H
#define CDMF_EVENT_LISTENER_H

#include "core/event.h"

namespace cdmf {

/**
 * @brief Interface for event listeners
 *
 * Event listeners receive events from the EventDispatcher.
 * Listeners can be registered with optional filters to receive
 * only specific types of events.
 */
class IEventListener {
public:
    virtual ~IEventListener() = default;

    /**
     * @brief Handle an event
     *
     * This method is called by the EventDispatcher when an event
     * matching the listener's filter is fired.
     *
     * @param event The event to handle
     *
     * @note This method may be called from different threads depending
     *       on the EventDispatcher configuration. Implementations should
     *       be thread-safe.
     */
    virtual void handleEvent(const Event& event) = 0;
};

/**
 * @brief Interface for framework-specific event listeners
 *
 * Framework listeners receive framework lifecycle events and module events.
 */
class IFrameworkListener {
public:
    virtual ~IFrameworkListener() = default;

    /**
     * @brief Handle a framework event
     *
     * @param event The framework event
     */
    virtual void frameworkEvent(const Event& event) = 0;
};

/**
 * @brief Interface for module-specific event listeners
 *
 * Module listeners receive events related to module lifecycle.
 */
class IModuleListener {
public:
    virtual ~IModuleListener() = default;

    /**
     * @brief Handle a module event
     *
     * @param event The module event
     */
    virtual void moduleChanged(const Event& event) = 0;
};

} // namespace cdmf

#endif // CDMF_EVENT_LISTENER_H
