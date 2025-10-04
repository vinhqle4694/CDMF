#include <gtest/gtest.h>
#include "core/event_dispatcher.h"
#include "core/event_filter.h"
#include <atomic>
#include <thread>
#include <chrono>

using namespace cdmf;
using namespace std::chrono_literals;

// Test listener
class TestListener : public IEventListener {
public:
    std::atomic<int> eventCount{0};
    Event lastEvent{"none"};
    std::mutex eventMutex;

    void handleEvent(const Event& event) override {
        std::lock_guard<std::mutex> lock(eventMutex);
        ++eventCount;
        lastEvent = event;
    }
};

TEST(EventDispatcherTest, Construction) {
    EventDispatcher dispatcher(4);
    EXPECT_FALSE(dispatcher.isRunning());
    EXPECT_EQ(0u, dispatcher.getListenerCount());
}

TEST(EventDispatcherTest, StartStop) {
    EventDispatcher dispatcher(2);

    EXPECT_FALSE(dispatcher.isRunning());

    dispatcher.start();
    EXPECT_TRUE(dispatcher.isRunning());

    dispatcher.stop();
    EXPECT_FALSE(dispatcher.isRunning());
}

TEST(EventDispatcherTest, AddRemoveListener) {
    EventDispatcher dispatcher(2);
    TestListener listener;

    EXPECT_EQ(0u, dispatcher.getListenerCount());

    dispatcher.addEventListener(&listener);
    EXPECT_EQ(1u, dispatcher.getListenerCount());

    dispatcher.removeEventListener(&listener);
    EXPECT_EQ(0u, dispatcher.getListenerCount());
}

TEST(EventDispatcherTest, FireEventSync) {
    EventDispatcher dispatcher(2);
    TestListener listener;

    dispatcher.addEventListener(&listener);

    Event event("test.event");
    dispatcher.fireEventSync(event);

    EXPECT_EQ(1, listener.eventCount);
    EXPECT_EQ("test.event", listener.lastEvent.getType());
}

TEST(EventDispatcherTest, FireEventAsync) {
    EventDispatcher dispatcher(2);
    dispatcher.start();

    TestListener listener;
    dispatcher.addEventListener(&listener);

    Event event("test.event");
    dispatcher.fireEvent(event);

    // Wait for event to be delivered
    std::this_thread::sleep_for(100ms);

    EXPECT_GE(listener.eventCount, 1);

    dispatcher.stop();
}

TEST(EventDispatcherTest, EventFilter) {
    EventDispatcher dispatcher(2);
    TestListener listener1, listener2;

    EventFilter filter1("(type=module.*)");
    EventFilter filter2("(type=service.*)");

    dispatcher.addEventListener(&listener1, filter1);
    dispatcher.addEventListener(&listener2, filter2);

    Event moduleEvent("module.installed");
    Event serviceEvent("service.registered");

    dispatcher.fireEventSync(moduleEvent);
    EXPECT_EQ(1, listener1.eventCount);
    EXPECT_EQ(0, listener2.eventCount);

    dispatcher.fireEventSync(serviceEvent);
    EXPECT_EQ(1, listener1.eventCount);
    EXPECT_EQ(1, listener2.eventCount);
}

TEST(EventDispatcherTest, Priority) {
    EventDispatcher dispatcher(2);

    struct OrderedListener : public IEventListener {
        std::vector<int>& order;
        int id;

        OrderedListener(std::vector<int>& o, int i) : order(o), id(i) {}

        void handleEvent(const Event&) override {
            order.push_back(id);
        }
    };

    std::vector<int> order;
    OrderedListener listener1(order, 1);
    OrderedListener listener2(order, 2);
    OrderedListener listener3(order, 3);

    // Add in reverse priority order
    dispatcher.addEventListener(&listener3, EventFilter(), 1);
    dispatcher.addEventListener(&listener1, EventFilter(), 3);
    dispatcher.addEventListener(&listener2, EventFilter(), 2);

    Event event("test");
    dispatcher.fireEventSync(event);

    // Should be called in priority order: 1, 2, 3
    ASSERT_EQ(3u, order.size());
    EXPECT_EQ(1, order[0]);
    EXPECT_EQ(2, order[1]);
    EXPECT_EQ(3, order[2]);
}

TEST(EventDispatcherTest, MultipleListeners) {
    EventDispatcher dispatcher(2);

    TestListener listener1, listener2, listener3;

    dispatcher.addEventListener(&listener1);
    dispatcher.addEventListener(&listener2);
    dispatcher.addEventListener(&listener3);

    Event event("test.event");
    dispatcher.fireEventSync(event);

    EXPECT_EQ(1, listener1.eventCount);
    EXPECT_EQ(1, listener2.eventCount);
    EXPECT_EQ(1, listener3.eventCount);
}

TEST(EventDispatcherTest, SynchronousExecution) {
    EventDispatcher dispatcher(2);
    dispatcher.start();

    TestListener listener;
    dispatcher.addEventListener(&listener, EventFilter(), 0, true); // Synchronous

    Event event("test.event");
    dispatcher.fireEvent(event);

    // Wait briefly
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(1, listener.eventCount);

    dispatcher.stop();
}

TEST(EventDispatcherTest, WaitForEvents) {
    EventDispatcher dispatcher(2);
    dispatcher.start();

    TestListener listener;
    dispatcher.addEventListener(&listener);

    // Fire multiple events
    for (int i = 0; i < 10; ++i) {
        Event event("test.event");
        event.setProperty("id", i);
        dispatcher.fireEvent(event);
    }

    // Wait for all events to be delivered
    bool success = dispatcher.waitForEvents(5000);
    EXPECT_TRUE(success);
    EXPECT_EQ(10, listener.eventCount);

    dispatcher.stop();
}

TEST(EventDispatcherTest, UpdateListener) {
    EventDispatcher dispatcher(2);
    TestListener listener;

    EventFilter filter1("(type=test1)");
    dispatcher.addEventListener(&listener, filter1);

    Event event1("test1");
    Event event2("test2");

    dispatcher.fireEventSync(event1);
    EXPECT_EQ(1, listener.eventCount);

    dispatcher.fireEventSync(event2);
    EXPECT_EQ(1, listener.eventCount); // Not matched

    // Update filter
    EventFilter filter2("(type=test2)");
    dispatcher.addEventListener(&listener, filter2);

    dispatcher.fireEventSync(event2);
    EXPECT_EQ(2, listener.eventCount); // Now matched
}

TEST(EventDispatcherTest, ListenerException) {
    EventDispatcher dispatcher(2);

    struct ThrowingListener : public IEventListener {
        void handleEvent(const Event&) override {
            throw std::runtime_error("Test exception");
        }
    };

    struct NormalListener : public IEventListener {
        std::atomic<int> count{0};
        void handleEvent(const Event&) override {
            ++count;
        }
    };

    ThrowingListener throwing;
    NormalListener normal;

    dispatcher.addEventListener(&throwing);
    dispatcher.addEventListener(&normal);

    Event event("test");

    // Exception should be caught and not affect other listeners
    EXPECT_NO_THROW(dispatcher.fireEventSync(event));
    EXPECT_EQ(1, normal.count);
}

TEST(EventDispatcherTest, PendingEventCount) {
    EventDispatcher dispatcher(2);
    dispatcher.start();

    TestListener listener;
    dispatcher.addEventListener(&listener);

    EXPECT_EQ(0u, dispatcher.getPendingEventCount());

    // Fire events
    for (int i = 0; i < 5; ++i) {
        Event event("test");
        dispatcher.fireEvent(event);
    }

    // Wait for processing
    dispatcher.waitForEvents(1000);

    EXPECT_EQ(0u, dispatcher.getPendingEventCount());

    dispatcher.stop();
}

TEST(EventDispatcherTest, NullListener) {
    EventDispatcher dispatcher(2);

    // Should not crash
    EXPECT_NO_THROW(dispatcher.addEventListener(nullptr));
    EXPECT_NO_THROW(dispatcher.removeEventListener(nullptr));

    EXPECT_EQ(0u, dispatcher.getListenerCount());
}
