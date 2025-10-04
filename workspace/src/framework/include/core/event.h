#ifndef CDMF_EVENT_H
#define CDMF_EVENT_H

#include "utils/properties.h"
#include <string>
#include <chrono>
#include <memory>

namespace cdmf {

/**
 * @brief Base event class for the event system
 *
 * Events have:
 * - Type: String identifying the event type (e.g., "module.installed")
 * - Source: Pointer to the object that generated the event
 * - Timestamp: When the event was created
 * - Properties: Additional event data
 *
 * Events are immutable after creation except for properties.
 */
class Event {
public:
    using TimePoint = std::chrono::system_clock::time_point;

    /**
     * @brief Construct an event
     * @param type Event type string
     * @param source Pointer to event source (can be nullptr)
     */
    Event(const std::string& type, void* source = nullptr);

    /**
     * @brief Construct an event with properties
     * @param type Event type string
     * @param source Pointer to event source
     * @param properties Event properties
     */
    Event(const std::string& type, void* source, const Properties& properties);

    /**
     * @brief Copy constructor
     */
    Event(const Event& other);

    /**
     * @brief Move constructor
     */
    Event(Event&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    Event& operator=(const Event& other);

    /**
     * @brief Move assignment
     */
    Event& operator=(Event&& other) noexcept;

    /**
     * @brief Destructor
     */
    virtual ~Event() = default;

    /**
     * @brief Get event type
     */
    const std::string& getType() const { return type_; }

    /**
     * @brief Get event source
     */
    void* getSource() const { return source_; }

    /**
     * @brief Get event timestamp
     */
    const TimePoint& getTimestamp() const { return timestamp_; }

    /**
     * @brief Get event properties (read-only)
     */
    const Properties& getProperties() const { return properties_; }

    /**
     * @brief Get event properties (mutable)
     */
    Properties& getProperties() { return properties_; }

    /**
     * @brief Set a property
     * @param key Property key
     * @param value Property value
     */
    void setProperty(const std::string& key, const std::any& value);

    /**
     * @brief Get a property
     * @param key Property key
     * @return Property value as std::any, or empty std::any if not found
     */
    std::any getProperty(const std::string& key) const;

    /**
     * @brief Check if property exists
     * @param key Property key
     * @return true if property exists, false otherwise
     */
    bool hasProperty(const std::string& key) const;

    /**
     * @brief Get property as string
     * @param key Property key
     * @param defaultValue Default value if not found
     * @return Property value or default
     */
    std::string getPropertyString(const std::string& key,
                                  const std::string& defaultValue = "") const;

    /**
     * @brief Get property as int
     * @param key Property key
     * @param defaultValue Default value if not found
     * @return Property value or default
     */
    int getPropertyInt(const std::string& key, int defaultValue = 0) const;

    /**
     * @brief Get property as bool
     * @param key Property key
     * @param defaultValue Default value if not found
     * @return Property value or default
     */
    bool getPropertyBool(const std::string& key, bool defaultValue = false) const;

    /**
     * @brief Get time since event creation
     * @return Duration since event was created
     */
    std::chrono::milliseconds getAge() const;

    /**
     * @brief Check if event type matches a pattern
     *
     * Supports wildcard matching with '*':
     * - "module.*" matches "module.installed", "module.started", etc.
     * - "*" matches all types
     *
     * @param pattern Pattern to match against
     * @return true if type matches pattern, false otherwise
     */
    bool matchesType(const std::string& pattern) const;

    /**
     * @brief Convert event to string representation
     */
    std::string toString() const;

private:
    std::string type_;
    void* source_;
    TimePoint timestamp_;
    Properties properties_;
};

} // namespace cdmf

#endif // CDMF_EVENT_H
