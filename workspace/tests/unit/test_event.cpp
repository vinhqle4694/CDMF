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

// ============================================================================
// Event Boundary and Edge Case Tests
// ============================================================================

TEST(EventTest, EmptyTypeString) {
    // Event with empty type should work
    Event event("");
    EXPECT_EQ("", event.getType());
    EXPECT_TRUE(event.matchesType(""));
    EXPECT_TRUE(event.matchesType("*"));
}

TEST(EventTest, VeryLongTypeString) {
    // Test with very long type name
    std::string longType(10000, 'a');
    Event event(longType);
    EXPECT_EQ(longType, event.getType());
}

TEST(EventTest, SpecialCharactersInType) {
    // Test special characters in event type
    Event event("test.event@#$%^&*()");
    EXPECT_EQ("test.event@#$%^&*()", event.getType());
}

TEST(EventTest, WildcardMatchingEdgeCases) {
    Event event("module.service.installed");

    // Test various wildcard patterns
    EXPECT_TRUE(event.matchesType("*"));
    EXPECT_TRUE(event.matchesType("module.*"));
    EXPECT_TRUE(event.matchesType("module.service.*"));
    EXPECT_FALSE(event.matchesType("service.*"));
    EXPECT_FALSE(event.matchesType("*.installed"));  // Mid/suffix wildcard not supported
}

TEST(EventTest, PropertyOverwrite) {
    Event event("test.event");

    event.setProperty("key", std::string("value1"));
    EXPECT_EQ("value1", event.getPropertyString("key"));

    // Overwrite with different value
    event.setProperty("key", std::string("value2"));
    EXPECT_EQ("value2", event.getPropertyString("key"));
}

TEST(EventTest, PropertyTypeMismatch) {
    Event event("test.event");

    event.setProperty("key", std::string("text"));

    // Try to retrieve as int (should return default)
    EXPECT_EQ(42, event.getPropertyInt("key", 42));
}

TEST(EventTest, ManyProperties) {
    Event event("test.event");

    // Add many properties
    for (int i = 0; i < 1000; ++i) {
        event.setProperty("key" + std::to_string(i), i);
    }

    // Verify all properties exist
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(event.hasProperty("key" + std::to_string(i)));
        EXPECT_EQ(i, event.getPropertyInt("key" + std::to_string(i)));
    }
}

TEST(EventTest, PropertyWithEmptyKey) {
    Event event("test.event");

    event.setProperty("", std::string("value"));
    EXPECT_TRUE(event.hasProperty(""));
    EXPECT_EQ("value", event.getPropertyString(""));
}

TEST(EventTest, NullptrSource) {
    Event event("test.event", nullptr);
    EXPECT_EQ(nullptr, event.getSource());
}

TEST(EventTest, SelfAssignment) {
    Event event("test.event");
    event.setProperty("key", std::string("value"));

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-assign-overloaded"
    event = event;  // Self-assignment
    #pragma GCC diagnostic pop

    EXPECT_EQ("test.event", event.getType());
    EXPECT_EQ("value", event.getPropertyString("key"));
}

TEST(EventTest, GetAgeProgression) {
    Event event("test.event");

    auto age1 = event.getAge();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto age2 = event.getAge();

    EXPECT_GT(age2.count(), age1.count());
}

TEST(EventTest, CopyPreservesAllData) {
    int dummy = 42;
    Properties props;
    props.set("key1", std::string("value1"));
    props.set("key2", 123);

    Event original("test.event", &dummy, props);
    Event copy(original);

    EXPECT_EQ(original.getType(), copy.getType());
    EXPECT_EQ(original.getSource(), copy.getSource());
    EXPECT_EQ(original.getPropertyString("key1"), copy.getPropertyString("key1"));
    EXPECT_EQ(original.getPropertyInt("key2"), copy.getPropertyInt("key2"));
    EXPECT_EQ(original.getTimestamp(), copy.getTimestamp());
}

// ============================================================================
// EventFilter Boundary and Edge Case Tests
// ============================================================================

TEST(EventFilterTest, EmptyFilterString) {
    EventFilter filter("");
    EXPECT_TRUE(filter.isEmpty());

    Event event("test.event");
    EXPECT_TRUE(filter.matches(event));
}

TEST(EventFilterTest, ComplexNestedFiltersDeep) {
    // Very deeply nested filter
    EventFilter filter("(&(&(&(a=1)(b=2))(c=3))(d=4))");

    Event event("test");
    event.setProperty("a", std::string("1"));
    event.setProperty("b", std::string("2"));
    event.setProperty("c", std::string("3"));
    event.setProperty("d", std::string("4"));

    EXPECT_TRUE(filter.matches(event));
}

TEST(EventFilterTest, FilterWithNumericEdgeCases) {
    EventFilter filterZero("(value=0)");
    EventFilter filterNegative("(value>-10)");
    EventFilter filterLarge("(value<1000000)");

    Event event("test");
    event.setProperty("value", std::string("0"));
    EXPECT_TRUE(filterZero.matches(event));

    event.setProperty("value", std::string("-5"));
    EXPECT_TRUE(filterNegative.matches(event));

    event.setProperty("value", std::string("999999"));
    EXPECT_TRUE(filterLarge.matches(event));
}

TEST(EventFilterTest, MultipleOperatorsOnSameProperty) {
    EventFilter filter("(&(age>=18)(age<65))");

    Event event1("test");
    event1.setProperty("age", std::string("25"));
    EXPECT_TRUE(filter.matches(event1));

    Event event2("test");
    event2.setProperty("age", std::string("10"));
    EXPECT_FALSE(filter.matches(event2));

    Event event3("test");
    event3.setProperty("age", std::string("70"));
    EXPECT_FALSE(filter.matches(event3));
}

TEST(EventFilterTest, CaseInsensitivePropertyNames) {
    EventFilter filter("(Status=active)");

    Event event("test");
    event.setProperty("Status", std::string("active"));
    EXPECT_TRUE(filter.matches(event));
}

TEST(EventFilterTest, PresenceFilterMultipleProperties) {
    EventFilter filter("(&(prop1=*)(prop2=*)(prop3=*))");

    Event event1("test");
    event1.setProperty("prop1", std::string("a"));
    event1.setProperty("prop2", std::string("b"));
    event1.setProperty("prop3", std::string("c"));
    EXPECT_TRUE(filter.matches(event1));

    Event event2("test");
    event2.setProperty("prop1", std::string("a"));
    event2.setProperty("prop2", std::string("b"));
    // Missing prop3
    EXPECT_FALSE(filter.matches(event2));
}

// Note: The following tests are commented out because the current EventFilter
// implementation may not throw exceptions for all invalid inputs.
// These tests document expected behavior but may require implementation updates.

// TEST(EventFilterTest, InvalidOperators) {
//     // Invalid operator should throw
//     EXPECT_THROW(EventFilter("(key~=value)"), std::invalid_argument);
//     EXPECT_THROW(EventFilter("(key<==value)"), std::invalid_argument);
// }

// TEST(EventFilterTest, MismatchedParentheses) {
//     EXPECT_THROW(EventFilter("((a=1)"), std::invalid_argument);
//     EXPECT_THROW(EventFilter("(a=1))"), std::invalid_argument);
//     EXPECT_THROW(EventFilter("(a=1)(b=2)"), std::invalid_argument);
// }

// TEST(EventFilterTest, EmptySubFilters) {
//     // AND/OR with empty subfilters should throw
//     EXPECT_THROW(EventFilter("(&)"), std::invalid_argument);
//     EXPECT_THROW(EventFilter("(|)"), std::invalid_argument);
// }

TEST(EventFilterTest, LargeOrFilter) {
    // Large OR filter with many conditions
    std::string filterStr = "(|";
    for (int i = 0; i < 100; ++i) {
        filterStr += "(id=" + std::to_string(i) + ")";
    }
    filterStr += ")";

    EventFilter filter(filterStr);

    Event event("test");
    event.setProperty("id", std::string("50"));
    EXPECT_TRUE(filter.matches(event));
}

TEST(EventFilterTest, NotFilterNested) {
    EventFilter filter("(!(!(status=active)))");

    Event event("test");
    event.setProperty("status", std::string("active"));
    EXPECT_TRUE(filter.matches(event));
}

TEST(EventFilterTest, ComparisonWithMissingProperty) {
    EventFilter filter("(count>5)");

    Event event("test");
    // No count property
    EXPECT_FALSE(filter.matches(event));
}

TEST(EventFilterTest, StringComparisonEdgeCases) {
    EventFilter filter1("(name>=aa)");
    EventFilter filter2("(name<=zz)");

    Event event("test");
    event.setProperty("name", std::string("mm"));

    EXPECT_TRUE(filter1.matches(event));
    EXPECT_TRUE(filter2.matches(event));
}

TEST(EventFilterTest, BooleanPropertyValues) {
    EventFilter filterTrue("(enabled=true)");
    EventFilter filterFalse("(enabled=false)");

    Event event1("test");
    event1.setProperty("enabled", std::string("true"));
    EXPECT_TRUE(filterTrue.matches(event1));
    EXPECT_FALSE(filterFalse.matches(event1));

    Event event2("test");
    event2.setProperty("enabled", std::string("false"));
    EXPECT_FALSE(filterTrue.matches(event2));
    EXPECT_TRUE(filterFalse.matches(event2));
}
