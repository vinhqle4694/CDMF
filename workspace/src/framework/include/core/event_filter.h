#ifndef CDMF_EVENT_FILTER_H
#define CDMF_EVENT_FILTER_H

#include "core/event.h"
#include <string>
#include <memory>
#include <vector>

namespace cdmf {

/**
 * @brief Event filter using LDAP-style filter syntax
 *
 * Supports filtering events based on properties using LDAP-like filter expressions.
 *
 * Supported operators:
 * - Equality: (property=value)
 * - Inequality: (property!=value)
 * - Less than: (property<value)
 * - Greater than: (property>value)
 * - Less or equal: (property<=value)
 * - Greater or equal: (property>=value)
 * - Presence: (property=*)
 * - AND: (&(filter1)(filter2)...)
 * - OR: (|(filter1)(filter2)...)
 * - NOT: (!(filter))
 *
 * Examples:
 * - "(type=module.installed)" - Type equals "module.installed"
 * - "(&(priority>=5)(category=security))" - Priority >= 5 AND category = security
 * - "(|(status=active)(status=pending))" - Status is active OR pending
 * - "(!(status=disabled))" - Status is NOT disabled
 * - "(module=*)" - Module property exists
 */
class EventFilter {
public:
    /**
     * @brief Default constructor (matches all events)
     */
    EventFilter();

    /**
     * @brief Construct filter from string
     * @param filterString LDAP-style filter string
     * @throws std::invalid_argument if filter string is invalid
     */
    explicit EventFilter(const std::string& filterString);

    /**
     * @brief Copy constructor
     */
    EventFilter(const EventFilter& other);

    /**
     * @brief Move constructor
     */
    EventFilter(EventFilter&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    EventFilter& operator=(const EventFilter& other);

    /**
     * @brief Move assignment
     */
    EventFilter& operator=(EventFilter&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~EventFilter();

    /**
     * @brief Check if an event matches this filter
     * @param event Event to test
     * @return true if event matches, false otherwise
     */
    bool matches(const Event& event) const;

    /**
     * @brief Get the filter string
     */
    const std::string& toString() const { return filterString_; }

    /**
     * @brief Check if filter is empty (matches all)
     */
    bool isEmpty() const { return filterString_.empty(); }

private:
    // Forward declaration of filter node
    struct FilterNode;

    std::string filterString_;
    std::unique_ptr<FilterNode> root_;

    /**
     * @brief Parse filter string into node tree
     */
    std::unique_ptr<FilterNode> parseFilter(const std::string& filter);

    /**
     * @brief Evaluate a filter node against an event
     */
    bool evaluate(const FilterNode* node, const Event& event) const;

    /**
     * @brief Parse comparison operator and value
     */
    std::unique_ptr<FilterNode> parseComparison(const std::string& expr);

    /**
     * @brief Skip whitespace in string
     */
    static size_t skipWhitespace(const std::string& str, size_t pos);

    /**
     * @brief Find matching closing parenthesis
     */
    static size_t findMatchingParen(const std::string& str, size_t start);
};

} // namespace cdmf

#endif // CDMF_EVENT_FILTER_H
