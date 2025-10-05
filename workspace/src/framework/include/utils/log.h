#ifndef UTILS_LOG_H
#define UTILS_LOG_H

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <ctime>

namespace utils {

enum class LogLevel {
    VERBOSE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// Default log level - can be overridden at compile time
// Define CDMF_LOG_LEVEL_DEFAULT to change default level
// Example: -DCDMF_LOG_LEVEL_DEFAULT=LogLevel::INFO to hide VERBOSE and DEBUG
#ifndef CDMF_LOG_LEVEL_DEFAULT
    #define CDMF_LOG_LEVEL_DEFAULT ::utils::LogLevel::VERBOSE
#endif

// Global maximum log level - logs below this level are suppressed
static LogLevel g_max_log_level = CDMF_LOG_LEVEL_DEFAULT;

// Set the maximum log level at runtime
inline void setLogLevel(LogLevel level) {
    g_max_log_level = level;
}

// Get the current maximum log level
inline LogLevel getLogLevel() {
    return g_max_log_level;
}

inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::VERBOSE: return "VERBOSE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &now_time_t);
    #else
        localtime_r(&now_time_t, &tm_buf);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return oss.str();
}

inline const char* extractFilename(const char* path) {
    const char* filename = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    return filename;
}

inline void log(LogLevel level, const char* file, int line, const std::string& message) {
    // Filter out logs below the maximum log level
    if (level < g_max_log_level) {
        return;
    }

    std::ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] "
        << "[" << logLevelToString(level) << "] "
        << "[" << extractFilename(file) << ":" << line << "] "
        << message;

    if (level == LogLevel::ERROR || level == LogLevel::FATAL) {
        std::cerr << oss.str() << std::endl;
    } else {
        std::cout << oss.str() << std::endl;
    }
}

} // namespace utils

/**
 * @brief Log Level Filtering
 *
 * Usage:
 * 1. Set log level at compile time (in CMakeLists.txt):
 *    add_definitions(-DCDMF_LOG_LEVEL_DEFAULT=::utils::LogLevel::INFO)
 *
 * 2. Set log level at runtime (in main.cpp or anywhere):
 *    utils::setLogLevel(utils::LogLevel::INFO);  // Hide VERBOSE and DEBUG
 *
 * 3. Get current log level:
 *    auto level = utils::getLogLevel();
 *
 * Log levels (from lowest to highest):
 *   VERBOSE < DEBUG < INFO < WARNING < ERROR < FATAL
 *
 * Example: Setting to INFO will show INFO, WARNING, ERROR, and FATAL only.
 */

// Log macros
#define LOGV(msg) ::utils::log(::utils::LogLevel::VERBOSE, __FILE__, __LINE__, msg)
#define LOGD(msg) ::utils::log(::utils::LogLevel::DEBUG, __FILE__, __LINE__, msg)
#define LOGI(msg) ::utils::log(::utils::LogLevel::INFO, __FILE__, __LINE__, msg)
#define LOGW(msg) ::utils::log(::utils::LogLevel::WARNING, __FILE__, __LINE__, msg)
#define LOGE(msg) ::utils::log(::utils::LogLevel::ERROR, __FILE__, __LINE__, msg)
#define LOGF(msg) ::utils::log(::utils::LogLevel::FATAL, __FILE__, __LINE__, msg)

// Formatted log macros (support for stream-style formatting)
#define LOGV_FMT(msg) { std::ostringstream _oss; _oss << msg; LOGV(_oss.str()); }
#define LOGD_FMT(msg) { std::ostringstream _oss; _oss << msg; LOGD(_oss.str()); }
#define LOGI_FMT(msg) { std::ostringstream _oss; _oss << msg; LOGI(_oss.str()); }
#define LOGW_FMT(msg) { std::ostringstream _oss; _oss << msg; LOGW(_oss.str()); }
#define LOGE_FMT(msg) { std::ostringstream _oss; _oss << msg; LOGE(_oss.str()); }
#define LOGF_FMT(msg) { std::ostringstream _oss; _oss << msg; LOGF(_oss.str()); }

#endif // UTILS_LOG_H
