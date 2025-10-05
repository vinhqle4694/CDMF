#ifndef CDMF_FRAMEWORK_TYPES_H
#define CDMF_FRAMEWORK_TYPES_H

#include <string>
#include <ostream>

namespace cdmf {

/**
 * @brief Framework lifecycle states
 *
 * State transitions:
 * CREATED -> STARTING -> ACTIVE -> STOPPING -> STOPPED
 */
enum class FrameworkState {
    CREATED,    ///< Framework created but not initialized
    STARTING,   ///< Framework is initializing
    ACTIVE,     ///< Framework is running
    STOPPING,   ///< Framework is shutting down
    STOPPED     ///< Framework has stopped
};

/**
 * @brief Convert FrameworkState to string
 */
inline std::string frameworkStateToString(FrameworkState state) {
    switch (state) {
        case FrameworkState::CREATED:  return "CREATED";
        case FrameworkState::STARTING: return "STARTING";
        case FrameworkState::ACTIVE:   return "ACTIVE";
        case FrameworkState::STOPPING: return "STOPPING";
        case FrameworkState::STOPPED:  return "STOPPED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Stream output operator for FrameworkState
 */
inline std::ostream& operator<<(std::ostream& os, FrameworkState state) {
    os << frameworkStateToString(state);
    return os;
}

/**
 * @brief Framework event types
 */
enum class FrameworkEventType {
    STARTED,              ///< Framework has started
    STOPPED,              ///< Framework has stopped
    ERROR,                ///< Framework encountered an error
    WARNING,              ///< Framework warning
    INFO,                 ///< Framework informational event

    // Module events
    MODULE_INSTALLED,     ///< Module has been installed
    MODULE_STARTED,       ///< Module has been started
    MODULE_STOPPED,       ///< Module has been stopped
    MODULE_UNINSTALLED,   ///< Module has been uninstalled
    MODULE_UPDATED,       ///< Module has been updated
    MODULE_RESOLVED,      ///< Module dependencies resolved
    MODULE_UNRESOLVED,    ///< Module dependencies could not be resolved

    // Service events
    SERVICE_REGISTERED,   ///< Service has been registered
    SERVICE_UNREGISTERED, ///< Service has been unregistered
    SERVICE_MODIFIED,     ///< Service properties modified

    // Boot events
    BOOT_COMPLETED        ///< All services started successfully
};

/**
 * @brief Convert FrameworkEventType to string
 */
inline std::string frameworkEventTypeToString(FrameworkEventType type) {
    switch (type) {
        case FrameworkEventType::STARTED:              return "STARTED";
        case FrameworkEventType::STOPPED:              return "STOPPED";
        case FrameworkEventType::ERROR:                return "ERROR";
        case FrameworkEventType::WARNING:              return "WARNING";
        case FrameworkEventType::INFO:                 return "INFO";
        case FrameworkEventType::MODULE_INSTALLED:     return "MODULE_INSTALLED";
        case FrameworkEventType::MODULE_STARTED:       return "MODULE_STARTED";
        case FrameworkEventType::MODULE_STOPPED:       return "MODULE_STOPPED";
        case FrameworkEventType::MODULE_UNINSTALLED:   return "MODULE_UNINSTALLED";
        case FrameworkEventType::MODULE_UPDATED:       return "MODULE_UPDATED";
        case FrameworkEventType::MODULE_RESOLVED:      return "MODULE_RESOLVED";
        case FrameworkEventType::MODULE_UNRESOLVED:    return "MODULE_UNRESOLVED";
        case FrameworkEventType::SERVICE_REGISTERED:   return "SERVICE_REGISTERED";
        case FrameworkEventType::SERVICE_UNREGISTERED: return "SERVICE_UNREGISTERED";
        case FrameworkEventType::SERVICE_MODIFIED:     return "SERVICE_MODIFIED";
        case FrameworkEventType::BOOT_COMPLETED:       return "BOOT_COMPLETED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Stream output operator for FrameworkEventType
 */
inline std::ostream& operator<<(std::ostream& os, FrameworkEventType type) {
    os << frameworkEventTypeToString(type);
    return os;
}

// Forward declarations for framework components
class Framework;
class FrameworkImpl;
class Module;
class IModuleContext;
class IFrameworkListener;

} // namespace cdmf

#endif // CDMF_FRAMEWORK_TYPES_H
