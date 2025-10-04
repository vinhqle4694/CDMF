#ifndef CDMF_PROPERTIES_H
#define CDMF_PROPERTIES_H

#include <string>
#include <map>
#include <vector>
#include <any>
#include <optional>
#include <shared_mutex>

namespace cdmf {

/**
 * @brief Thread-safe key-value property container
 *
 * Properties stores key-value pairs where keys are strings and values
 * can be of any type (using std::any). Provides type-safe getters with
 * default value support.
 *
 * Thread-safety: All methods are thread-safe using read-write locks.
 */
class Properties {
public:
    /**
     * @brief Default constructor
     */
    Properties();

    /**
     * @brief Copy constructor
     */
    Properties(const Properties& other);

    /**
     * @brief Move constructor
     */
    Properties(Properties&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    Properties& operator=(const Properties& other);

    /**
     * @brief Move assignment
     */
    Properties& operator=(Properties&& other) noexcept;

    /**
     * @brief Set a property value
     * @param key Property key
     * @param value Property value (any type)
     */
    void set(const std::string& key, const std::any& value);

    /**
     * @brief Get a property value as std::any
     * @param key Property key
     * @return std::any containing the value, or empty std::any if not found
     */
    std::any get(const std::string& key) const;

    /**
     * @brief Get a string property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as string, or defaultValue if not found or wrong type
     */
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * @brief Get an integer property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as int, or defaultValue if not found or wrong type
     */
    int getInt(const std::string& key, int defaultValue = 0) const;

    /**
     * @brief Get a boolean property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as bool, or defaultValue if not found or wrong type
     */
    bool getBool(const std::string& key, bool defaultValue = false) const;

    /**
     * @brief Get a double property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as double, or defaultValue if not found or wrong type
     */
    double getDouble(const std::string& key, double defaultValue = 0.0) const;

    /**
     * @brief Get a long property
     * @param key Property key
     * @param defaultValue Default value if key not found
     * @return Property value as long, or defaultValue if not found or wrong type
     */
    long getLong(const std::string& key, long defaultValue = 0L) const;

    /**
     * @brief Check if a property exists
     * @param key Property key
     * @return true if property exists, false otherwise
     */
    bool has(const std::string& key) const;

    /**
     * @brief Remove a property
     * @param key Property key
     * @return true if property was removed, false if not found
     */
    bool remove(const std::string& key);

    /**
     * @brief Get all property keys
     * @return Vector of all keys
     */
    std::vector<std::string> keys() const;

    /**
     * @brief Get the number of properties
     * @return Number of properties
     */
    size_t size() const;

    /**
     * @brief Check if properties container is empty
     * @return true if empty, false otherwise
     */
    bool empty() const;

    /**
     * @brief Clear all properties
     */
    void clear();

    /**
     * @brief Merge another Properties object into this one
     *
     * Existing keys will be overwritten with values from other.
     *
     * @param other Properties to merge
     */
    void merge(const Properties& other);

    /**
     * @brief Get a typed property value
     * @tparam T Type to retrieve
     * @param key Property key
     * @return Optional containing the value if found and correct type, std::nullopt otherwise
     */
    template<typename T>
    std::optional<T> getAs(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = properties_.find(key);
        if (it == properties_.end()) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /**
     * @brief Equality comparison
     */
    bool operator==(const Properties& other) const;

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const Properties& other) const;

private:
    std::map<std::string, std::any> properties_;
    mutable std::shared_mutex mutex_;
};

} // namespace cdmf

#endif // CDMF_PROPERTIES_H
