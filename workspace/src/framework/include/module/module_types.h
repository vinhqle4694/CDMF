#ifndef CDMF_MODULE_TYPES_H
#define CDMF_MODULE_TYPES_H

#include <cstdint>
#include <string>

namespace cdmf {

/**
 * @brief Module state enumeration
 *
 * Represents the lifecycle state of a module in the CDMF framework.
 *
 * State Transitions:
 * - INSTALLED → RESOLVED → STARTING → ACTIVE → STOPPING → RESOLVED
 * - Any state (except ACTIVE) → UNINSTALLED
 */
enum class ModuleState {
    /**
     * @brief Module is loaded but dependencies not resolved
     *
     * Entry: Module installed via Framework::installModule()
     * Allowed operations: uninstall(), resolve()
     */
    INSTALLED,

    /**
     * @brief Dependencies satisfied, ready to start
     *
     * Entry: Dependency resolution successful
     * Allowed operations: start(), uninstall()
     */
    RESOLVED,

    /**
     * @brief Module is in the process of starting
     *
     * Entry: start() called
     * Duration: Typically 1-100ms
     * Timeout: 10 seconds (configurable)
     */
    STARTING,

    /**
     * @brief Module is fully operational
     *
     * Entry: ModuleActivator::start() completed successfully
     * Allowed operations: stop(), update()
     */
    ACTIVE,

    /**
     * @brief Module is in the process of stopping
     *
     * Entry: stop() called
     * Duration: Typically 1-50ms
     * Timeout: 5 seconds (configurable)
     */
    STOPPING,

    /**
     * @brief Module has been uninstalled (terminal state)
     *
     * Entry: uninstall() called
     * Module is no longer usable
     */
    UNINSTALLED
};

/**
 * @brief Convert ModuleState to string representation
 * @param state Module state
 * @return String representation
 */
inline const char* moduleStateToString(ModuleState state) {
    switch (state) {
        case ModuleState::INSTALLED:   return "INSTALLED";
        case ModuleState::RESOLVED:    return "RESOLVED";
        case ModuleState::STARTING:    return "STARTING";
        case ModuleState::ACTIVE:      return "ACTIVE";
        case ModuleState::STOPPING:    return "STOPPING";
        case ModuleState::UNINSTALLED: return "UNINSTALLED";
        default:                        return "UNKNOWN";
    }
}

/**
 * @brief Module event types
 *
 * Events fired during module lifecycle transitions.
 */
enum class ModuleEventType {
    MODULE_INSTALLED,      ///< Module installed into framework
    MODULE_RESOLVED,       ///< Module dependencies resolved
    MODULE_STARTING,       ///< Module starting (before activator.start())
    MODULE_STARTED,        ///< Module started (after activator.start())
    MODULE_STOPPING,       ///< Module stopping (before activator.stop())
    MODULE_STOPPED,        ///< Module stopped (after activator.stop())
    MODULE_UPDATED,        ///< Module updated to new version
    MODULE_UNINSTALLED,    ///< Module uninstalled
    MODULE_RESOLVED_FAILED ///< Module resolution failed
};

/**
 * @brief Convert ModuleEventType to string representation
 * @param eventType Module event type
 * @return String representation
 */
inline const char* moduleEventTypeToString(ModuleEventType eventType) {
    switch (eventType) {
        case ModuleEventType::MODULE_INSTALLED:       return "MODULE_INSTALLED";
        case ModuleEventType::MODULE_RESOLVED:        return "MODULE_RESOLVED";
        case ModuleEventType::MODULE_STARTING:        return "MODULE_STARTING";
        case ModuleEventType::MODULE_STARTED:         return "MODULE_STARTED";
        case ModuleEventType::MODULE_STOPPING:        return "MODULE_STOPPING";
        case ModuleEventType::MODULE_STOPPED:         return "MODULE_STOPPED";
        case ModuleEventType::MODULE_UPDATED:         return "MODULE_UPDATED";
        case ModuleEventType::MODULE_UNINSTALLED:     return "MODULE_UNINSTALLED";
        case ModuleEventType::MODULE_RESOLVED_FAILED: return "MODULE_RESOLVED_FAILED";
        default:                                       return "UNKNOWN";
    }
}

// Forward declarations
class Module;
class IModuleContext;
class IModuleActivator;
class ModuleRegistry;
class ModuleLoader;
class Framework;

} // namespace cdmf

#endif // CDMF_MODULE_TYPES_H
