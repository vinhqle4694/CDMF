#include "configuration_admin.h"
#include "utils/log.h"
#include <mutex>
#include <algorithm>
#include <stdexcept>
#include <sstream>

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

std::string ConfigurationAdmin::dispatchCommand(const std::string& methodName,
                                               const std::vector<std::string>& args) {
    LOGD_FMT("Dispatching command: " << methodName << " with " << args.size() << " argument(s)");

    try {
        // createConfiguration
        if (methodName == "createConfiguration") {
            if (args.size() != 1) {
                return "Error: createConfiguration requires exactly 1 argument: <pid>\n"
                       "Usage: call cdmf::IConfigurationAdmin createConfiguration <pid>";
            }

            createConfiguration(args[0]);
            return "Created configuration: " + args[0];
        }

        // getConfiguration
        else if (methodName == "getConfiguration") {
            if (args.size() != 1) {
                return "Error: getConfiguration requires exactly 1 argument: <pid>\n"
                       "Usage: call cdmf::IConfigurationAdmin getConfiguration <pid>";
            }

            Configuration* config = getConfiguration(args[0]);
            if (!config) {
                return "Error: Failed to get configuration";
            }

            std::ostringstream oss;
            oss << "Configuration: " << config->getPid() << "\n";

            auto& props = config->getProperties();
            auto keys = props.keys();

            if (keys.empty()) {
                oss << "  (No properties set)\n";
            } else {
                oss << "Properties (" << keys.size() << "):\n";
                for (const auto& key : keys) {
                    oss << "  " << key << " = " << props.getString(key, "") << "\n";
                }
            }

            return oss.str();
        }

        // listConfigurations
        else if (methodName == "listConfigurations") {
            std::string filter = args.empty() ? "" : args[0];

            auto configs = listConfigurations(filter);

            std::ostringstream oss;
            oss << "Configurations (" << configs.size() << ")";
            if (!filter.empty()) {
                oss << " [filter: \"" << filter << "\"]";
            }
            oss << ":\n";

            if (configs.empty()) {
                oss << "  (No configurations found)\n";
            } else {
                for (auto* config : configs) {
                    auto& props = config->getProperties();
                    oss << "  * " << config->getPid();
                    if (!props.keys().empty()) {
                        oss << " (" << props.keys().size() << " properties)";
                    }
                    oss << "\n";
                }
            }

            return oss.str();
        }

        // deleteConfiguration
        else if (methodName == "deleteConfiguration") {
            if (args.size() != 1) {
                return "Error: deleteConfiguration requires exactly 1 argument: <pid>\n"
                       "Usage: call cdmf::IConfigurationAdmin deleteConfiguration <pid>";
            }

            deleteConfiguration(args[0]);
            return "Deleted configuration: " + args[0];
        }

        // Unknown method
        else {
            return "Error: Unknown method '" + methodName + "' for service cdmf::IConfigurationAdmin\n"
                   "Use 'call cdmf::IConfigurationAdmin --help' to see available methods.";
        }

    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // namespace services
} // namespace cdmf
