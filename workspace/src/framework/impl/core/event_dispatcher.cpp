#include "core/event_dispatcher.h"
#include <algorithm>
#include <chrono>

namespace cdmf {

EventDispatcher::EventDispatcher(size_t threadPoolSize)
    : listeners_()
    , listenersMutex_()
    , threadPool_(std::make_unique<utils::ThreadPool>(threadPoolSize))
    , eventQueue_(1000) // Max 1000 pending events
    , running_(false)
    , dispatchThread_() {
}

EventDispatcher::~EventDispatcher() {
    stop();
}

void EventDispatcher::start() {
    if (running_.exchange(true)) {
        return; // Already running
    }

    // Start dispatch loop
    dispatchThread_ = std::thread(&EventDispatcher::dispatchLoop, this);
}

void EventDispatcher::stop() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }

    // Push sentinel event to wake up dispatch thread
    Event sentinel("__STOP__");
    eventQueue_.push(sentinel);

    // Wait for dispatch thread to finish
    if (dispatchThread_.joinable()) {
        dispatchThread_.join();
    }

    // Shutdown and wait for thread pool to finish
    threadPool_->shutdown();
    threadPool_->wait();
}

void EventDispatcher::addEventListener(IEventListener* listener,
                                      const EventFilter& filter,
                                      int priority,
                                      bool synchronous) {
    if (!listener) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(listenersMutex_);

    // Check if listener already registered
    auto it = std::find_if(listeners_.begin(), listeners_.end(),
        [listener](const ListenerEntry& entry) {
            return entry.listener == listener;
        });

    if (it != listeners_.end()) {
        // Update existing entry
        it->filter = filter;
        it->priority = priority;
        it->synchronous = synchronous;
    } else {
        // Add new entry
        listeners_.emplace_back(listener, filter, priority, synchronous);
    }

    // Sort by priority
    sortListeners();
}

void EventDispatcher::removeEventListener(IEventListener* listener) {
    if (!listener) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(listenersMutex_);

    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
            [listener](const ListenerEntry& entry) {
                return entry.listener == listener;
            }),
        listeners_.end()
    );
}

void EventDispatcher::fireEvent(const Event& event) {
    if (!running_) {
        return;
    }

    eventQueue_.push(event);
}

void EventDispatcher::fireEventSync(const Event& event) {
    auto matchingListeners = getMatchingListeners(event);

    for (const auto& entry : matchingListeners) {
        dispatchToListener(entry, event);
    }
}

size_t EventDispatcher::getListenerCount() const {
    std::shared_lock<std::shared_mutex> lock(listenersMutex_);
    return listeners_.size();
}

size_t EventDispatcher::getPendingEventCount() const {
    return eventQueue_.size();
}

bool EventDispatcher::waitForEvents(int timeout_ms) {
    auto start = std::chrono::steady_clock::now();

    // Wait for both event queue and thread pool task queue to be empty
    while (getPendingEventCount() > 0 || threadPool_->getPendingTaskCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
            if (elapsed.count() >= timeout_ms) {
                return false;
            }
        }
    }

    return true;
}

void EventDispatcher::dispatchLoop() {
    while (true) {
        // Wait for next event (blocking with timeout)
        auto eventOpt = eventQueue_.pop(std::chrono::milliseconds(100));

        if (!eventOpt.has_value()) {
            // Timeout - check if we should stop
            if (!running_) {
                break;
            }
            continue;
        }

        Event event = std::move(eventOpt.value());

        // Check for sentinel stop event
        if (event.getType() == "__STOP__") {
            break;
        }

        // Get matching listeners
        auto matchingListeners = getMatchingListeners(event);

        // Dispatch to listeners
        for (const auto& entry : matchingListeners) {
            if (entry.synchronous) {
                // Execute synchronously
                dispatchToListener(entry, event);
            } else {
                // Execute asynchronously on thread pool
                threadPool_->enqueue([this, entry, event]() {
                    dispatchToListener(entry, event);
                });
            }
        }
    }
}

void EventDispatcher::dispatchToListener(const ListenerEntry& entry, const Event& event) {
    try {
        entry.listener->handleEvent(event);
    } catch (const std::exception& e) {
        // Log error but don't propagate to other listeners
        // In production, this would use the logging system
        (void)e; // Suppress unused warning
    } catch (...) {
        // Catch all to prevent listener errors from affecting other listeners
    }
}

std::vector<ListenerEntry> EventDispatcher::getMatchingListeners(const Event& event) const {
    std::shared_lock<std::shared_mutex> lock(listenersMutex_);

    std::vector<ListenerEntry> matching;
    matching.reserve(listeners_.size());

    for (const auto& entry : listeners_) {
        if (entry.filter.isEmpty() || entry.filter.matches(event)) {
            matching.push_back(entry);
        }
    }

    return matching;
}

void EventDispatcher::sortListeners() {
    // Sort by priority (higher priority first)
    std::sort(listeners_.begin(), listeners_.end());
}

} // namespace cdmf
