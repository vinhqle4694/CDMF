#include "config/configuration_impl.h"
#include "utils/log.h"
#include <stdexcept>
#include <sstream>

namespace cdmf {

ConfigurationImpl::ConfigurationImpl(const std::string& pid, UpdateCallback callback)
    : pid_(pid)
    , factory_pid_()
    , properties_()
    , deleted_(false)
    , update_callback_(std::move(callback))
{
}

ConfigurationImpl::ConfigurationImpl(const std::string& pid,
                                     const std::string& factoryPid,
                                     UpdateCallback callback)
    : pid_(pid)
    , factory_pid_(factoryPid)
    , properties_()
    , deleted_(false)
    , update_callback_(std::move(callback))
{
}

// ==================================================================
// Identity
// ==================================================================

std::string ConfigurationImpl::getPid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pid_;
}

std::string ConfigurationImpl::getFactoryPid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return factory_pid_;
}

bool ConfigurationImpl::isFactoryConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !factory_pid_.empty();
}

// ==================================================================
// Properties Access
// ==================================================================

const Properties& ConfigurationImpl::getProperties() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_;
}

Properties& ConfigurationImpl::getProperties() {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_;
}

// ==================================================================
// Type-Safe Getters
// ==================================================================

std::string ConfigurationImpl::getString(const std::string& key,
                                         const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.getString(key, defaultValue);
}

int ConfigurationImpl::getInt(const std::string& key, int defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.getInt(key, defaultValue);
}

bool ConfigurationImpl::getBool(const std::string& key, bool defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.getBool(key, defaultValue);
}

double ConfigurationImpl::getDouble(const std::string& key, double defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.getDouble(key, defaultValue);
}

long ConfigurationImpl::getLong(const std::string& key, long defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.getLong(key, defaultValue);
}

std::vector<std::string> ConfigurationImpl::getStringArray(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto value = properties_.getAs<std::vector<std::string>>(key);
    if (value.has_value()) {
        return value.value();
    }

    // Try to parse as comma-separated string
    std::string str = properties_.getString(key, "");
    if (str.empty()) {
        return std::vector<std::string>();
    }

    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            result.push_back(item.substr(start, end - start + 1));
        } else if (start != std::string::npos) {
            result.push_back(item.substr(start));
        }
    }

    return result;
}

bool ConfigurationImpl::hasProperty(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.has(key);
}

// ==================================================================
// Configuration Modification
// ==================================================================

void ConfigurationImpl::update(const Properties& props) {
    Properties oldProps;
    std::string pidCopy;
    std::string factoryPidCopy;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        ensureNotDeleted();

        // Save old properties for event
        oldProps = properties_;

        // Update properties
        properties_ = props;

        // Copy PID and factory PID for callback and logging
        pidCopy = pid_;
        factoryPidCopy = factory_pid_;
    }

    if (update_callback_) {
        update_callback_(this, pidCopy, factoryPidCopy, oldProps, props, false);
    }

    LOGD("Configuration updated: PID=" + pidCopy + ", properties=" +
         std::to_string(props.size()));
}

void ConfigurationImpl::remove() {
    Properties oldProps;
    std::string pidCopy;
    std::string factoryPidCopy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureNotDeleted();

        // Save old properties for event
        oldProps = properties_;

        // Mark as deleted
        deleted_ = true;

        // Copy PID and factory PID for callback and logging
        pidCopy = pid_;
        factoryPidCopy = factory_pid_;
    }

    // Fire remove callback outside the lock to avoid deadlock
    if (update_callback_) {
        update_callback_(this, pidCopy, factoryPidCopy, oldProps, Properties(), true);
    }

    LOGD("Configuration removed: PID=" + pidCopy);
}

// ==================================================================
// State
// ==================================================================

bool ConfigurationImpl::isDeleted() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return deleted_;
}

size_t ConfigurationImpl::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.size();
}

bool ConfigurationImpl::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_.empty();
}

// ==================================================================
// Internal API
// ==================================================================

void ConfigurationImpl::setPropertiesInternal(const Properties& props) {
    std::lock_guard<std::mutex> lock(mutex_);
    properties_ = props;
}

void ConfigurationImpl::markDeleted() {
    std::lock_guard<std::mutex> lock(mutex_);
    deleted_ = true;
}

// ==================================================================
// Private Methods
// ==================================================================

void ConfigurationImpl::ensureNotDeleted() const {
    if (deleted_) {
        throw std::runtime_error("Configuration has been deleted: PID=" + pid_);
    }
}

} // namespace cdmf
