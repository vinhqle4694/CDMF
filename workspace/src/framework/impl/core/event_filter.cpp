#include "core/event_filter.h"
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace cdmf {

// ============================================================================
// FilterNode - Internal representation of filter tree
// ============================================================================

struct EventFilter::FilterNode {
    enum class Type {
        AND,
        OR,
        NOT,
        COMPARISON
    };

    enum class Operator {
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        GREATER_THAN,
        LESS_EQUAL,
        GREATER_EQUAL,
        PRESENT
    };

    Type type;
    Operator op;
    std::string key;
    std::string value;
    std::vector<std::unique_ptr<FilterNode>> children;

    FilterNode(Type t) : type(t), op(Operator::EQUAL) {}
};

// ============================================================================
// EventFilter Implementation
// ============================================================================

EventFilter::EventFilter()
    : filterString_("")
    , root_(nullptr) {
}

EventFilter::EventFilter(const std::string& filterString)
    : filterString_(filterString)
    , root_(nullptr) {

    if (!filterString.empty()) {
        root_ = parseFilter(filterString);
    }
}

EventFilter::EventFilter(const EventFilter& other)
    : filterString_(other.filterString_)
    , root_(nullptr) {

    if (!filterString_.empty()) {
        root_ = parseFilter(filterString_);
    }
}

EventFilter::EventFilter(EventFilter&& other) noexcept
    : filterString_(std::move(other.filterString_))
    , root_(std::move(other.root_)) {
}

EventFilter& EventFilter::operator=(const EventFilter& other) {
    if (this != &other) {
        filterString_ = other.filterString_;
        if (!filterString_.empty()) {
            root_ = parseFilter(filterString_);
        } else {
            root_ = nullptr;
        }
    }
    return *this;
}

EventFilter& EventFilter::operator=(EventFilter&& other) noexcept {
    if (this != &other) {
        filterString_ = std::move(other.filterString_);
        root_ = std::move(other.root_);
    }
    return *this;
}

EventFilter::~EventFilter() = default;

bool EventFilter::matches(const Event& event) const {
    if (!root_) {
        return true; // Empty filter matches all
    }
    return evaluate(root_.get(), event);
}

size_t EventFilter::skipWhitespace(const std::string& str, size_t pos) {
    while (pos < str.length() && std::isspace(str[pos])) {
        ++pos;
    }
    return pos;
}

size_t EventFilter::findMatchingParen(const std::string& str, size_t start) {
    int depth = 1;
    size_t pos = start + 1;

    while (pos < str.length() && depth > 0) {
        if (str[pos] == '(') {
            ++depth;
        } else if (str[pos] == ')') {
            --depth;
        }
        ++pos;
    }

    if (depth != 0) {
        throw std::invalid_argument("Unmatched parentheses in filter");
    }

    return pos - 1;
}

std::unique_ptr<EventFilter::FilterNode> EventFilter::parseFilter(const std::string& filter) {
    std::string trimmed = filter;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        return nullptr;
    }

    // Must start and end with parentheses
    if (trimmed[0] != '(' || trimmed[trimmed.length() - 1] != ')') {
        throw std::invalid_argument("Filter must be enclosed in parentheses");
    }

    // Remove outer parentheses
    std::string inner = trimmed.substr(1, trimmed.length() - 2);
    inner.erase(0, inner.find_first_not_of(" \t\n\r"));

    if (inner.empty()) {
        throw std::invalid_argument("Empty filter expression");
    }

    // Check for logical operators
    if (inner[0] == '&') {
        // AND operator
        auto node = std::make_unique<FilterNode>(FilterNode::Type::AND);
        size_t pos = 1;
        pos = skipWhitespace(inner, pos);

        while (pos < inner.length()) {
            if (inner[pos] != '(') {
                throw std::invalid_argument("Expected '(' in AND expression");
            }
            size_t end = findMatchingParen(inner, pos);
            std::string subExpr = inner.substr(pos, end - pos + 1);
            node->children.push_back(parseFilter(subExpr));
            pos = skipWhitespace(inner, end + 1);
        }

        if (node->children.empty()) {
            throw std::invalid_argument("AND expression must have at least one child");
        }

        return node;

    } else if (inner[0] == '|') {
        // OR operator
        auto node = std::make_unique<FilterNode>(FilterNode::Type::OR);
        size_t pos = 1;
        pos = skipWhitespace(inner, pos);

        while (pos < inner.length()) {
            if (inner[pos] != '(') {
                throw std::invalid_argument("Expected '(' in OR expression");
            }
            size_t end = findMatchingParen(inner, pos);
            std::string subExpr = inner.substr(pos, end - pos + 1);
            node->children.push_back(parseFilter(subExpr));
            pos = skipWhitespace(inner, end + 1);
        }

        if (node->children.empty()) {
            throw std::invalid_argument("OR expression must have at least one child");
        }

        return node;

    } else if (inner[0] == '!') {
        // NOT operator
        auto node = std::make_unique<FilterNode>(FilterNode::Type::NOT);
        size_t pos = 1;
        pos = skipWhitespace(inner, pos);

        if (pos >= inner.length() || inner[pos] != '(') {
            throw std::invalid_argument("Expected '(' after NOT operator");
        }

        size_t end = findMatchingParen(inner, pos);
        std::string subExpr = inner.substr(pos, end - pos + 1);
        node->children.push_back(parseFilter(subExpr));

        return node;

    } else {
        // Comparison expression
        return parseComparison(inner);
    }
}

std::unique_ptr<EventFilter::FilterNode> EventFilter::parseComparison(const std::string& expr) {
    auto node = std::make_unique<FilterNode>(FilterNode::Type::COMPARISON);

    // Find operator
    size_t opPos = std::string::npos;
    FilterNode::Operator op = FilterNode::Operator::EQUAL;

    // Check for two-character operators first
    if ((opPos = expr.find("!=")) != std::string::npos) {
        op = FilterNode::Operator::NOT_EQUAL;
    } else if ((opPos = expr.find("<=")) != std::string::npos) {
        op = FilterNode::Operator::LESS_EQUAL;
    } else if ((opPos = expr.find(">=")) != std::string::npos) {
        op = FilterNode::Operator::GREATER_EQUAL;
    } else if ((opPos = expr.find('<')) != std::string::npos) {
        op = FilterNode::Operator::LESS_THAN;
    } else if ((opPos = expr.find('>')) != std::string::npos) {
        op = FilterNode::Operator::GREATER_THAN;
    } else if ((opPos = expr.find('=')) != std::string::npos) {
        op = FilterNode::Operator::EQUAL;
    } else {
        throw std::invalid_argument("No operator found in comparison: " + expr);
    }

    // Extract key and value
    std::string key = expr.substr(0, opPos);
    size_t opLen = (op == FilterNode::Operator::NOT_EQUAL ||
                    op == FilterNode::Operator::LESS_EQUAL ||
                    op == FilterNode::Operator::GREATER_EQUAL) ? 2 : 1;
    std::string value = expr.substr(opPos + opLen);

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t\n\r"));
    key.erase(key.find_last_not_of(" \t\n\r") + 1);
    value.erase(0, value.find_first_not_of(" \t\n\r"));
    value.erase(value.find_last_not_of(" \t\n\r") + 1);

    if (key.empty()) {
        throw std::invalid_argument("Empty key in comparison");
    }

    // Check for presence operator (value is *)
    if (value == "*") {
        op = FilterNode::Operator::PRESENT;
    }

    node->op = op;
    node->key = key;
    node->value = value;

    return node;
}

bool EventFilter::evaluate(const FilterNode* node, const Event& event) const {
    if (!node) {
        return true;
    }

    switch (node->type) {
        case FilterNode::Type::AND: {
            for (const auto& child : node->children) {
                if (!evaluate(child.get(), event)) {
                    return false;
                }
            }
            return true;
        }

        case FilterNode::Type::OR: {
            for (const auto& child : node->children) {
                if (evaluate(child.get(), event)) {
                    return true;
                }
            }
            return false;
        }

        case FilterNode::Type::NOT: {
            if (node->children.empty()) {
                return true;
            }
            return !evaluate(node->children[0].get(), event);
        }

        case FilterNode::Type::COMPARISON: {
            // Special case for event type
            if (node->key == "type") {
                const std::string& eventType = event.getType();
                switch (node->op) {
                    case FilterNode::Operator::EQUAL:
                        // Use wildcard matching for type comparison
                        return event.matchesType(node->value);
                    case FilterNode::Operator::NOT_EQUAL:
                        return !event.matchesType(node->value);
                    case FilterNode::Operator::PRESENT:
                        return !eventType.empty();
                    default:
                        return false;
                }
            }

            // Check property existence
            if (!event.hasProperty(node->key)) {
                return false;
            }

            // Handle presence operator
            if (node->op == FilterNode::Operator::PRESENT) {
                return true;
            }

            // Get property value as string for comparison
            std::string propValue = event.getPropertyString(node->key);

            // Try numeric comparison first
            try {
                int propInt = std::stoi(propValue);
                int filterInt = std::stoi(node->value);

                switch (node->op) {
                    case FilterNode::Operator::EQUAL:
                        return propInt == filterInt;
                    case FilterNode::Operator::NOT_EQUAL:
                        return propInt != filterInt;
                    case FilterNode::Operator::LESS_THAN:
                        return propInt < filterInt;
                    case FilterNode::Operator::GREATER_THAN:
                        return propInt > filterInt;
                    case FilterNode::Operator::LESS_EQUAL:
                        return propInt <= filterInt;
                    case FilterNode::Operator::GREATER_EQUAL:
                        return propInt >= filterInt;
                    default:
                        return false;
                }
            } catch (const std::exception&) {
                // Not numeric, use string comparison
                switch (node->op) {
                    case FilterNode::Operator::EQUAL:
                        return propValue == node->value;
                    case FilterNode::Operator::NOT_EQUAL:
                        return propValue != node->value;
                    case FilterNode::Operator::LESS_THAN:
                        return propValue < node->value;
                    case FilterNode::Operator::GREATER_THAN:
                        return propValue > node->value;
                    case FilterNode::Operator::LESS_EQUAL:
                        return propValue <= node->value;
                    case FilterNode::Operator::GREATER_EQUAL:
                        return propValue >= node->value;
                    default:
                        return false;
                }
            }
        }
    }

    return false;
}

} // namespace cdmf
