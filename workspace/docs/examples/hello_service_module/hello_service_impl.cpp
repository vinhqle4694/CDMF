/**
 * @file hello_service_impl.cpp
 * @brief Hello Service Implementation
 */

#include "hello_service_impl.h"
#include "utils/log.h"
#include <stdexcept>

namespace cdmf {
namespace services {

HelloServiceImpl::HelloServiceImpl()
    : workStarted_(false)
    , greetingCount_(0) {
    LOGI("HelloServiceImpl created (work NOT started yet)");
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

    return greeting;
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
