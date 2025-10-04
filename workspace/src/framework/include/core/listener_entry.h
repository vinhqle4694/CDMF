#ifndef CDMF_LISTENER_ENTRY_H
#define CDMF_LISTENER_ENTRY_H

#include "core/event_listener.h"
#include "core/event_filter.h"
#include <thread>
#include <memory>

namespace cdmf {

/**
 * @brief Entry for a registered event listener
 *
 * Contains the listener pointer, associated filter, priority, and other metadata.
 */
struct ListenerEntry {
    IEventListener* listener;          ///< Pointer to the listener (not owned)
    EventFilter filter;                ///< Event filter
    int priority;                      ///< Priority (higher = earlier execution)
    std::thread::id threadId;          ///< Thread ID where listener was registered
    bool synchronous;                  ///< If true, execute synchronously
    void* context;                     ///< Optional context data

    /**
     * @brief Default constructor
     */
    ListenerEntry()
        : listener(nullptr)
        , filter()
        , priority(0)
        , threadId(std::this_thread::get_id())
        , synchronous(false)
        , context(nullptr) {
    }

    /**
     * @brief Construct a listener entry
     * @param l Listener pointer
     * @param f Event filter
     * @param p Priority (default 0)
     * @param sync Synchronous execution (default false)
     */
    ListenerEntry(IEventListener* l, const EventFilter& f, int p = 0, bool sync = false)
        : listener(l)
        , filter(f)
        , priority(p)
        , threadId(std::this_thread::get_id())
        , synchronous(sync)
        , context(nullptr) {
    }

    /**
     * @brief Comparison operator for priority sorting
     *
     * Higher priority entries come first.
     */
    bool operator<(const ListenerEntry& other) const {
        return priority > other.priority; // Higher priority first
    }

    /**
     * @brief Equality comparison
     */
    bool operator==(const ListenerEntry& other) const {
        return listener == other.listener;
    }
};

} // namespace cdmf

#endif // CDMF_LISTENER_ENTRY_H
