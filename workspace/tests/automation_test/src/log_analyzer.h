/**
 * @file log_analyzer.h
 * @brief Analyzes CDMF log files for automation testing
 *
 * LogAnalyzer provides functionality to:
 * - Parse log files line by line
 * - Search for specific log patterns
 * - Verify expected log messages exist
 * - Check log severity levels
 * - Extract timestamps and measure durations
 * - Validate log sequence and ordering
 */

#ifndef CDMF_LOG_ANALYZER_H
#define CDMF_LOG_ANALYZER_H

#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <optional>

namespace cdmf {
namespace automation {

/**
 * @brief Log severity levels
 */
enum class LogLevel {
    VERBOSE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
    UNKNOWN
};

/**
 * @brief Parsed log entry
 */
struct LogEntry {
    std::string timestamp;      ///< Timestamp string
    LogLevel level;             ///< Log level
    std::string message;        ///< Log message
    std::string raw_line;       ///< Raw log line
    int line_number;            ///< Line number in file

    LogEntry() : level(LogLevel::UNKNOWN), line_number(-1) {}
};

/**
 * @brief Log pattern matching result
 */
struct MatchResult {
    bool found;                 ///< Pattern found
    LogEntry entry;             ///< Matched log entry
    std::vector<std::string> captured_groups;  ///< Regex captured groups

    MatchResult() : found(false) {}
};

/**
 * @brief Analyzes CDMF log files for automation testing
 *
 * Example usage:
 * @code
 * LogAnalyzer analyzer("./logs/cdmf.log");
 * analyzer.load();
 *
 * // Check if framework started
 * bool started = analyzer.containsPattern("Framework started successfully");
 *
 * // Verify no errors
 * bool hasErrors = analyzer.hasLogLevel(LogLevel::ERROR);
 *
 * // Count specific messages
 * int count = analyzer.countPattern("Module .* loaded");
 *
 * // Extract module names
 * auto modules = analyzer.extractAll("Module (\\w+) loaded", 1);
 * @endcode
 */
class LogAnalyzer {
public:
    /**
     * @brief Construct log analyzer
     * @param log_file_path Path to log file
     */
    explicit LogAnalyzer(const std::string& log_file_path);

    /**
     * @brief Load and parse log file
     * @return true if loaded successfully
     */
    bool load();

    /**
     * @brief Reload log file (useful for monitoring)
     * @return true if reloaded successfully
     */
    bool reload();

    /**
     * @brief Get all log entries
     * @return Vector of all parsed log entries
     */
    const std::vector<LogEntry>& getEntries() const { return entries_; }

    /**
     * @brief Get entries with specific log level
     * @param level Log level to filter
     * @return Vector of entries with specified level
     */
    std::vector<LogEntry> getEntriesByLevel(LogLevel level) const;

    /**
     * @brief Check if log contains pattern
     * @param pattern Regex pattern or plain text
     * @param use_regex true to use regex, false for plain text
     * @return true if pattern found
     */
    bool containsPattern(const std::string& pattern, bool use_regex = false) const;

    /**
     * @brief Find first occurrence of pattern
     * @param pattern Regex pattern
     * @return Match result with entry and captured groups
     */
    MatchResult findFirst(const std::string& pattern) const;

    /**
     * @brief Find all occurrences of pattern
     * @param pattern Regex pattern
     * @return Vector of match results
     */
    std::vector<MatchResult> findAll(const std::string& pattern) const;

    /**
     * @brief Count occurrences of pattern
     * @param pattern Regex pattern or plain text
     * @param use_regex true to use regex, false for plain text
     * @return Number of matches
     */
    int countPattern(const std::string& pattern, bool use_regex = false) const;

    /**
     * @brief Extract captured group from all matches
     * @param pattern Regex pattern with capture groups
     * @param group_index Index of capture group to extract (0 = full match)
     * @return Vector of extracted strings
     */
    std::vector<std::string> extractAll(const std::string& pattern, int group_index = 0) const;

    /**
     * @brief Check if log has entries with specific level
     * @param level Log level to check
     * @return true if any entries found with specified level
     */
    bool hasLogLevel(LogLevel level) const;

    /**
     * @brief Count entries with specific log level
     * @param level Log level to count
     * @return Number of entries with specified level
     */
    int countLogLevel(LogLevel level) const;

    /**
     * @brief Verify expected patterns exist in order
     * @param patterns Vector of patterns (in expected order)
     * @param use_regex true to use regex patterns
     * @return true if all patterns found in order
     */
    bool verifySequence(const std::vector<std::string>& patterns, bool use_regex = false) const;

    /**
     * @brief Get lines between two patterns
     * @param start_pattern Start pattern
     * @param end_pattern End pattern
     * @return Vector of log entries between patterns
     */
    std::vector<LogEntry> getEntriesBetween(const std::string& start_pattern, const std::string& end_pattern) const;

    /**
     * @brief Get duration between two patterns
     * @param start_pattern Start pattern with timestamp
     * @param end_pattern End pattern with timestamp
     * @return Duration in milliseconds, or -1 if not found
     */
    int64_t getDurationMs(const std::string& start_pattern, const std::string& end_pattern) const;

    /**
     * @brief Get last N log entries
     * @param n Number of entries
     * @return Vector of last N entries
     */
    std::vector<LogEntry> getLastEntries(size_t n) const;

    /**
     * @brief Get log file path
     * @return Path to log file
     */
    std::string getLogFilePath() const { return log_file_path_; }

    /**
     * @brief Get total number of log entries
     * @return Number of entries
     */
    size_t getEntryCount() const { return entries_.size(); }

    /**
     * @brief Clear all loaded entries
     */
    void clear();

    /**
     * @brief Convert log level to string
     * @param level Log level
     * @return String representation
     */
    static std::string logLevelToString(LogLevel level);

    /**
     * @brief Parse log level from string
     * @param level_str String representation
     * @return Log level
     */
    static LogLevel parseLogLevel(const std::string& level_str);

private:
    /**
     * @brief Parse single log line
     * @param line Log line
     * @param line_number Line number in file
     * @return Parsed log entry
     */
    LogEntry parseLine(const std::string& line, int line_number) const;

    /**
     * @brief Extract timestamp from log line
     * @param line Log line
     * @return Timestamp string or empty
     */
    std::string extractTimestamp(const std::string& line) const;

private:
    std::string log_file_path_;
    std::vector<LogEntry> entries_;
};

} // namespace automation
} // namespace cdmf

#endif // CDMF_LOG_ANALYZER_H
