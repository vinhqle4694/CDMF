#ifndef CDMF_CONFIGURATION_EVENT_H
#define CDMF_CONFIGURATION_EVENT_H

#include "core/event.h"
#include "utils/properties.h"
#include <string>
#include <memory>

namespace cdmf {

// Forward declaration
class Configuration;

/**
 * @brief Configuration event types
 *
 * Events fired when configurations are created, updated, or deleted.
 */
enum class ConfigurationEventType {
    /**
     * @brief Configuration was created
     *
     * Fired when a new configuration is created via ConfigurationAdmin.
     */
    CREATED,

    /**
     * @brief Configuration was updated
     *
     * Fired when configuration properties are modified via Configuration::update()
     */
    UPDATED,

    /**
     * @brief Configuration was deleted
     *
     * Fired when configuration is removed via Configuration::remove() or
     * ConfigurationAdmin::deleteConfiguration()
     */
    DELETED
};

/**
 * @brief Configuration event
 *
 * Events fired by the Configuration Admin service when configurations
 * are created, updated, or deleted. These events extend the base Event
 * class and provide access to:
 * - Event type (CREATED, UPDATED, DELETED)
 * - PID (Persistent Identifier) of the affected configuration
 * - Factory PID (if this is a factory configuration)
 * - Configuration object reference
 * - Old properties (for UPDATE events)
 * - New properties (for CREATE and UPDATE events)
 *
 * Configuration listeners receive these events to respond to dynamic
 * configuration changes at runtime.
 *
 * Thread-safety: ConfigurationEvent objects are immutable after construction
 * and safe to share across threads.
 *
 * Example usage:
 * @code
 * class MyModuleActivator : public IConfigurationListener {
 *     void configurationEvent(const ConfigurationEvent& event) override {
 *         if (event.getEventType() == ConfigurationEventType::UPDATED) {
 *             LOGI("Configuration updated: " + event.getPid());
 *
 *             // Check what changed
 *             Properties oldProps = event.getOldProperties();
 *             Properties newProps = event.getNewProperties();
 *
 *             if (oldProps.getInt("port") != newProps.getInt("port")) {
 *                 // Port changed - restart server
 *                 restartServer(newProps.getInt("port"));
 *             }
 *         }
 *     }
 * };
 * @endcode
 */
class ConfigurationEvent : public Event {
public:
    /**
     * @brief Event type string constants
     */
    static constexpr const char* EVENT_TYPE_CREATED = "configuration.created";
    static constexpr const char* EVENT_TYPE_UPDATED = "configuration.updated";
    static constexpr const char* EVENT_TYPE_DELETED = "configuration.deleted";

    /**
     * @brief Construct a configuration event
     *
     * @param type Event type (CREATED, UPDATED, or DELETED)
     * @param pid Persistent Identifier of the configuration
     * @param config Pointer to configuration object (can be nullptr for DELETED)
     * @param oldProperties Old properties (for UPDATED events, empty otherwise)
     * @param newProperties New properties (for CREATED/UPDATED, empty for DELETED)
     */
    ConfigurationEvent(
        ConfigurationEventType type,
        const std::string& pid,
        Configuration* config,
        const Properties& oldProperties = Properties(),
        const Properties& newProperties = Properties());

    /**
     * @brief Construct a configuration event with factory PID
     *
     * @param type Event type (CREATED, UPDATED, or DELETED)
     * @param pid Persistent Identifier of the configuration
     * @param factoryPid Factory PID (for factory configurations)
     * @param config Pointer to configuration object (can be nullptr for DELETED)
     * @param oldProperties Old properties (for UPDATED events, empty otherwise)
     * @param newProperties New properties (for CREATED/UPDATED, empty for DELETED)
     */
    ConfigurationEvent(
        ConfigurationEventType type,
        const std::string& pid,
        const std::string& factoryPid,
        Configuration* config,
        const Properties& oldProperties = Properties(),
        const Properties& newProperties = Properties());

    /**
     * @brief Copy constructor
     */
    ConfigurationEvent(const ConfigurationEvent& other);

    /**
     * @brief Move constructor
     */
    ConfigurationEvent(ConfigurationEvent&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    ConfigurationEvent& operator=(const ConfigurationEvent& other);

    /**
     * @brief Move assignment
     */
    ConfigurationEvent& operator=(ConfigurationEvent&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~ConfigurationEvent() override = default;

    /**
     * @brief Get the event type
     * @return Event type (CREATED, UPDATED, or DELETED)
     */
    ConfigurationEventType getEventType() const { return event_type_; }

    /**
     * @brief Get the configuration PID
     * @return Persistent Identifier
     */
    const std::string& getPid() const { return pid_; }

    /**
     * @brief Get the factory PID
     * @return Factory PID, or empty string if not a factory configuration
     */
    const std::string& getFactoryPid() const { return factory_pid_; }

    /**
     * @brief Get the configuration object
     * @return Pointer to configuration, or nullptr if deleted
     */
    Configuration* getConfiguration() const { return configuration_; }

    /**
     * @brief Get the old properties (before update)
     *
     * Only meaningful for UPDATED events. For CREATED and DELETED events,
     * returns an empty Properties object.
     *
     * @return Old properties
     */
    const Properties& getOldProperties() const { return old_properties_; }

    /**
     * @brief Get the new properties (after update/create)
     *
     * Meaningful for CREATED and UPDATED events. For DELETED events,
     * returns an empty Properties object.
     *
     * @return New properties
     */
    const Properties& getNewProperties() const { return new_properties_; }

    /**
     * @brief Check if this is a factory configuration event
     * @return true if factory PID is set, false otherwise
     */
    bool isFactoryConfiguration() const { return !factory_pid_.empty(); }

    /**
     * @brief Convert event to string representation
     * @return Human-readable string
     */
    std::string toString() const;

    /**
     * @brief Convert event type enum to string
     * @param type Event type
     * @return Event type string
     */
    static std::string eventTypeToString(ConfigurationEventType type);

    /**
     * @brief Convert event type string to enum
     * @param typeString Event type string
     * @return Event type enum
     */
    static ConfigurationEventType stringToEventType(const std::string& typeString);

private:
    ConfigurationEventType event_type_;
    std::string pid_;
    std::string factory_pid_;
    Configuration* configuration_;
    Properties old_properties_;
    Properties new_properties_;

    /**
     * @brief Convert event type to event string
     */
    static std::string getEventTypeString(ConfigurationEventType type);
};

} // namespace cdmf

#endif // CDMF_CONFIGURATION_EVENT_H
