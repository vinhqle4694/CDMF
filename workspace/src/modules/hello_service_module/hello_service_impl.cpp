/**
 * @file hello_service_impl.cpp
 * @brief Hello Service Implementation
 */

#include "hello_service_impl.h"
#include "utils/log.h"
#include "security/permission_manager.h"
#include "security/permission_types.h"
#include <stdexcept>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace cdmf {
namespace services {

HelloServiceImpl::HelloServiceImpl(const std::string& logFilePath, const std::string& moduleId)
    : workStarted_(false)
    , greetingCount_(0)
    , logFilePath_(logFilePath)
    , moduleId_(moduleId) {
    LOGI_FMT("HelloServiceImpl created for module: " << moduleId_ << ", log file: " << logFilePath_ << " (work NOT started yet)");
}

HelloServiceImpl::~HelloServiceImpl() {
    stopWork();
    LOGI("HelloServiceImpl destroyed");
}

void HelloServiceImpl::startWork() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (workStarted_) {
        LOGW("HelloServiceImpl: Work already started");
        return;
    }

    LOGI("HelloServiceImpl: Starting work...");

    // Initialize service (could start threads, open connections, etc.)
    workStarted_ = true;
    greetingCount_ = 0;

    LOGI("HelloServiceImpl: Work started successfully");
}

void HelloServiceImpl::stopWork() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!workStarted_) {
        return;
    }

    LOGI("HelloServiceImpl: Stopping work...");

    // Cleanup (stop threads, close connections, etc.)
    workStarted_ = false;

    LOGI_FMT("HelloServiceImpl: Work stopped (provided " << greetingCount_.load() << " greetings)");
}

std::string HelloServiceImpl::greet(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!workStarted_) {
        LOGW("HelloServiceImpl: Service not ready - work not started");
        throw std::runtime_error("Service not ready - work not started");
    }

    // Increment greeting counter
    greetingCount_++;

    std::string greeting = "Hello, " + name + "! Welcome to CDMF!";
    LOGI_FMT("HelloServiceImpl: Greeting #" << greetingCount_.load() << " - " << greeting);

    // Write to file (demonstrates FILE_WRITE permission check)
    writeGreetingToFile(greeting);

    return greeting;
}

void HelloServiceImpl::writeGreetingToFile(const std::string& greeting) {
    // Check FILE_WRITE permission using moduleId from manifest
    auto& permMgr = security::PermissionManager::getInstance();

    if (!permMgr.checkPermission(moduleId_, security::PermissionType::FILE_WRITE, logFilePath_)) {
        LOGW_FMT("HelloServiceImpl: Missing FILE_WRITE permission for " << logFilePath_);
        return;  // Silently skip if no permission
    }

    try {
        // Get current timestamp
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        std::ostringstream timestamp;
        timestamp << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");

        // Write to file
        std::ofstream logFile(logFilePath_, std::ios::app);
        if (logFile.is_open()) {
            logFile << "[" << timestamp.str() << "] " << greeting << std::endl;
            logFile.close();
            LOGI_FMT("HelloServiceImpl: Greeting logged to " << logFilePath_);
        } else {
            LOGW_FMT("HelloServiceImpl: Failed to open log file: " << logFilePath_);
        }
    } catch (const std::exception& e) {
        LOGW_FMT("HelloServiceImpl: Exception writing to file: " << e.what());
    }
}

std::string HelloServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workStarted_ ? "Running" : "Stopped";
}

int HelloServiceImpl::getGreetingCount() const {
    return greetingCount_.load();
}

} // namespace services
} // namespace cdmf
