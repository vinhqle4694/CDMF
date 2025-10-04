#include "utils/properties.h"
#include <algorithm>
#include <mutex>

namespace cdmf {

Properties::Properties() = default;

Properties::Properties(const Properties& other) {
    std::shared_lock<std::shared_mutex> lock(other.mutex_);
    properties_ = other.properties_;
}

Properties::Properties(Properties&& other) noexcept {
    std::unique_lock<std::shared_mutex> lock(other.mutex_);
    properties_ = std::move(other.properties_);
}

Properties& Properties::operator=(const Properties& other) {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lock1(mutex_, std::defer_lock);
        std::shared_lock<std::shared_mutex> lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        properties_ = other.properties_;
    }
    return *this;
}

Properties& Properties::operator=(Properties&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lock1(mutex_, std::defer_lock);
        std::unique_lock<std::shared_mutex> lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        properties_ = std::move(other.properties_);
    }
    return *this;
}

void Properties::set(const std::string& key, const std::any& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    properties_[key] = value;
}

std::any Properties::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = properties_.find(key);
    if (it != properties_.end()) {
        return it->second;
    }
    return std::any();
}

std::string Properties::getString(const std::string& key, const std::string& defaultValue) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = properties_.find(key);
    if (it == properties_.end()) {
        return defaultValue;
    }

    // Try std::string first
    try {
        return std::any_cast<std::string>(it->second);
    } catch (const std::bad_any_cast&) {
        // Try const char* next
        try {
            const char* cstr = std::any_cast<const char*>(it->second);
            return std::string(cstr);
        } catch (const std::bad_any_cast&) {
            // Try char* as fallback
            try {
                char* str = std::any_cast<char*>(it->second);
                return std::string(str);
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }
    }
}

int Properties::getInt(const std::string& key, int defaultValue) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = properties_.find(key);
    if (it == properties_.end()) {
        return defaultValue;
    }

    // Try int first
    try {
        return std::any_cast<int>(it->second);
    } catch (const std::bad_any_cast&) {
        // Try parsing from string
        try {
            std::string str = std::any_cast<std::string>(it->second);
            return std::stoi(str);
        } catch (...) {
            // Try parsing from const char*
            try {
                const char* cstr = std::any_cast<const char*>(it->second);
                return std::stoi(std::string(cstr));
            } catch (...) {
                return defaultValue;
            }
        }
    }
}

bool Properties::getBool(const std::string& key, bool defaultValue) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = properties_.find(key);
    if (it == properties_.end()) {
        return defaultValue;
    }

    // Try bool first
    try {
        return std::any_cast<bool>(it->second);
    } catch (const std::bad_any_cast&) {
        // Try parsing from string
        try {
            std::string str = std::any_cast<std::string>(it->second);
            return str == "true" || str == "1" || str == "yes" || str == "on";
        } catch (...) {
            // Try parsing from const char*
            try {
                const char* cstr = std::any_cast<const char*>(it->second);
                std::string str(cstr);
                return str == "true" || str == "1" || str == "yes" || str == "on";
            } catch (...) {
                return defaultValue;
            }
        }
    }
}

double Properties::getDouble(const std::string& key, double defaultValue) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = properties_.find(key);
    if (it == properties_.end()) {
        return defaultValue;
    }

    // Try double first
    try {
        return std::any_cast<double>(it->second);
    } catch (const std::bad_any_cast&) {
        // Try parsing from string
        try {
            std::string str = std::any_cast<std::string>(it->second);
            return std::stod(str);
        } catch (...) {
            // Try parsing from const char*
            try {
                const char* cstr = std::any_cast<const char*>(it->second);
                return std::stod(std::string(cstr));
            } catch (...) {
                return defaultValue;
            }
        }
    }
}

long Properties::getLong(const std::string& key, long defaultValue) const {
    auto value = getAs<long>(key);
    return value.value_or(defaultValue);
}

bool Properties::has(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return properties_.find(key) != properties_.end();
}

bool Properties::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    return properties_.erase(key) > 0;
}

std::vector<std::string> Properties::keys() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(properties_.size());
    for (const auto& pair : properties_) {
        result.push_back(pair.first);
    }
    return result;
}

size_t Properties::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return properties_.size();
}

bool Properties::empty() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return properties_.empty();
}

void Properties::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    properties_.clear();
}

void Properties::merge(const Properties& other) {
    std::unique_lock<std::shared_mutex> lock1(mutex_, std::defer_lock);
    std::shared_lock<std::shared_mutex> lock2(other.mutex_, std::defer_lock);
    std::lock(lock1, lock2);

    for (const auto& pair : other.properties_) {
        properties_[pair.first] = pair.second;
    }
}

bool Properties::operator==(const Properties& other) const {
    std::shared_lock<std::shared_mutex> lock1(mutex_, std::defer_lock);
    std::shared_lock<std::shared_mutex> lock2(other.mutex_, std::defer_lock);
    std::lock(lock1, lock2);

    if (properties_.size() != other.properties_.size()) {
        return false;
    }

    // Note: This comparison is limited because std::any doesn't support direct comparison
    // We can only compare keys, not values
    for (const auto& pair : properties_) {
        if (other.properties_.find(pair.first) == other.properties_.end()) {
            return false;
        }
    }

    return true;
}

bool Properties::operator!=(const Properties& other) const {
    return !(*this == other);
}

} // namespace cdmf
