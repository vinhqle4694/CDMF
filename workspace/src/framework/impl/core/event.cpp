#include "core/event.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace cdmf {

Event::Event(const std::string& type, void* source)
    : type_(type)
    , source_(source)
    , timestamp_(std::chrono::system_clock::now())
    , properties_() {
}

Event::Event(const std::string& type, void* source, const Properties& properties)
    : type_(type)
    , source_(source)
    , timestamp_(std::chrono::system_clock::now())
    , properties_(properties) {
}

Event::Event(const Event& other)
    : type_(other.type_)
    , source_(other.source_)
    , timestamp_(other.timestamp_)
    , properties_(other.properties_) {
}

Event::Event(Event&& other) noexcept
    : type_(std::move(other.type_))
    , source_(other.source_)
    , timestamp_(other.timestamp_)
    , properties_(std::move(other.properties_)) {
}

Event& Event::operator=(const Event& other) {
    if (this != &other) {
        type_ = other.type_;
        source_ = other.source_;
        timestamp_ = other.timestamp_;
        properties_ = other.properties_;
    }
    return *this;
}

Event& Event::operator=(Event&& other) noexcept {
    if (this != &other) {
        type_ = std::move(other.type_);
        source_ = other.source_;
        timestamp_ = other.timestamp_;
        properties_ = std::move(other.properties_);
    }
    return *this;
}

void Event::setProperty(const std::string& key, const std::any& value) {
    properties_.set(key, value);
}

std::any Event::getProperty(const std::string& key) const {
    return properties_.get(key);
}

bool Event::hasProperty(const std::string& key) const {
    return properties_.has(key);
}

std::string Event::getPropertyString(const std::string& key,
                                     const std::string& defaultValue) const {
    return properties_.getString(key, defaultValue);
}

int Event::getPropertyInt(const std::string& key, int defaultValue) const {
    return properties_.getInt(key, defaultValue);
}

bool Event::getPropertyBool(const std::string& key, bool defaultValue) const {
    return properties_.getBool(key, defaultValue);
}

std::chrono::milliseconds Event::getAge() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp_);
}

bool Event::matchesType(const std::string& pattern) const {
    // Handle wildcard matching
    if (pattern == "*") {
        return true;
    }

    // Simple wildcard matching: supports "prefix.*" pattern
    size_t starPos = pattern.find('*');
    if (starPos == std::string::npos) {
        // No wildcard, exact match
        return type_ == pattern;
    }

    if (starPos == pattern.length() - 1) {
        // Pattern ends with *, check prefix
        std::string prefix = pattern.substr(0, starPos);
        return type_.substr(0, prefix.length()) == prefix;
    }

    // More complex wildcard patterns not supported in this implementation
    return false;
}

std::string Event::toString() const {
    std::ostringstream oss;

    // Format timestamp
    auto timeT = std::chrono::system_clock::to_time_t(timestamp_);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()) % 1000;

    oss << "Event{"
        << "type='" << type_ << "', "
        << "source=" << source_ << ", "
        << "timestamp=";

    // Format time
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count()
        << ", properties=[";

    // Add properties
    auto keys = properties_.keys();
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << keys[i];
    }

    oss << "]}";
    return oss.str();
}

} // namespace cdmf
