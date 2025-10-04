#include <gtest/gtest.h>
#include "core/event.h"
#include "core/event_filter.h"
#include <thread>
#include <chrono>

using namespace cdmf;
using namespace std::chrono_literals;

// ============================================================================
// Event Tests
// ============================================================================

TEST(EventTest, BasicConstruction) {
    Event event("test.event");

    EXPECT_EQ("test.event", event.getType());
    EXPECT_EQ(nullptr, event.getSource());
}

TEST(EventTest, ConstructionWithSource) {
    int dummy = 42;
    Event event("test.event", &dummy);

    EXPECT_EQ("test.event", event.getType());
    EXPECT_EQ(&dummy, event.getSource());
}

TEST(EventTest, ConstructionWithProperties) {
    Properties props;
    props.set("key1", std::string("value1"));
    props.set("key2", 42);

    Event event("test.event", nullptr, props);

    EXPECT_EQ("test.event", event.getType());
    EXPECT_EQ("value1", event.getPropertyString("key1"));
    EXPECT_EQ(42, event.getPropertyInt("key2"));
}

TEST(EventTest, Timestamp) {
    auto before = std::chrono::system_clock::now();
    Event event("test.event");
    auto after = std::chrono::system_clock::now();

    EXPECT_GE(event.getTimestamp(), before);
    EXPECT_LE(event.getTimestamp(), after);
}

TEST(EventTest, SetAndGetProperty) {
    Event event("test.event");

    event.setProperty("string_prop", std::string("test"));
    event.setProperty("int_prop", 123);
    event.setProperty("bool_prop", true);

    EXPECT_EQ("test", event.getPropertyString("string_prop"));
    EXPECT_EQ(123, event.getPropertyInt("int_prop"));
    EXPECT_TRUE(event.getPropertyBool("bool_prop"));
}

TEST(EventTest, HasProperty) {
    Event event("test.event");

    EXPECT_FALSE(event.hasProperty("key"));

    event.setProperty("key", std::string("value"));

    EXPECT_TRUE(event.hasProperty("key"));
}

TEST(EventTest, GetPropertyWithDefault) {
    Event event("test.event");

    EXPECT_EQ("default", event.getPropertyString("nonexistent", "default"));
    EXPECT_EQ(99, event.getPropertyInt("nonexistent", 99));
    EXPECT_TRUE(event.getPropertyBool("nonexistent", true));
}

TEST(EventTest, GetAge) {
    Event event("test.event");

    std::this_thread::sleep_for(10ms);

    auto age = event.getAge();
    EXPECT_GE(age.count(), 10);
}

TEST(EventTest, MatchesTypeExact) {
    Event event("module.installed");

    EXPECT_TRUE(event.matchesType("module.installed"));
    EXPECT_FALSE(event.matchesType("module.started"));
}

TEST(EventTest, MatchesTypeWildcard) {
    Event event("module.installed");

    EXPECT_TRUE(event.matchesType("*"));
    EXPECT_TRUE(event.matchesType("module.*"));
    EXPECT_FALSE(event.matchesType("service.*"));
}

TEST(EventTest, ToString) {
    Event event("test.event");
    event.setProperty("key1", std::string("value1"));

    std::string str = event.toString();

    EXPECT_NE(std::string::npos, str.find("test.event"));
    EXPECT_NE(std::string::npos, str.find("key1"));
}

TEST(EventTest, CopyConstructor) {
    Event event1("test.event");
    event1.setProperty("key", std::string("value"));

    Event event2(event1);

    EXPECT_EQ(event1.getType(), event2.getType());
    EXPECT_EQ(event1.getPropertyString("key"), event2.getPropertyString("key"));
    EXPECT_EQ(event1.getTimestamp(), event2.getTimestamp());
}

TEST(EventTest, MoveConstructor) {
    Event event1("test.event");
    event1.setProperty("key", std::string("value"));

    Event event2(std::move(event1));

    EXPECT_EQ("test.event", event2.getType());
    EXPECT_EQ("value", event2.getPropertyString("key"));
}

TEST(EventTest, CopyAssignment) {
    Event event1("test.event1");
    event1.setProperty("key1", std::string("value1"));

    Event event2("test.event2");
    event2.setProperty("key2", std::string("value2"));

    event2 = event1;

    EXPECT_EQ("test.event1", event2.getType());
    EXPECT_EQ("value1", event2.getPropertyString("key1"));
    EXPECT_FALSE(event2.hasProperty("key2"));
}

TEST(EventTest, MoveAssignment) {
    Event event1("test.event1");
    event1.setProperty("key", std::string("value"));

    Event event2("test.event2");
    event2 = std::move(event1);

    EXPECT_EQ("test.event1", event2.getType());
    EXPECT_EQ("value", event2.getPropertyString("key"));
}

// ============================================================================
// EventFilter Tests
// ============================================================================

TEST(EventFilterTest, EmptyFilter) {
    EventFilter filter;

    Event event("test.event");
    EXPECT_TRUE(filter.matches(event));
    EXPECT_TRUE(filter.isEmpty());
}

TEST(EventFilterTest, TypeEqualityFilter) {
    EventFilter filter("(type=module.installed)");

    Event event1("module.installed");
    Event event2("module.started");

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_FALSE(filter.matches(event2));
}

TEST(EventFilterTest, PropertyEqualityFilter) {
    EventFilter filter("(status=active)");

    Event event1("test.event");
    event1.setProperty("status", std::string("active"));

    Event event2("test.event");
    event2.setProperty("status", std::string("inactive"));

    Event event3("test.event");
    // No status property

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_FALSE(filter.matches(event2));
    EXPECT_FALSE(filter.matches(event3));
}

TEST(EventFilterTest, PropertyNotEqualFilter) {
    EventFilter filter("(status!=disabled)");

    Event event1("test.event");
    event1.setProperty("status", std::string("active"));

    Event event2("test.event");
    event2.setProperty("status", std::string("disabled"));

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_FALSE(filter.matches(event2));
}

TEST(EventFilterTest, NumericComparisonFilters) {
    EventFilter filterGT("(priority>5)");
    EventFilter filterLT("(priority<10)");
    EventFilter filterGE("(priority>=5)");
    EventFilter filterLE("(priority<=10)");

    Event event("test.event");
    event.setProperty("priority", std::string("7"));

    EXPECT_TRUE(filterGT.matches(event));
    EXPECT_TRUE(filterLT.matches(event));
    EXPECT_TRUE(filterGE.matches(event));
    EXPECT_TRUE(filterLE.matches(event));
}

TEST(EventFilterTest, PresenceFilter) {
    EventFilter filter("(module=*)");

    Event event1("test.event");
    event1.setProperty("module", std::string("test-module"));

    Event event2("test.event");
    // No module property

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_FALSE(filter.matches(event2));
}

TEST(EventFilterTest, AndFilter) {
    EventFilter filter("(&(priority>=5)(category=security))");

    Event event1("test.event");
    event1.setProperty("priority", std::string("7"));
    event1.setProperty("category", std::string("security"));

    Event event2("test.event");
    event2.setProperty("priority", std::string("3"));
    event2.setProperty("category", std::string("security"));

    Event event3("test.event");
    event3.setProperty("priority", std::string("7"));
    event3.setProperty("category", std::string("network"));

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_FALSE(filter.matches(event2));
    EXPECT_FALSE(filter.matches(event3));
}

TEST(EventFilterTest, OrFilter) {
    EventFilter filter("(|(status=active)(status=pending))");

    Event event1("test.event");
    event1.setProperty("status", std::string("active"));

    Event event2("test.event");
    event2.setProperty("status", std::string("pending"));

    Event event3("test.event");
    event3.setProperty("status", std::string("disabled"));

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_TRUE(filter.matches(event2));
    EXPECT_FALSE(filter.matches(event3));
}

TEST(EventFilterTest, NotFilter) {
    EventFilter filter("(!(status=disabled))");

    Event event1("test.event");
    event1.setProperty("status", std::string("active"));

    Event event2("test.event");
    event2.setProperty("status", std::string("disabled"));

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_FALSE(filter.matches(event2));
}

TEST(EventFilterTest, ComplexNestedFilter) {
    // (priority >= 5 AND (category = security OR category = critical))
    EventFilter filter("(&(priority>=5)(|(category=security)(category=critical)))");

    Event event1("test.event");
    event1.setProperty("priority", std::string("7"));
    event1.setProperty("category", std::string("security"));

    Event event2("test.event");
    event2.setProperty("priority", std::string("7"));
    event2.setProperty("category", std::string("critical"));

    Event event3("test.event");
    event3.setProperty("priority", std::string("3"));
    event3.setProperty("category", std::string("security"));

    Event event4("test.event");
    event4.setProperty("priority", std::string("7"));
    event4.setProperty("category", std::string("info"));

    EXPECT_TRUE(filter.matches(event1));
    EXPECT_TRUE(filter.matches(event2));
    EXPECT_FALSE(filter.matches(event3));
    EXPECT_FALSE(filter.matches(event4));
}

TEST(EventFilterTest, FilterWithWhitespace) {
    EventFilter filter("( status = active )");

    Event event("test.event");
    event.setProperty("status", std::string("active"));

    EXPECT_TRUE(filter.matches(event));
}

TEST(EventFilterTest, InvalidFilterSyntax) {
    EXPECT_THROW(EventFilter("invalid"), std::invalid_argument);
    EXPECT_THROW(EventFilter("(unclosed"), std::invalid_argument);
    EXPECT_THROW(EventFilter("unopened)"), std::invalid_argument);
    EXPECT_THROW(EventFilter("()"), std::invalid_argument);
    EXPECT_THROW(EventFilter("(&)"), std::invalid_argument);
}

TEST(EventFilterTest, ToString) {
    std::string filterStr = "(status=active)";
    EventFilter filter(filterStr);

    EXPECT_EQ(filterStr, filter.toString());
}

TEST(EventFilterTest, CopyConstructor) {
    EventFilter filter1("(status=active)");
    EventFilter filter2(filter1);

    Event event("test.event");
    event.setProperty("status", std::string("active"));

    EXPECT_TRUE(filter2.matches(event));
    EXPECT_EQ(filter1.toString(), filter2.toString());
}

TEST(EventFilterTest, MoveConstructor) {
    EventFilter filter1("(status=active)");
    EventFilter filter2(std::move(filter1));

    Event event("test.event");
    event.setProperty("status", std::string("active"));

    EXPECT_TRUE(filter2.matches(event));
}

TEST(EventFilterTest, CopyAssignment) {
    EventFilter filter1("(status=active)");
    EventFilter filter2("(status=inactive)");

    filter2 = filter1;

    Event event("test.event");
    event.setProperty("status", std::string("active"));

    EXPECT_TRUE(filter2.matches(event));
}

TEST(EventFilterTest, MoveAssignment) {
    EventFilter filter1("(status=active)");
    EventFilter filter2("(status=inactive)");

    filter2 = std::move(filter1);

    Event event("test.event");
    event.setProperty("status", std::string("active"));

    EXPECT_TRUE(filter2.matches(event));
}

TEST(EventFilterTest, StringComparison) {
    EventFilter filterLT("(name<charlie)");
    EventFilter filterGT("(name>alice)");

    Event event("test.event");
    event.setProperty("name", std::string("bob"));

    EXPECT_TRUE(filterLT.matches(event));
    EXPECT_TRUE(filterGT.matches(event));
}
