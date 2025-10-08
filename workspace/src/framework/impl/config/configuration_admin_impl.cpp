#include "config/configuration_admin_impl.h"
#include "config/configuration_event.h"
#include "utils/log.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <filesystem>

// Include nlohmann/json for JSON parsing
#include <nlohmann/json.hpp>

namespace cdmf {

namespace fs = std::filesystem;
using json = nlohmann::json;

ConfigurationAdminImpl::ConfigurationAdminImpl()
    : factory_instance_counter_(0)
{
    LOGI("Configuration Admin initialized");
}

ConfigurationAdminImpl::~ConfigurationAdminImpl() {
    LOGI("Configuration Admin shutdown - clearing all configurations");
    clearAll();
}

// ==================================================================
// Configuration Management
// ==================================================================

Configuration* ConfigurationAdminImpl::createConfiguration(const std::string& pid) {
    if (pid.empty()) {
        throw std::invalid_argument("PID cannot be empty");
    }

    if (!isValidPid(pid)) {
        throw std::invalid_argument("Invalid PID format: " + pid);
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already exists
    auto it = configurations_.find(pid);
    if (it != configurations_.end()) {
        LOGD("Configuration already exists: " + pid);
        return it->second.get();
    }

    // Create new configuration
    auto callback = [this](ConfigurationImpl* config,
                          const std::string& pid,
                          const std::string& factoryPid,
                          const Properties& oldProps,
                          const Properties& newProps,
                          bool removed) {
        this->onConfigurationChanged(config, pid, factoryPid, oldProps, newProps, removed);
    };

    auto config = std::make_unique<ConfigurationImpl>(pid, callback);
    Configuration* result = config.get();

    configurations_[pid] = std::move(config);

    LOGD("Configuration created: " + pid);

    // TODO: TEMPORARILY DISABLED - Fire CREATED event (with empty properties initially)
    // ConfigurationEvent event(
    //     ConfigurationEventType::CREATED,
    //     pid,
    //     result,
    //     Properties(),  // old properties (empty for new config)
    //     Properties()   // new properties (empty until first update)
    // );
    // fireConfigurationEvent(event);

    // LOGI("DEBUG: About to return from createConfiguration");

    return result;
}

Configuration* ConfigurationAdminImpl::getConfiguration(const std::string& pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = configurations_.find(pid);
    if (it != configurations_.end()) {
        return it->second.get();
    }

    return nullptr;
}

std::vector<Configuration*> ConfigurationAdminImpl::listConfigurations(
    const std::string& filter) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Configuration*> result;
    result.reserve(configurations_.size());

    for (const auto& pair : configurations_) {
        if (filter.empty() || matchesFilter(pair.first, filter)) {
            result.push_back(pair.second.get());
        }
    }

    return result;
}

bool ConfigurationAdminImpl::deleteConfiguration(const std::string& pid) {
    std::string factoryPid;
    Properties oldProps;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = configurations_.find(pid);
        if (it == configurations_.end()) {
            return false;
        }

        ConfigurationImpl* config = it->second.get();
        factoryPid = config->getFactoryPid();
        oldProps = config->getProperties();

        // Mark as deleted
        config->markDeleted();

        // Remove from factory instances if applicable
        if (!factoryPid.empty()) {
            auto factoryIt = factory_instances_.find(factoryPid);
            if (factoryIt != factory_instances_.end()) {
                factoryIt->second.erase(pid);
                if (factoryIt->second.empty()) {
                    factory_instances_.erase(factoryIt);
                }
            }
        }

        // Remove from storage
        configurations_.erase(it);
    } // Release lock

    // Fire DELETED event outside lock
    ConfigurationEvent event(
        ConfigurationEventType::DELETED,
        pid,
        factoryPid,
        nullptr,  // configuration is being deleted
        oldProps,
        Properties()
    );

    fireConfigurationEvent(event);

    LOGD("Configuration deleted: " + pid);

    return true;
}

// ==================================================================
// Factory Configurations
// ==================================================================

Configuration* ConfigurationAdminImpl::createFactoryConfiguration(
    const std::string& factoryPid) {

    if (factoryPid.empty()) {
        throw std::invalid_argument("Factory PID cannot be empty");
    }

    // Generate unique instance name
    std::string instanceName = generateInstanceName(factoryPid);
    return createFactoryConfiguration(factoryPid, instanceName);
}

Configuration* ConfigurationAdminImpl::createFactoryConfiguration(
    const std::string& factoryPid,
    const std::string& instanceName) {

    if (factoryPid.empty()) {
        throw std::invalid_argument("Factory PID cannot be empty");
    }

    if (instanceName.empty()) {
        throw std::invalid_argument("Instance name cannot be empty");
    }

    // Build instance PID
    std::string instancePid = buildInstancePid(factoryPid, instanceName);

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already exists
    auto it = configurations_.find(instancePid);
    if (it != configurations_.end()) {
        LOGW("Factory configuration instance already exists: " + instancePid);
        return it->second.get();
    }

    // Create new factory configuration
    auto callback = [this](ConfigurationImpl* config,
                          const std::string& pid,
                          const std::string& factoryPid,
                          const Properties& oldProps,
                          const Properties& newProps,
                          bool removed) {
        this->onConfigurationChanged(config, pid, factoryPid, oldProps, newProps, removed);
    };

    auto config = std::make_unique<ConfigurationImpl>(instancePid, factoryPid, callback);
    Configuration* result = config.get();

    configurations_[instancePid] = std::move(config);

    // Track factory instance
    factory_instances_[factoryPid].insert(instancePid);

    LOGI("Factory configuration created: factoryPid=" + factoryPid +
         ", instancePid=" + instancePid);

    // Fire CREATED event
    ConfigurationEvent event(
        ConfigurationEventType::CREATED,
        instancePid,
        factoryPid,
        result,
        Properties(),
        Properties()
    );

    fireConfigurationEvent(event);

    return result;
}

std::vector<Configuration*> ConfigurationAdminImpl::listFactoryConfigurations(
    const std::string& factoryPid) {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Configuration*> result;

    auto it = factory_instances_.find(factoryPid);
    if (it != factory_instances_.end()) {
        result.reserve(it->second.size());
        for (const auto& instancePid : it->second) {
            auto configIt = configurations_.find(instancePid);
            if (configIt != configurations_.end()) {
                result.push_back(configIt->second.get());
            }
        }
    }

    return result;
}

// ==================================================================
// File Persistence
// ==================================================================

bool ConfigurationAdminImpl::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOGE("Failed to open configuration file: " + path);
            return false;
        }

        json j;
        file >> j;

        // Parse PID
        if (!j.contains("pid") || !j["pid"].is_string()) {
            LOGE("Configuration file missing 'pid' field: " + path);
            return false;
        }

        std::string pid = j["pid"].get<std::string>();

        // Parse factory PID (optional)
        std::string factoryPid;
        if (j.contains("factoryPid") && j["factoryPid"].is_string()) {
            factoryPid = j["factoryPid"].get<std::string>();
        }

        // Parse properties
        Properties props;
        if (j.contains("properties") && j["properties"].is_object()) {
            for (auto& [key, value] : j["properties"].items()) {
                if (value.is_string()) {
                    props.set(key, value.get<std::string>());
                } else if (value.is_number_integer()) {
                    props.set(key, value.get<int>());
                } else if (value.is_number_float()) {
                    props.set(key, value.get<double>());
                } else if (value.is_boolean()) {
                    props.set(key, value.get<bool>());
                } else if (value.is_array()) {
                    std::vector<std::string> arr;
                    for (const auto& elem : value) {
                        if (elem.is_string()) {
                            arr.push_back(elem.get<std::string>());
                        }
                    }
                    props.set(key, arr);
                }
            }
        }

        // Create or update configuration
        Configuration* config;
        if (factoryPid.empty()) {
            config = createConfiguration(pid);
        } else {
            // Extract instance name from PID (format: factoryPid~instanceName)
            size_t pos = pid.find('~');
            std::string instanceName = (pos != std::string::npos)
                ? pid.substr(pos + 1)
                : generateInstanceName(factoryPid);

            config = createFactoryConfiguration(factoryPid, instanceName);
        }

        // Update properties if not empty
        if (!props.empty()) {
            config->update(props);
        }

        LOGI("Configuration loaded from file: " + path + " (PID: " + pid + ")");
        return true;

    } catch (const json::exception& e) {
        LOGE("JSON parse error in " + path + ": " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LOGE("Error loading configuration from " + path + ": " + std::string(e.what()));
        return false;
    }
}

bool ConfigurationAdminImpl::saveToFile(const std::string& path, const std::string& pid) {
    try {
        Configuration* config = getConfiguration(pid);
        if (!config) {
            LOGE("Configuration not found for PID: " + pid);
            return false;
        }

        json j;
        j["pid"] = pid;

        std::string factoryPid = config->getFactoryPid();
        if (!factoryPid.empty()) {
            j["factoryPid"] = factoryPid;
        }

        // Convert properties to JSON
        json propsJson = json::object();
        const Properties& props = config->getProperties();

        for (const auto& key : props.keys()) {
            // Try different types
            if (auto strVal = props.getAs<std::string>(key)) {
                propsJson[key] = strVal.value();
            } else if (auto intVal = props.getAs<int>(key)) {
                propsJson[key] = intVal.value();
            } else if (auto boolVal = props.getAs<bool>(key)) {
                propsJson[key] = boolVal.value();
            } else if (auto doubleVal = props.getAs<double>(key)) {
                propsJson[key] = doubleVal.value();
            } else if (auto arrVal = props.getAs<std::vector<std::string>>(key)) {
                propsJson[key] = arrVal.value();
            }
        }

        j["properties"] = propsJson;

        // Write to file
        std::ofstream file(path);
        if (!file.is_open()) {
            LOGE("Failed to open file for writing: " + path);
            return false;
        }

        file << j.dump(2);  // Pretty print with 2-space indent
        file.close();

        LOGI("Configuration saved to file: " + path + " (PID: " + pid + ")");
        return true;

    } catch (const std::exception& e) {
        LOGE("Error saving configuration to " + path + ": " + std::string(e.what()));
        return false;
    }
}

int ConfigurationAdminImpl::loadFromDirectory(const std::string& path) {
    int loaded = 0;

    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            LOGE("Directory does not exist: " + path);
            return 0;
        }

        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (loadFromFile(entry.path().string())) {
                    loaded++;
                }
            }
        }

        LOGI("Loaded " + std::to_string(loaded) + " configurations from directory: " + path);

    } catch (const std::exception& e) {
        LOGE("Error loading configurations from directory " + path + ": " +
             std::string(e.what()));
    }

    return loaded;
}

int ConfigurationAdminImpl::saveToDirectory(const std::string& path) {
    int saved = 0;

    try {
        // Create directory if it doesn't exist
        if (!fs::exists(path)) {
            fs::create_directories(path);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& pair : configurations_) {
            const std::string& pid = pair.first;

            // Generate filename from PID (replace special characters)
            std::string filename = pid;
            std::replace(filename.begin(), filename.end(), ':', '_');
            std::replace(filename.begin(), filename.end(), '~', '_');
            std::replace(filename.begin(), filename.end(), '/', '_');

            std::string filepath = path + "/" + filename + ".json";

            if (saveToFile(filepath, pid)) {
                saved++;
            }
        }

        LOGI("Saved " + std::to_string(saved) + " configurations to directory: " + path);

    } catch (const std::exception& e) {
        LOGE("Error saving configurations to directory " + path + ": " +
             std::string(e.what()));
    }

    return saved;
}

// ==================================================================
// Configuration Listeners
// ==================================================================

void ConfigurationAdminImpl::addConfigurationListener(IConfigurationListener* listener) {
    if (!listener) {
        throw std::invalid_argument("Listener cannot be null");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.insert(listener);

    LOGD("Configuration listener added (total: " + std::to_string(listeners_.size()) + ")");
}

bool ConfigurationAdminImpl::removeConfigurationListener(IConfigurationListener* listener) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t removed = listeners_.erase(listener);

    if (removed > 0) {
        LOGD("Configuration listener removed (remaining: " +
             std::to_string(listeners_.size()) + ")");
    }

    return removed > 0;
}

size_t ConfigurationAdminImpl::getListenerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listeners_.size();
}

// ==================================================================
// Statistics and Information
// ==================================================================

size_t ConfigurationAdminImpl::getConfigurationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return configurations_.size();
}

size_t ConfigurationAdminImpl::getFactoryConfigurationCount() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    for (const auto& pair : factory_instances_) {
        count += pair.second.size();
    }
    return count;
}

void ConfigurationAdminImpl::clearAll() {
    // Collect PIDs to delete
    std::vector<std::string> pids;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pids.reserve(configurations_.size());
        for (const auto& pair : configurations_) {
            pids.push_back(pair.first);
        }
    } // Release lock

    // Delete all configurations outside lock (fires DELETED events)
    for (const auto& pid : pids) {
        deleteConfiguration(pid);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Clear remaining data
        configurations_.clear();
        factory_instances_.clear();
    } // Release lock

    LOGD("All configurations cleared");
}

// ==================================================================
// Internal Methods
// ==================================================================

void ConfigurationAdminImpl::onConfigurationChanged(
    ConfigurationImpl* config,
    const std::string& pid,
    const std::string& factoryPid,
    const Properties& oldProps,
    const Properties& newProps,
    bool removed) {

    if (!config) {
        LOGW("ConfigurationAdminImpl::onConfigurationChanged() config is NULL, returning");
        return;
    }

    ConfigurationEventType eventType = removed
        ? ConfigurationEventType::DELETED
        : ConfigurationEventType::UPDATED;

    ConfigurationEvent event(
        eventType,
        pid,
        factoryPid,
        removed ? nullptr : config,
        oldProps,
        newProps
    );

    fireConfigurationEvent(event);
}

void ConfigurationAdminImpl::fireConfigurationEvent(const ConfigurationEvent& event) {
    // Copy listeners to avoid holding lock during callback
    std::vector<IConfigurationListener*> listenersCopy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        listenersCopy.reserve(listeners_.size());
        listenersCopy.assign(listeners_.begin(), listeners_.end());
    }

    // Fire event to all listeners (without holding lock)
    for (auto* listener : listenersCopy) {
        try {
            listener->configurationEvent(event);
        } catch (const std::exception& e) {
            LOGE("Exception in configuration listener: " + std::string(e.what()));
        } catch (...) {
            LOGE("Unknown exception in configuration listener");
        }
    }
}

std::string ConfigurationAdminImpl::generateInstanceName(const std::string& factoryPid) {
    uint64_t counter = factory_instance_counter_.fetch_add(1);
    return std::to_string(counter);
}

std::string ConfigurationAdminImpl::buildInstancePid(
    const std::string& factoryPid,
    const std::string& instanceName) {

    return factoryPid + "~" + instanceName;
}

bool ConfigurationAdminImpl::isValidPid(const std::string& pid) const {
    if (pid.empty()) {
        return false;
    }

    // PIDs should not start or end with dots or special chars
    if (pid.front() == '.' || pid.back() == '.') {
        return false;
    }

    return true;
}

bool ConfigurationAdminImpl::matchesFilter(const std::string& pid,
                                           const std::string& filter) const {
    if (filter.empty() || filter == "*") {
        return true;
    }

    // Simple wildcard matching
    if (filter.back() == '*') {
        std::string prefix = filter.substr(0, filter.length() - 1);
        return pid.substr(0, prefix.length()) == prefix;
    }

    // Exact match
    return pid == filter;
}

} // namespace cdmf
