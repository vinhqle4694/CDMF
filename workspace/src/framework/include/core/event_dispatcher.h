#ifndef CDMF_EVENT_DISPATCHER_H
#define CDMF_EVENT_DISPATCHER_H

#include "core/event.h"
#include "core/event_listener.h"
#include "core/listener_entry.h"
#include "utils/blocking_queue.h"
#include "utils/thread_pool.h"
#include <vector>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <thread>

namespace cdmf {

/**
 * @brief Event dispatcher with priority-based event delivery
 *
 * Features:
 * - Asynchronous event delivery using thread pool
 * - Synchronous event delivery option
 * - Event filtering
 * - Priority-based listener ordering
 * - Thread-safe listener registration/removal
 */
class EventDispatcher {
public:
    /**
     * @brief Construct event dispatcher
     * @param threadPoolSize Number of worker threads for async event delivery
     */
    explicit EventDispatcher(size_t threadPoolSize = 4);

    /**
     * @brief Destructor
     */
    ~EventDispatcher();

    // Prevent copying
    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    /**
     * @brief Start the event dispatcher
     */
    void start();

    /**
     * @brief Stop the event dispatcher
     *
     * Waits for all pending events to be delivered.
     */
    void stop();

    /**
     * @brief Check if dispatcher is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Add an event listener
     *
     * @param listener Listener to add (must remain valid until removed)
     * @param filter Event filter
     * @param priority Listener priority (higher = executed first, default 0)
     * @param synchronous If true, execute listener synchronously (default false)
     */
    void addEventListener(IEventListener* listener,
                         const EventFilter& filter = EventFilter(),
                         int priority = 0,
                         bool synchronous = false);

    /**
     * @brief Remove an event listener
     *
     * @param listener Listener to remove
     */
    void removeEventListener(IEventListener* listener);

    /**
     * @brief Fire an event asynchronously
     *
     * The event is queued and delivered to matching listeners
     * on worker threads.
     *
     * @param event Event to fire
     */
    void fireEvent(const Event& event);

    /**
     * @brief Fire an event synchronously
     *
     * The event is delivered immediately to all matching listeners
     * on the calling thread.
     *
     * @param event Event to fire
     */
    void fireEventSync(const Event& event);

    /**
     * @brief Get number of registered listeners
     */
    size_t getListenerCount() const;

    /**
     * @brief Get number of pending events in queue
     */
    size_t getPendingEventCount() const;

    /**
     * @brief Wait for all pending events to be delivered
     *
     * @param timeout_ms Maximum time to wait in milliseconds (0 = wait forever)
     * @return true if all events delivered, false if timed out
     */
    bool waitForEvents(int timeout_ms = 0);

private:
    std::vector<ListenerEntry> listeners_;
    mutable std::shared_mutex listenersMutex_;

    std::unique_ptr<utils::ThreadPool> threadPool_;
    utils::BlockingQueue<Event> eventQueue_;

    std::atomic<bool> running_;
    std::thread dispatchThread_;

    /**
     * @brief Main dispatch loop
     */
    void dispatchLoop();

    /**
     * @brief Dispatch event to a single listener
     */
    void dispatchToListener(const ListenerEntry& entry, const Event& event);

    /**
     * @brief Get listeners matching an event
     */
    std::vector<ListenerEntry> getMatchingListeners(const Event& event) const;

    /**
     * @brief Sort listeners by priority
     */
    void sortListeners();
};

} // namespace cdmf

#endif // CDMF_EVENT_DISPATCHER_H
