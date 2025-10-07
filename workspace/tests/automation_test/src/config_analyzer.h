/**
 * @file config_analyzer.h
 * @brief Analyzes CDMF configuration files for automation testing
 *
 * ConfigAnalyzer provides functionality to:
 * - Load and parse JSON configuration files
 * - Verify configuration values
 * - Check for missing or invalid fields
 * - Validate configuration against expected schema
 * - Cross-reference configuration with log output
 */

#ifndef CDMF_CONFIG_ANALYZER_H
#define CDMF_CONFIG_ANALYZER_H

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace cdmf {
namespace automation {

using json = nlohmann::json;

/**
 * @brief Validation result for configuration
 */
struct ValidationResult {
    bool valid;                             ///< Overall validation result
    std::vector<std::string> errors;        ///< List of validation errors
    std::vector<std::string> warnings;      ///< List of validation warnings

    ValidationResult() : valid(true) {}

    void addError(const std::string& error) {
        errors.push_back(error);
        valid = false;
    }

    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

/**
 * @brief Analyzes CDMF configuration files
 *
 * Example usage:
 * @code
 * ConfigAnalyzer analyzer("./config/framework.json");
 * analyzer.load();
 *
 * // Verify configuration fields
 * bool hasIpc = analyzer.hasField("ipc.enabled");
 * std::string logFile = analyzer.getString("logging.file", "");
 *
 * // Validate configuration
 * auto result = analyzer.validate();
 * if (!result.valid) {
 *     for (const auto& error : result.errors) {
 *         std::cerr << "Config error: " << error << std::endl;
 *     }
 * }
 *
 * // Compare with expected values
 * bool matches = analyzer.verifyValue("event.thread_pool_size", 8);
 * @endcode
 */
class ConfigAnalyzer {
public:
    /**
     * @brief Construct config analyzer
     * @param config_file_path Path to configuration file
     */
    explicit ConfigAnalyzer(const std::string& config_file_path);

    /**
     * @brief Load and parse configuration file
     * @return true if loaded successfully
     */
    bool load();

    /**
     * @brief Reload configuration file
     * @return true if reloaded successfully
     */
    bool reload();

    /**
     * @brief Check if configuration has field
     * @param field_path Field path (e.g., "ipc.enabled", "logging.level")
     * @return true if field exists
     */
    bool hasField(const std::string& field_path) const;

    /**
     * @brief Get string value from configuration
     * @param field_path Field path
     * @param default_value Default value if not found
     * @return String value or default
     */
    std::string getString(const std::string& field_path, const std::string& default_value = "") const;

    /**
     * @brief Get integer value from configuration
     * @param field_path Field path
     * @param default_value Default value if not found
     * @return Integer value or default
     */
    int getInt(const std::string& field_path, int default_value = 0) const;

    /**
     * @brief Get boolean value from configuration
     * @param field_path Field path
     * @param default_value Default value if not found
     * @return Boolean value or default
     */
    bool getBool(const std::string& field_path, bool default_value = false) const;

    /**
     * @brief Get double value from configuration
     * @param field_path Field path
     * @param default_value Default value if not found
     * @return Double value or default
     */
    double getDouble(const std::string& field_path, double default_value = 0.0) const;

    /**
     * @brief Verify field has expected value
     * @param field_path Field path
     * @param expected_value Expected value (string, int, bool, etc.)
     * @return true if value matches
     */
    template<typename T>
    bool verifyValue(const std::string& field_path, const T& expected_value) const;

    /**
     * @brief Validate configuration against expected schema
     * @return Validation result with errors and warnings
     */
    ValidationResult validate() const;

    /**
     * @brief Validate framework configuration
     * @return Validation result
     */
    ValidationResult validateFrameworkConfig() const;

    /**
     * @brief Validate module configuration
     * @return Validation result
     */
    ValidationResult validateModuleConfig() const;

    /**
     * @brief Validate IPC configuration
     * @return Validation result
     */
    ValidationResult validateIpcConfig() const;

    /**
     * @brief Validate logging configuration
     * @return Validation result
     */
    ValidationResult validateLoggingConfig() const;

    /**
     * @brief Get all configuration keys
     * @return Vector of all keys (dotted notation)
     */
    std::vector<std::string> getAllKeys() const;

    /**
     * @brief Export configuration to string (pretty JSON)
     * @return JSON string
     */
    std::string toString() const;

    /**
     * @brief Get raw JSON object
     * @return JSON object
     */
    const json& getJson() const { return config_; }

    /**
     * @brief Get configuration file path
     * @return Path to config file
     */
    std::string getConfigFilePath() const { return config_file_path_; }

    /**
     * @brief Check if configuration loaded successfully
     * @return true if loaded
     */
    bool isLoaded() const { return loaded_; }

private:
    /**
     * @brief Navigate to nested field
     * @param field_path Dotted path (e.g., "ipc.enabled")
     * @return Pointer to JSON value or nullptr
     */
    const json* getField(const std::string& field_path) const;

    /**
     * @brief Extract all keys from JSON object recursively
     * @param j JSON object
     * @param prefix Current key prefix
     * @param keys Output vector of keys
     */
    void extractKeys(const json& j, const std::string& prefix, std::vector<std::string>& keys) const;

    /**
     * @brief Check if value is valid path
     * @param path Path to check
     * @return true if path looks valid
     */
    bool isValidPath(const std::string& path) const;

private:
    std::string config_file_path_;
    json config_;
    bool loaded_;
};

// Template implementation
template<typename T>
bool ConfigAnalyzer::verifyValue(const std::string& field_path, const T& expected_value) const {
    const json* field = getField(field_path);
    if (!field) {
        return false;
    }

    try {
        T actual_value = field->get<T>();
        return actual_value == expected_value;
    } catch (const json::exception& e) {
        return false;
    }
}

} // namespace automation
} // namespace cdmf

#endif // CDMF_CONFIG_ANALYZER_H
