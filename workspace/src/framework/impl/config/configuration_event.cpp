#include "config/configuration_event.h"
#include "config/configuration.h"
#include <sstream>

namespace cdmf {

std::string ConfigurationEvent::getEventTypeString(ConfigurationEventType type) {
    switch (type) {
        case ConfigurationEventType::CREATED:
            return EVENT_TYPE_CREATED;
        case ConfigurationEventType::UPDATED:
            return EVENT_TYPE_UPDATED;
        case ConfigurationEventType::DELETED:
            return EVENT_TYPE_DELETED;
        default:
            return "configuration.unknown";
    }
}

ConfigurationEvent::ConfigurationEvent(
    ConfigurationEventType type,
    const std::string& pid,
    Configuration* config,
    const Properties& oldProperties,
    const Properties& newProperties)
    : Event(getEventTypeString(type), config)
    , event_type_(type)
    , pid_(pid)
    , factory_pid_()
    , configuration_(config)
    , old_properties_(oldProperties)
    , new_properties_(newProperties)
{
    // Add event-specific properties
    setProperty("pid", pid);
    setProperty("event.type", eventTypeToString(type));

    if (config) {
        setProperty("configuration", std::any(static_cast<void*>(config)));
    }
}

ConfigurationEvent::ConfigurationEvent(
    ConfigurationEventType type,
    const std::string& pid,
    const std::string& factoryPid,
    Configuration* config,
    const Properties& oldProperties,
    const Properties& newProperties)
    : Event(getEventTypeString(type), config)
    , event_type_(type)
    , pid_(pid)
    , factory_pid_(factoryPid)
    , configuration_(config)
    , old_properties_(oldProperties)
    , new_properties_(newProperties)
{
    // Add event-specific properties
    setProperty("pid", pid);
    setProperty("factory.pid", factoryPid);
    setProperty("event.type", eventTypeToString(type));

    if (config) {
        setProperty("configuration", std::any(static_cast<void*>(config)));
    }
}

ConfigurationEvent::ConfigurationEvent(const ConfigurationEvent& other)
    : Event(other)
    , event_type_(other.event_type_)
    , pid_(other.pid_)
    , factory_pid_(other.factory_pid_)
    , configuration_(other.configuration_)
    , old_properties_(other.old_properties_)
    , new_properties_(other.new_properties_)
{
}

ConfigurationEvent::ConfigurationEvent(ConfigurationEvent&& other) noexcept
    : Event(std::move(other))
    , event_type_(other.event_type_)
    , pid_(std::move(other.pid_))
    , factory_pid_(std::move(other.factory_pid_))
    , configuration_(other.configuration_)
    , old_properties_(std::move(other.old_properties_))
    , new_properties_(std::move(other.new_properties_))
{
    other.configuration_ = nullptr;
}

ConfigurationEvent& ConfigurationEvent::operator=(const ConfigurationEvent& other) {
    if (this != &other) {
        Event::operator=(other);
        event_type_ = other.event_type_;
        pid_ = other.pid_;
        factory_pid_ = other.factory_pid_;
        configuration_ = other.configuration_;
        old_properties_ = other.old_properties_;
        new_properties_ = other.new_properties_;
    }
    return *this;
}

ConfigurationEvent& ConfigurationEvent::operator=(ConfigurationEvent&& other) noexcept {
    if (this != &other) {
        Event::operator=(std::move(other));
        event_type_ = other.event_type_;
        pid_ = std::move(other.pid_);
        factory_pid_ = std::move(other.factory_pid_);
        configuration_ = other.configuration_;
        old_properties_ = std::move(other.old_properties_);
        new_properties_ = std::move(other.new_properties_);
        other.configuration_ = nullptr;
    }
    return *this;
}

std::string ConfigurationEvent::toString() const {
    std::ostringstream oss;
    oss << "ConfigurationEvent{"
        << "type=" << eventTypeToString(event_type_)
        << ", pid=" << pid_;

    if (!factory_pid_.empty()) {
        oss << ", factoryPid=" << factory_pid_;
    }

    oss << ", configuration=" << (configuration_ ? "present" : "null");

    if (event_type_ == ConfigurationEventType::UPDATED) {
        oss << ", oldProperties.size=" << old_properties_.size()
            << ", newProperties.size=" << new_properties_.size();
    } else if (event_type_ == ConfigurationEventType::CREATED) {
        oss << ", newProperties.size=" << new_properties_.size();
    }

    oss << "}";
    return oss.str();
}

std::string ConfigurationEvent::eventTypeToString(ConfigurationEventType type) {
    switch (type) {
        case ConfigurationEventType::CREATED:
            return "CREATED";
        case ConfigurationEventType::UPDATED:
            return "UPDATED";
        case ConfigurationEventType::DELETED:
            return "DELETED";
        default:
            return "UNKNOWN";
    }
}

ConfigurationEventType ConfigurationEvent::stringToEventType(const std::string& typeString) {
    if (typeString == "CREATED") {
        return ConfigurationEventType::CREATED;
    } else if (typeString == "UPDATED") {
        return ConfigurationEventType::UPDATED;
    } else if (typeString == "DELETED") {
        return ConfigurationEventType::DELETED;
    }

    // Default fallback (should not happen in normal cases)
    return ConfigurationEventType::UPDATED;
}

} // namespace cdmf
