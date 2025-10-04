#include "persistence_manager.h"
#include "utils/log.h"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace cdmf {
namespace services {

PersistenceManager::PersistenceManager(const std::string& storageDir)
    : storageDir_(storageDir) {
    ensureStorageDirectory();
    LOGI_FMT("PersistenceManager initialized with storage directory: " << storageDir);
}

PersistenceManager::~PersistenceManager() = default;

cdmf::Properties PersistenceManager::load(const std::string& pid) {
    using json = nlohmann::json;
    std::string filePath = getFilePath(pid);
    cdmf::Properties props;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOGD_FMT("Configuration file not found: " << filePath << ", returning empty properties");
        return props;
    }

    LOGD_FMT("Loading configuration from: " << filePath);

    try {
        json j;
        file >> j;

        for (auto& [key, value] : j.items()) {
            if (value.is_string()) {
                props.set(key, value.get<std::string>());
            } else if (value.is_number_integer()) {
                props.set(key, value.get<int>());
            } else if (value.is_number_float()) {
                props.set(key, value.get<double>());
            } else if (value.is_boolean()) {
                props.set(key, value.get<bool>());
            } else {
                // Store as string for unsupported types
                props.set(key, value.dump());
            }
        }

        file.close();
        LOGI_FMT("Loaded configuration: " << pid << " with " << props.keys().size() << " properties");
    } catch (const json::exception& e) {
        LOGE_FMT("Failed to parse JSON configuration file: " << filePath << ", error: " << e.what());
        file.close();
    }

    return props;
}

void PersistenceManager::save(const std::string& pid, const cdmf::Properties& properties) {
    using json = nlohmann::json;
    std::string filePath = getFilePath(pid);

    LOGD_FMT("Saving configuration: " << pid << " to " << filePath);

    std::ofstream file(filePath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open configuration file for writing: " << filePath);
        throw std::runtime_error("Failed to open configuration file for writing: " + filePath);
    }

    json j;
    auto keys = properties.keys();

    for (const auto& key : keys) {
        auto valueAny = properties.get(key);

        if (valueAny.has_value()) {
            if (valueAny.type() == typeid(std::string)) {
                j[key] = std::any_cast<std::string>(valueAny);
            } else if (valueAny.type() == typeid(const char*)) {
                j[key] = std::string(std::any_cast<const char*>(valueAny));
            } else if (valueAny.type() == typeid(char*)) {
                j[key] = std::string(std::any_cast<char*>(valueAny));
            } else if (valueAny.type() == typeid(int)) {
                j[key] = std::any_cast<int>(valueAny);
            } else if (valueAny.type() == typeid(bool)) {
                j[key] = std::any_cast<bool>(valueAny);
            } else if (valueAny.type() == typeid(double)) {
                j[key] = std::any_cast<double>(valueAny);
            } else if (valueAny.type() == typeid(long)) {
                j[key] = std::any_cast<long>(valueAny);
            } else {
                LOGW_FMT("Unsupported type for key '" << key << "': " << valueAny.type().name());
            }
        }
    }

    file << j.dump(4);  // Pretty print with 4 space indentation
    file.close();

    LOGI_FMT("Saved configuration: " << pid << " with " << properties.keys().size() << " properties");
}

void PersistenceManager::remove(const std::string& pid) {
    std::string filePath = getFilePath(pid);

    if (std::filesystem::exists(filePath)) {
        std::filesystem::remove(filePath);
        LOGI_FMT("Removed configuration file: " << filePath);
    } else {
        LOGD_FMT("Configuration file does not exist, nothing to remove: " << filePath);
    }
}

std::vector<std::string> PersistenceManager::listAll() {
    std::vector<std::string> pids;

    if (!std::filesystem::exists(storageDir_)) {
        LOGD_FMT("Storage directory does not exist: " << storageDir_);
        return pids;
    }

    for (const auto& entry : std::filesystem::directory_iterator(storageDir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::string pid = entry.path().stem().string();
            pids.push_back(pid);
        }
    }

    LOGD_FMT("Found " << pids.size() << " configuration files in " << storageDir_);
    return pids;
}

std::string PersistenceManager::getFilePath(const std::string& pid) const {
    return storageDir_ + "/" + pid + ".json";
}

void PersistenceManager::ensureStorageDirectory() {
    if (!std::filesystem::exists(storageDir_)) {
        std::filesystem::create_directories(storageDir_);
        LOGI_FMT("Created storage directory: " << storageDir_);
    }
}

} // namespace services
} // namespace cdmf
