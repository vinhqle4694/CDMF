/**
 * @file automation_manager.cpp
 * @brief Implementation of AutomationManager
 */

#include "automation_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <thread>

namespace cdmf {
namespace automation {

AutomationManager::AutomationManager(const ProcessConfig& config)
    : config_(config)
    , pid_(-1)
    , status_(ProcessStatus::NOT_STARTED)
    , exit_code_(-1)
    , stdout_fd_(-1)
    , stderr_fd_(-1)
{
}

AutomationManager::~AutomationManager() {
    if (isRunning()) {
        stop(config_.shutdown_timeout_ms);
        if (isRunning()) {
            kill();
        }
    }

    if (stdout_fd_ >= 0) {
        close(stdout_fd_);
    }
    if (stderr_fd_ >= 0) {
        close(stderr_fd_);
    }
}

bool AutomationManager::start() {
    if (status_ != ProcessStatus::NOT_STARTED && status_ != ProcessStatus::STOPPED) {
        std::cerr << "Process already started or starting" << std::endl;
        return false;
    }

    status_ = ProcessStatus::STARTING;

    // Create log file directory if it doesn't exist
    std::string logDir = config_.log_file.substr(0, config_.log_file.find_last_of("/"));
    std::string mkdirCmd = "mkdir -p " + logDir;
    system(mkdirCmd.c_str());

    // Open log file for writing
    int log_fd = open(config_.log_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (log_fd < 0) {
        std::cerr << "Failed to open log file: " << config_.log_file << std::endl;
        status_ = ProcessStatus::CRASHED;
        return false;
    }

    // Fork process
    pid_ = fork();

    if (pid_ < 0) {
        // Fork failed
        std::cerr << "Failed to fork process: " << strerror(errno) << std::endl;
        close(log_fd);
        status_ = ProcessStatus::CRASHED;
        return false;
    }

    if (pid_ == 0) {
        // Child process

        // Change working directory
        if (!config_.working_directory.empty()) {
            if (chdir(config_.working_directory.c_str()) != 0) {
                std::cerr << "Failed to change directory to: " << config_.working_directory << std::endl;
                _exit(1);
            }
        }

        // Setup environment variables
        for (const auto& env : config_.env_vars) {
            size_t pos = env.find('=');
            if (pos != std::string::npos) {
                std::string key = env.substr(0, pos);
                std::string value = env.substr(pos + 1);
                setenv(key.c_str(), value.c_str(), 1);
            }
        }

        // Set CDMF_FRAMEWORK_CONFIG environment variable
        if (!config_.config_file.empty()) {
            setenv("CDMF_FRAMEWORK_CONFIG", config_.config_file.c_str(), 1);
        }

        // Redirect stdout and stderr to log file
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);

        // Execute CDMF
        execl(config_.executable_path.c_str(), config_.executable_path.c_str(), nullptr);

        // If execl returns, it failed
        std::cerr << "Failed to execute: " << config_.executable_path << " - " << strerror(errno) << std::endl;
        _exit(1);
    }

    // Parent process
    close(log_fd);

    start_time_ = std::chrono::steady_clock::now();

    // Wait a bit to see if process starts successfully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    updateStatus();

    if (status_ == ProcessStatus::CRASHED) {
        return false;
    }

    status_ = ProcessStatus::RUNNING;
    return true;
}

bool AutomationManager::stop(int timeout_ms) {
    if (!isRunning()) {
        return true;
    }

    if (timeout_ms < 0) {
        timeout_ms = config_.shutdown_timeout_ms;
    }

    status_ = ProcessStatus::STOPPING;

    // Send SIGTERM for graceful shutdown
    if (!sendSignal(SIGTERM)) {
        return false;
    }

    // Wait for process to exit
    auto start = std::chrono::steady_clock::now();
    while (true) {
        updateStatus();

        if (status_ == ProcessStatus::STOPPED || status_ == ProcessStatus::CRASHED) {
            return true;
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms) {
            status_ = ProcessStatus::TIMEOUT;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

bool AutomationManager::kill() {
    if (!isRunning()) {
        return true;
    }

    bool result = sendSignal(SIGKILL);

    // Wait for process to be reaped
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    updateStatus();

    return result;
}

bool AutomationManager::isRunning() const {
    return status_ == ProcessStatus::STARTING ||
           status_ == ProcessStatus::RUNNING ||
           status_ == ProcessStatus::STOPPING;
}

ProcessStatus AutomationManager::getStatus() const {
    return status_;
}

int AutomationManager::getExitCode() const {
    return exit_code_;
}

bool AutomationManager::waitForExit(int timeout_ms) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        updateStatus();

        if (status_ == ProcessStatus::STOPPED || status_ == ProcessStatus::CRASHED) {
            return true;
        }

        if (timeout_ms >= 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms) {
                return false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

std::string AutomationManager::getStdout() const {
    std::ifstream file(config_.log_file);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string AutomationManager::getStderr() const {
    // In this implementation, stdout and stderr are redirected to same file
    return getStdout();
}

void AutomationManager::cleanup() {
    if (std::remove(config_.log_file.c_str()) == 0) {
        std::cout << "Cleaned up log file: " << config_.log_file << std::endl;
    }
}

void AutomationManager::updateStatus() {
    if (pid_ <= 0) {
        return;
    }

    int status;
    pid_t result = waitpid(pid_, &status, WNOHANG);

    if (result == 0) {
        // Process still running
        if (status_ == ProcessStatus::NOT_STARTED) {
            status_ = ProcessStatus::STARTING;
        }
        return;
    }

    if (result < 0) {
        // Error checking status
        if (errno == ECHILD) {
            // Process already reaped
            status_ = ProcessStatus::STOPPED;
        }
        return;
    }

    // Process terminated
    if (WIFEXITED(status)) {
        exit_code_ = WEXITSTATUS(status);
        if (exit_code_ == 0) {
            status_ = ProcessStatus::STOPPED;
        } else {
            status_ = ProcessStatus::CRASHED;
        }
    } else if (WIFSIGNALED(status)) {
        exit_code_ = WTERMSIG(status);
        status_ = ProcessStatus::CRASHED;
    }
}

bool AutomationManager::sendSignal(int signal) {
    if (pid_ <= 0) {
        return false;
    }

    if (::kill(pid_, signal) == 0) {
        return true;
    }

    if (errno == ESRCH) {
        // Process doesn't exist
        status_ = ProcessStatus::STOPPED;
    }

    return false;
}

} // namespace automation
} // namespace cdmf
