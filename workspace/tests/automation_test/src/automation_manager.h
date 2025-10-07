/**
 * @file automation_manager.h
 * @brief Manages CDMF process lifecycle for automation testing
 *
 * AutomationManager provides functionality to:
 * - Start CDMF process with custom configuration
 * - Monitor process status
 * - Capture stdout/stderr to log file
 * - Stop process gracefully or forcefully
 * - Clean up resources after test execution
 */

#ifndef CDMF_AUTOMATION_MANAGER_H
#define CDMF_AUTOMATION_MANAGER_H

#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <sys/types.h>

namespace cdmf {
namespace automation {

/**
 * @brief Process status enumeration
 */
enum class ProcessStatus {
    NOT_STARTED,    ///< Process has not been started yet
    STARTING,       ///< Process is starting
    RUNNING,        ///< Process is running normally
    STOPPING,       ///< Process is being stopped
    STOPPED,        ///< Process stopped gracefully
    CRASHED,        ///< Process crashed or terminated abnormally
    TIMEOUT         ///< Process operation timed out
};

/**
 * @brief Configuration for CDMF process execution
 */
struct ProcessConfig {
    std::string executable_path;        ///< Path to CDMF executable
    std::string config_file;            ///< Path to framework configuration
    std::string log_file;               ///< Path to output log file
    std::string working_directory;      ///< Working directory for process
    std::vector<std::string> env_vars;  ///< Environment variables (KEY=VALUE)
    int startup_timeout_ms;             ///< Timeout for process startup
    int shutdown_timeout_ms;            ///< Timeout for graceful shutdown

    ProcessConfig()
        : executable_path("./bin/cdmf")
        , config_file("./config/framework.json")
        , log_file("./logs/cdmf.log")
        , working_directory("./build")
        , startup_timeout_ms(5000)
        , shutdown_timeout_ms(5000)
    {}
};

/**
 * @brief Manages CDMF process lifecycle for automation testing
 *
 * Example usage:
 * @code
 * ProcessConfig config;
 * config.executable_path = "./bin/cdmf";
 * config.log_file = "./logs/test_cdmf.log";
 *
 * AutomationManager manager(config);
 * bool started = manager.start();
 *
 * // Wait for framework to initialize
 * std::this_thread::sleep_for(std::chrono::seconds(2));
 *
 * bool running = manager.isRunning();
 *
 * // Stop the process
 * manager.stop();
 * @endcode
 */
class AutomationManager {
public:
    /**
     * @brief Construct automation manager with configuration
     * @param config Process configuration
     */
    explicit AutomationManager(const ProcessConfig& config);

    /**
     * @brief Destructor - ensures process cleanup
     */
    ~AutomationManager();

    // Disable copy
    AutomationManager(const AutomationManager&) = delete;
    AutomationManager& operator=(const AutomationManager&) = delete;

    /**
     * @brief Start CDMF process
     * @return true if process started successfully
     */
    bool start();

    /**
     * @brief Stop CDMF process gracefully
     * @param timeout_ms Timeout for graceful shutdown (default: config value)
     * @return true if process stopped within timeout
     */
    bool stop(int timeout_ms = -1);

    /**
     * @brief Force kill the process
     * @return true if process was killed
     */
    bool kill();

    /**
     * @brief Check if process is running
     * @return true if process is running
     */
    bool isRunning() const;

    /**
     * @brief Get current process status
     * @return Current status
     */
    ProcessStatus getStatus() const;

    /**
     * @brief Get process ID
     * @return PID of running process, -1 if not running
     */
    pid_t getPid() const { return pid_; }

    /**
     * @brief Get process exit code
     * @return Exit code if process terminated, -1 if still running
     */
    int getExitCode() const;

    /**
     * @brief Wait for process to exit
     * @param timeout_ms Maximum time to wait (-1 for infinite)
     * @return true if process exited, false on timeout
     */
    bool waitForExit(int timeout_ms = -1);

    /**
     * @brief Get log file path
     * @return Path to log file
     */
    std::string getLogFilePath() const { return config_.log_file; }

    /**
     * @brief Get stdout content captured so far
     * @return Stdout content
     */
    std::string getStdout() const;

    /**
     * @brief Get stderr content captured so far
     * @return Stderr content
     */
    std::string getStderr() const;

    /**
     * @brief Clean up log files and temporary resources
     */
    void cleanup();

private:
    /**
     * @brief Setup environment variables for child process
     */
    void setupEnvironment();

    /**
     * @brief Redirect stdout/stderr to log file
     */
    bool redirectOutput();

    /**
     * @brief Check process status (non-blocking)
     */
    void updateStatus();

    /**
     * @brief Send signal to process
     * @param signal Signal number (SIGTERM, SIGKILL, etc.)
     * @return true if signal sent successfully
     */
    bool sendSignal(int signal);

private:
    ProcessConfig config_;
    pid_t pid_;
    ProcessStatus status_;
    int exit_code_;
    int stdout_fd_;
    int stderr_fd_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace automation
} // namespace cdmf

#endif // CDMF_AUTOMATION_MANAGER_H
