#include "configuration_admin.h"
#include "utils/log.h"
#include <mutex>
#include <algorithm>
#include <stdexcept>

namespace cdmf {
namespace services {

ConfigurationAdmin::ConfigurationAdmin(const std::string& storageDir)
    : persistenceManager_(std::make_unique<PersistenceManager>(storageDir)) {
    LOGI_FMT("ConfigurationAdmin starting with storage directory: " << storageDir);
    loadConfigurations();
    LOGI_FMT("ConfigurationAdmin initialized with " << configurations_.size() << " configurations");
}

ConfigurationAdmin::~ConfigurationAdmin() {
    LOGI("ConfigurationAdmin shutting down, saving configurations");
    saveConfigurations();
    LOGI("ConfigurationAdmin shutdown complete");
}

Configuration* ConfigurationAdmin::createConfiguration(const std::string& pid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (configurations_.find(pid) != configurations_.end()) {
        LOGE_FMT("Configuration with PID '" << pid << "' already exists");
        throw std::runtime_error("Configuration with PID '" + pid + "' already exists");
    }

    auto config = std::make_unique<Configuration>(pid);
    Configuration* configPtr = config.get();
    configurations_[pid] = std::move(config);

    lock.unlock();

    LOGI_FMT("Created new configuration: " << pid);

    // Notify listeners
    ConfigurationEvent event(ConfigurationEventType::CREATED, pid, configPtr);
    notifyListeners(event);

    return configPtr;
}

Configuration* ConfigurationAdmin::getConfiguration(const std::string& pid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = configurations_.find(pid);
    if (it != configurations_.end()) {
        LOGD_FMT("Retrieved existing configuration: " << pid);
        return it->second.get();
    }

    // Create new configuration
    auto config = std::make_unique<Configuration>(pid);
    Configuration* configPtr = config.get();
    configurations_[pid] = std::move(config);

    lock.unlock();

    LOGI_FMT("Auto-created configuration: " << pid);

    // Notify listeners
    ConfigurationEvent event(ConfigurationEventType::CREATED, pid, configPtr);
    notifyListeners(event);

    return configPtr;
}

std::vector<Configuration*> ConfigurationAdmin::listConfigurations(const std::string& filter) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<Configuration*> result;
    for (const auto& pair : configurations_) {
        if (filter.empty() || matchesFilter(pair.second.get(), filter)) {
            result.push_back(pair.second.get());
        }
    }

    LOGD_FMT("Listed " << result.size() << " configurations (filter: '" << (filter.empty() ? "none" : filter) << "')");
    return result;
}

void ConfigurationAdmin::deleteConfiguration(const std::string& pid) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = configurations_.find(pid);
    if (it == configurations_.end()) {
        LOGD_FMT("Configuration not found, nothing to delete: " << pid);
        return;  // Configuration doesn't exist
    }

    Configuration* configPtr = it->second.get();
    configPtr->remove();

    lock.unlock();

    LOGI_FMT("Deleting configuration: " << pid);

    // Notify listeners before removing
    ConfigurationEvent event(ConfigurationEventType::DELETED, pid, configPtr);
    notifyListeners(event);

    // Remove from persistence
    persistenceManager_->remove(pid);

    // Remove from map
    lock.lock();
    configurations_.erase(it);
}

void ConfigurationAdmin::addConfigurationListener(ConfigurationListener* listener) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = std::find(listeners_.begin(), listeners_.end(), listener);
    if (it == listeners_.end()) {
        listeners_.push_back(listener);
        LOGD_FMT("Added configuration listener, total listeners: " << listeners_.size());
    }
}

void ConfigurationAdmin::removeConfigurationListener(ConfigurationListener* listener) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = std::find(listeners_.begin(), listeners_.end(), listener);
    if (it != listeners_.end()) {
        listeners_.erase(it);
        LOGD_FMT("Removed configuration listener, total listeners: " << listeners_.size());
    }
}

void ConfigurationAdmin::loadConfigurations() {
    LOGI("Loading configurations from persistent storage");
    auto pids = persistenceManager_->listAll();

    for (const auto& pid : pids) {
        auto config = std::make_unique<Configuration>(pid);
        auto properties = persistenceManager_->load(pid);
        config->update(properties);

        std::unique_lock<std::shared_mutex> lock(mutex_);
        configurations_[pid] = std::move(config);
    }

    LOGI_FMT("Loaded " << configurations_.size() << " configurations from storage");
}

void ConfigurationAdmin::saveConfigurations() {
    LOGI("Saving configurations to persistent storage");
    std::shared_lock<std::shared_mutex> lock(mutex_);

    int savedCount = 0;
    for (const auto& pair : configurations_) {
        if (!pair.second->isRemoved()) {
            persistenceManager_->save(pair.first, pair.second->getProperties());
            savedCount++;
        }
    }

    LOGI_FMT("Saved " << savedCount << " configurations to storage");
}

void ConfigurationAdmin::notifyListeners(const ConfigurationEvent& event) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto listenersCopy = listeners_;
    lock.unlock();

    LOGD_FMT("Notifying " << listenersCopy.size() << " listeners of configuration event for PID: " << event.getPid());

    for (auto* listener : listenersCopy) {
        try {
            listener->configurationEvent(event);
        } catch (const std::exception& e) {
            LOGW_FMT("Configuration listener threw exception: " << e.what());
        } catch (...) {
            LOGW("Configuration listener threw unknown exception");
        }
    }
}

bool ConfigurationAdmin::matchesFilter(const Configuration* config, const std::string& filter) const {
    // Simple filter implementation - checks if PID contains the filter string
    // A full implementation would support LDAP-style filters
    if (filter.empty()) {
        return true;
    }

    return config->getPid().find(filter) != std::string::npos;
}

} // namespace services
} // namespace cdmf
