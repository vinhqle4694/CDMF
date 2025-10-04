#include "configuration.h"
#include "utils/log.h"
#include <mutex>
#include <stdexcept>

namespace cdmf {
namespace services {

Configuration::Configuration(const std::string& pid)
    : pid_(pid), removed_(false) {
    if (pid.empty()) {
        LOGE("Attempted to create configuration with empty PID");
        throw std::invalid_argument("Configuration PID cannot be empty");
    }
    LOGD_FMT("Created configuration with PID: " << pid);
}

Configuration::~Configuration() = default;

std::string Configuration::getPid() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return pid_;
}

cdmf::Properties& Configuration::getProperties() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return properties_;
}

const cdmf::Properties& Configuration::getProperties() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return properties_;
}

void Configuration::update(const cdmf::Properties& properties) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (removed_) {
        LOGE_FMT("Attempted to update removed configuration: " << pid_);
        throw std::runtime_error("Cannot update removed configuration");
    }
    properties_ = properties;
    LOGD_FMT("Updated configuration: " << pid_ << " with " << properties.keys().size() << " properties");
}

void Configuration::remove() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    removed_ = true;
    properties_ = cdmf::Properties();
    LOGI_FMT("Removed configuration: " << pid_);
}

bool Configuration::isRemoved() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return removed_;
}

} // namespace services
} // namespace cdmf
