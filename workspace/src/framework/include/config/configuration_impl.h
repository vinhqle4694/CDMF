#ifndef CDMF_CONFIGURATION_IMPL_H
#define CDMF_CONFIGURATION_IMPL_H

#include "config/configuration.h"
#include <mutex>
#include <functional>

namespace cdmf {

// Forward declaration
class ConfigurationAdminImpl;

/**
 * @brief Configuration implementation
 *
 * Thread-safe implementation of the Configuration interface.
 * Manages configuration properties and lifecycle.
 */
class ConfigurationImpl : public Configuration {
public:
    /**
     * @brief Callback function for configuration updates
     *
     * Called when configuration is updated or removed.
     * Used by ConfigurationAdmin to fire events.
     *
     * @param config Pointer to this configuration
     * @param pid Configuration PID
     * @param factoryPid Factory PID (empty if not a factory configuration)
     * @param oldProps Old properties (before update)
     * @param newProps New properties (after update, empty for removal)
     * @param removed true if configuration was removed, false if updated
     */
    using UpdateCallback = std::function<void(
        ConfigurationImpl* config,
        const std::string& pid,
        const std::string& factoryPid,
        const Properties& oldProps,
        const Properties& newProps,
        bool removed)>;

    /**
     * @brief Construct a regular configuration
     *
     * @param pid Persistent Identifier
     * @param callback Callback for update/remove events
     */
    ConfigurationImpl(const std::string& pid, UpdateCallback callback);

    /**
     * @brief Construct a factory configuration
     *
     * @param pid Persistent Identifier (instance PID)
     * @param factoryPid Factory PID
     * @param callback Callback for update/remove events
     */
    ConfigurationImpl(const std::string& pid,
                      const std::string& factoryPid,
                      UpdateCallback callback);

    /**
     * @brief Destructor
     */
    ~ConfigurationImpl() override = default;

    // ==================================================================
    // Identity
    // ==================================================================

    std::string getPid() const override;
    std::string getFactoryPid() const override;
    bool isFactoryConfiguration() const override;

    // ==================================================================
    // Properties Access
    // ==================================================================

    const Properties& getProperties() const override;
    Properties& getProperties() override;

    // ==================================================================
    // Type-Safe Getters
    // ==================================================================

    std::string getString(const std::string& key,
                          const std::string& defaultValue = "") const override;
    int getInt(const std::string& key, int defaultValue = 0) const override;
    bool getBool(const std::string& key, bool defaultValue = false) const override;
    double getDouble(const std::string& key, double defaultValue = 0.0) const override;
    long getLong(const std::string& key, long defaultValue = 0L) const override;
    std::vector<std::string> getStringArray(const std::string& key) const override;
    bool hasProperty(const std::string& key) const override;

    // ==================================================================
    // Configuration Modification
    // ==================================================================

    void update(const Properties& props) override;
    void remove() override;

    // ==================================================================
    // State
    // ==================================================================

    bool isDeleted() const override;
    size_t size() const override;
    bool empty() const override;

    // ==================================================================
    // Internal API (used by ConfigurationAdmin)
    // ==================================================================

    /**
     * @brief Set properties without firing events
     *
     * Used by ConfigurationAdmin during initial creation.
     *
     * @param props Properties to set
     */
    void setPropertiesInternal(const Properties& props);

    /**
     * @brief Mark configuration as deleted
     *
     * Used by ConfigurationAdmin when configuration is deleted.
     */
    void markDeleted();

private:
    std::string pid_;
    std::string factory_pid_;
    Properties properties_;
    bool deleted_;
    mutable std::mutex mutex_;
    UpdateCallback update_callback_;

    /**
     * @brief Check if deleted and throw if true
     */
    void ensureNotDeleted() const;
};

} // namespace cdmf

#endif // CDMF_CONFIGURATION_IMPL_H
