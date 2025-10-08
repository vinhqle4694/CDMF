#ifndef CDMF_CONFIGURATION_LISTENER_H
#define CDMF_CONFIGURATION_LISTENER_H

namespace cdmf {

// Forward declaration
class ConfigurationEvent;

/**
 * @brief Configuration listener interface
 *
 * Modules and components can implement this interface to receive
 * notifications when configurations are created, updated, or deleted.
 *
 * The listener is notified via configurationEvent() method whenever:
 * - A configuration is created (ConfigurationEventType::CREATED)
 * - A configuration is updated (ConfigurationEventType::UPDATED)
 * - A configuration is deleted (ConfigurationEventType::DELETED)
 *
 * Listeners are registered via:
 * - IModuleContext::addConfigurationListener()
 * - IConfigurationAdmin::addConfigurationListener()
 *
 * Listeners are automatically unregistered when:
 * - Module stops (for listeners registered via IModuleContext)
 * - Listener is explicitly removed
 * - Framework shuts down
 *
 * Thread-safety:
 * - configurationEvent() may be called from any thread
 * - Implementations must be thread-safe
 * - Do not perform blocking operations in configurationEvent()
 *
 * Example usage:
 * @code
 * class MyModuleActivator : public IModuleActivator,
 *                           public IConfigurationListener {
 * public:
 *     void start(IModuleContext* context) override {
 *         // Get initial configuration
 *         config_ = context->getConfiguration();
 *         if (config_) {
 *             applyConfiguration(config_);
 *         }
 *
 *         // Listen for configuration updates
 *         context->addConfigurationListener(this);
 *     }
 *
 *     void stop(IModuleContext* context) override {
 *         // Listener is automatically removed when module stops
 *     }
 *
 *     void configurationEvent(const ConfigurationEvent& event) override {
 *         if (event.getEventType() == ConfigurationEventType::UPDATED) {
 *             // Apply new configuration
 *             Configuration* newConfig = event.getConfiguration();
 *             applyConfiguration(newConfig);
 *         } else if (event.getEventType() == ConfigurationEventType::DELETED) {
 *             // Configuration was deleted, use defaults
 *             applyDefaults();
 *         }
 *     }
 *
 * private:
 *     void applyConfiguration(Configuration* config) {
 *         int port = config->getInt("server.port", 8080);
 *         int threads = config->getInt("server.threads", 4);
 *         // Apply settings...
 *     }
 *
 *     void applyDefaults() {
 *         // Use default configuration
 *     }
 *
 *     Configuration* config_;
 * };
 * @endcode
 *
 * Advanced filtering example:
 * @code
 * class DatabaseManager : public IConfigurationListener {
 * public:
 *     void start(IConfigurationAdmin* configAdmin) {
 *         // Listen for all database connection configurations
 *         // (factory configurations with PID pattern: factory.database.connection~*)
 *         configAdmin->addConfigurationListener(this);
 *     }
 *
 *     void configurationEvent(const ConfigurationEvent& event) override {
 *         if (event.getFactoryPid() == "factory.database.connection") {
 *             if (event.getEventType() == ConfigurationEventType::CREATED) {
 *                 // New database connection created
 *                 createConnectionPool(event.getConfiguration());
 *             } else if (event.getEventType() == ConfigurationEventType::DELETED) {
 *                 // Database connection deleted
 *                 removeConnectionPool(event.getPid());
 *             }
 *         }
 *     }
 *
 * private:
 *     std::map<std::string, ConnectionPool*> pools_;
 * };
 * @endcode
 */
class IConfigurationListener {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IConfigurationListener() = default;

    /**
     * @brief Called when a configuration event occurs
     *
     * This method is invoked whenever a configuration is created,
     * updated, or deleted. The listener should respond to the event
     * as appropriate for its use case.
     *
     * Important notes:
     * - This method may be called from any thread (thread-safe required)
     * - Do not perform blocking operations (use async operations)
     * - Do not throw exceptions (catch and log internally)
     * - Do not modify the configuration within this callback
     *   (use async operation to avoid deadlock)
     *
     * @param event Configuration event containing:
     *              - Event type (CREATED, UPDATED, DELETED)
     *              - PID of affected configuration
     *              - Configuration object reference (may be null for DELETED)
     *              - Old and new properties (for UPDATED events)
     */
    virtual void configurationEvent(const ConfigurationEvent& event) = 0;
};

} // namespace cdmf

#endif // CDMF_CONFIGURATION_LISTENER_H
