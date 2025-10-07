/**
 * @file config_analyzer.cpp
 * @brief Implementation of ConfigAnalyzer
 */

#include "config_analyzer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace cdmf {
namespace automation {

ConfigAnalyzer::ConfigAnalyzer(const std::string& config_file_path)
    : config_file_path_(config_file_path)
    , loaded_(false)
{
}

bool ConfigAnalyzer::load() {
    try {
        std::ifstream file(config_file_path_);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << config_file_path_ << std::endl;
            return false;
        }

        file >> config_;
        loaded_ = true;
        return true;
    } catch (const json::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        loaded_ = false;
        return false;
    }
}

bool ConfigAnalyzer::reload() {
    loaded_ = false;
    config_.clear();
    return load();
}

bool ConfigAnalyzer::hasField(const std::string& field_path) const {
    return getField(field_path) != nullptr;
}

std::string ConfigAnalyzer::getString(const std::string& field_path, const std::string& default_value) const {
    const json* field = getField(field_path);
    if (!field) {
        return default_value;
    }

    try {
        if (field->is_string()) {
            return field->get<std::string>();
        }
        // Try to convert other types to string
        return field->dump();
    } catch (const json::exception& e) {
        return default_value;
    }
}

int ConfigAnalyzer::getInt(const std::string& field_path, int default_value) const {
    const json* field = getField(field_path);
    if (!field) {
        return default_value;
    }

    try {
        if (field->is_number_integer()) {
            return field->get<int>();
        }
        if (field->is_string()) {
            return std::stoi(field->get<std::string>());
        }
        return default_value;
    } catch (const std::exception& e) {
        return default_value;
    }
}

bool ConfigAnalyzer::getBool(const std::string& field_path, bool default_value) const {
    const json* field = getField(field_path);
    if (!field) {
        return default_value;
    }

    try {
        if (field->is_boolean()) {
            return field->get<bool>();
        }
        if (field->is_string()) {
            std::string str = field->get<std::string>();
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            return str == "true" || str == "1" || str == "yes";
        }
        return default_value;
    } catch (const json::exception& e) {
        return default_value;
    }
}

double ConfigAnalyzer::getDouble(const std::string& field_path, double default_value) const {
    const json* field = getField(field_path);
    if (!field) {
        return default_value;
    }

    try {
        if (field->is_number()) {
            return field->get<double>();
        }
        if (field->is_string()) {
            return std::stod(field->get<std::string>());
        }
        return default_value;
    } catch (const std::exception& e) {
        return default_value;
    }
}

ValidationResult ConfigAnalyzer::validate() const {
    ValidationResult result;

    if (!loaded_) {
        result.addError("Configuration not loaded");
        return result;
    }

    // Validate framework configuration
    auto frameworkResult = validateFrameworkConfig();
    result.errors.insert(result.errors.end(), frameworkResult.errors.begin(), frameworkResult.errors.end());
    result.warnings.insert(result.warnings.end(), frameworkResult.warnings.begin(), frameworkResult.warnings.end());

    // Validate module configuration
    auto moduleResult = validateModuleConfig();
    result.errors.insert(result.errors.end(), moduleResult.errors.begin(), moduleResult.errors.end());
    result.warnings.insert(result.warnings.end(), moduleResult.warnings.begin(), moduleResult.warnings.end());

    // Validate IPC configuration
    auto ipcResult = validateIpcConfig();
    result.errors.insert(result.errors.end(), ipcResult.errors.begin(), ipcResult.errors.end());
    result.warnings.insert(result.warnings.end(), ipcResult.warnings.begin(), ipcResult.warnings.end());

    // Validate logging configuration
    auto loggingResult = validateLoggingConfig();
    result.errors.insert(result.errors.end(), loggingResult.errors.begin(), loggingResult.errors.end());
    result.warnings.insert(result.warnings.end(), loggingResult.warnings.begin(), loggingResult.warnings.end());

    result.valid = result.errors.empty();
    return result;
}

ValidationResult ConfigAnalyzer::validateFrameworkConfig() const {
    ValidationResult result;

    // Check required framework fields
    if (!hasField("framework.id")) {
        result.addError("Missing required field: framework.id");
    }
    if (!hasField("framework.version")) {
        result.addError("Missing required field: framework.version");
    }

    return result;
}

ValidationResult ConfigAnalyzer::validateModuleConfig() const {
    ValidationResult result;

    if (!hasField("modules")) {
        result.addWarning("Missing modules configuration section");
        return result;
    }

    // Check module paths
    if (hasField("modules.config_path")) {
        std::string path = getString("modules.config_path");
        if (!isValidPath(path)) {
            result.addWarning("modules.config_path may be invalid: " + path);
        }
    }

    if (hasField("modules.lib_path")) {
        std::string path = getString("modules.lib_path");
        if (!isValidPath(path)) {
            result.addWarning("modules.lib_path may be invalid: " + path);
        }
    }

    return result;
}

ValidationResult ConfigAnalyzer::validateIpcConfig() const {
    ValidationResult result;

    if (!hasField("ipc")) {
        result.addWarning("Missing IPC configuration section");
        return result;
    }

    // Check IPC enabled
    if (hasField("ipc.enabled") && getBool("ipc.enabled")) {
        // Validate transport settings
        if (!hasField("ipc.default_transport")) {
            result.addError("IPC enabled but default_transport not specified");
        } else {
            std::string transport = getString("ipc.default_transport");
            if (transport != "unix-socket" && transport != "shared-memory" && transport != "grpc" && transport != "tcp") {
                result.addWarning("Unknown IPC transport: " + transport);
            }
        }
    }

    return result;
}

ValidationResult ConfigAnalyzer::validateLoggingConfig() const {
    ValidationResult result;

    if (!hasField("logging")) {
        result.addWarning("Missing logging configuration section");
        return result;
    }

    // Check log level
    if (hasField("logging.level")) {
        std::string level = getString("logging.level");
        if (level != "VERBOSE" && level != "DEBUG" && level != "INFO" &&
            level != "WARNING" && level != "ERROR" && level != "FATAL") {
            result.addWarning("Unknown log level: " + level);
        }
    }

    // Check log file path
    if (hasField("logging.file")) {
        std::string logFile = getString("logging.file");
        if (logFile.empty()) {
            result.addWarning("Log file path is empty");
        }
    }

    // Check numeric settings
    if (hasField("logging.max_backups")) {
        int backups = getInt("logging.max_backups", -1);
        if (backups < 0) {
            result.addError("logging.max_backups must be >= 0");
        }
    }

    return result;
}

std::vector<std::string> ConfigAnalyzer::getAllKeys() const {
    std::vector<std::string> keys;
    extractKeys(config_, "", keys);
    return keys;
}

std::string ConfigAnalyzer::toString() const {
    return config_.dump(2); // Pretty print with 2 spaces
}

const json* ConfigAnalyzer::getField(const std::string& field_path) const {
    if (!loaded_) {
        return nullptr;
    }

    // Split path by '.'
    std::vector<std::string> parts;
    std::stringstream ss(field_path);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    const json* current = &config_;
    for (const auto& key : parts) {
        if (!current->contains(key)) {
            return nullptr;
        }
        current = &(*current)[key];
    }

    return current;
}

void ConfigAnalyzer::extractKeys(const json& j, const std::string& prefix, std::vector<std::string>& keys) const {
    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string key = prefix.empty() ? it.key() : prefix + "." + it.key();
            keys.push_back(key);
            extractKeys(it.value(), key, keys);
        }
    }
}

bool ConfigAnalyzer::isValidPath(const std::string& path) const {
    // Basic path validation
    if (path.empty()) {
        return false;
    }

    // Check for invalid characters (basic check)
    // In a real implementation, you might use filesystem APIs
    return path.find('\0') == std::string::npos;
}

} // namespace automation
} // namespace cdmf
