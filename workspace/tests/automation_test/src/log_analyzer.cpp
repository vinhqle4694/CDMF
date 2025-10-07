/**
 * @file log_analyzer.cpp
 * @brief Implementation of LogAnalyzer
 */

#include "log_analyzer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace cdmf {
namespace automation {

LogAnalyzer::LogAnalyzer(const std::string& log_file_path)
    : log_file_path_(log_file_path)
{
}

bool LogAnalyzer::load() {
    entries_.clear();

    std::ifstream file(log_file_path_);
    if (!file.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path_ << std::endl;
        return false;
    }

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number++;
        if (!line.empty()) {
            entries_.push_back(parseLine(line, line_number));
        }
    }

    return true;
}

bool LogAnalyzer::reload() {
    return load();
}

std::vector<LogEntry> LogAnalyzer::getEntriesByLevel(LogLevel level) const {
    std::vector<LogEntry> result;
    for (const auto& entry : entries_) {
        if (entry.level == level) {
            result.push_back(entry);
        }
    }
    return result;
}

bool LogAnalyzer::containsPattern(const std::string& pattern, bool use_regex) const {
    if (use_regex) {
        try {
            std::regex re(pattern);
            for (const auto& entry : entries_) {
                if (std::regex_search(entry.raw_line, re)) {
                    return true;
                }
            }
        } catch (const std::regex_error& e) {
            std::cerr << "Regex error: " << e.what() << std::endl;
            return false;
        }
    } else {
        for (const auto& entry : entries_) {
            if (entry.raw_line.find(pattern) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

MatchResult LogAnalyzer::findFirst(const std::string& pattern) const {
    MatchResult result;
    try {
        std::regex re(pattern);
        std::smatch match;

        for (const auto& entry : entries_) {
            if (std::regex_search(entry.raw_line, match, re)) {
                result.found = true;
                result.entry = entry;
                for (size_t i = 0; i < match.size(); i++) {
                    result.captured_groups.push_back(match[i].str());
                }
                return result;
            }
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    }
    return result;
}

std::vector<MatchResult> LogAnalyzer::findAll(const std::string& pattern) const {
    std::vector<MatchResult> results;
    try {
        std::regex re(pattern);
        std::smatch match;

        for (const auto& entry : entries_) {
            if (std::regex_search(entry.raw_line, match, re)) {
                MatchResult result;
                result.found = true;
                result.entry = entry;
                for (size_t i = 0; i < match.size(); i++) {
                    result.captured_groups.push_back(match[i].str());
                }
                results.push_back(result);
            }
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    }
    return results;
}

int LogAnalyzer::countPattern(const std::string& pattern, bool use_regex) const {
    int count = 0;

    if (use_regex) {
        try {
            std::regex re(pattern);
            for (const auto& entry : entries_) {
                if (std::regex_search(entry.raw_line, re)) {
                    count++;
                }
            }
        } catch (const std::regex_error& e) {
            std::cerr << "Regex error: " << e.what() << std::endl;
            return 0;
        }
    } else {
        for (const auto& entry : entries_) {
            if (entry.raw_line.find(pattern) != std::string::npos) {
                count++;
            }
        }
    }

    return count;
}

std::vector<std::string> LogAnalyzer::extractAll(const std::string& pattern, int group_index) const {
    std::vector<std::string> results;
    try {
        std::regex re(pattern);
        std::smatch match;

        for (const auto& entry : entries_) {
            if (std::regex_search(entry.raw_line, match, re)) {
                if (group_index >= 0 && static_cast<size_t>(group_index) < match.size()) {
                    results.push_back(match[group_index].str());
                }
            }
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    }
    return results;
}

bool LogAnalyzer::hasLogLevel(LogLevel level) const {
    for (const auto& entry : entries_) {
        if (entry.level == level) {
            return true;
        }
    }
    return false;
}

int LogAnalyzer::countLogLevel(LogLevel level) const {
    int count = 0;
    for (const auto& entry : entries_) {
        if (entry.level == level) {
            count++;
        }
    }
    return count;
}

bool LogAnalyzer::verifySequence(const std::vector<std::string>& patterns, bool use_regex) const {
    size_t current_pattern = 0;
    size_t current_entry = 0;

    while (current_pattern < patterns.size() && current_entry < entries_.size()) {
        bool found = false;

        if (use_regex) {
            try {
                std::regex re(patterns[current_pattern]);
                if (std::regex_search(entries_[current_entry].raw_line, re)) {
                    found = true;
                }
            } catch (const std::regex_error& e) {
                std::cerr << "Regex error: " << e.what() << std::endl;
                return false;
            }
        } else {
            if (entries_[current_entry].raw_line.find(patterns[current_pattern]) != std::string::npos) {
                found = true;
            }
        }

        if (found) {
            current_pattern++;
        }
        current_entry++;
    }

    return current_pattern == patterns.size();
}

std::vector<LogEntry> LogAnalyzer::getEntriesBetween(const std::string& start_pattern, const std::string& end_pattern) const {
    std::vector<LogEntry> result;
    bool in_range = false;
    std::regex start_re(start_pattern);
    std::regex end_re(end_pattern);

    try {
        for (const auto& entry : entries_) {
            if (!in_range && std::regex_search(entry.raw_line, start_re)) {
                in_range = true;
                continue;
            }

            if (in_range) {
                if (std::regex_search(entry.raw_line, end_re)) {
                    break;
                }
                result.push_back(entry);
            }
        }
    } catch (const std::regex_error& e) {
        std::cerr << "Regex error: " << e.what() << std::endl;
    }

    return result;
}

int64_t LogAnalyzer::getDurationMs(const std::string& start_pattern, const std::string& end_pattern) const {
    // This is a simplified implementation
    // In a real system, you would parse actual timestamps
    auto start = findFirst(start_pattern);
    auto end = findFirst(end_pattern);

    if (!start.found || !end.found) {
        return -1;
    }

    // Return line difference as a proxy (in real implementation, parse timestamps)
    return (end.entry.line_number - start.entry.line_number) * 10; // Assume 10ms per line
}

std::vector<LogEntry> LogAnalyzer::getLastEntries(size_t n) const {
    std::vector<LogEntry> result;
    size_t start = entries_.size() > n ? entries_.size() - n : 0;
    for (size_t i = start; i < entries_.size(); i++) {
        result.push_back(entries_[i]);
    }
    return result;
}

void LogAnalyzer::clear() {
    entries_.clear();
}

std::string LogAnalyzer::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::VERBOSE: return "VERBOSE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

LogLevel LogAnalyzer::parseLogLevel(const std::string& level_str) {
    if (level_str == "VERBOSE" || level_str == "V") return LogLevel::VERBOSE;
    if (level_str == "DEBUG" || level_str == "D") return LogLevel::DEBUG;
    if (level_str == "INFO" || level_str == "I") return LogLevel::INFO;
    if (level_str == "WARNING" || level_str == "WARN" || level_str == "W") return LogLevel::WARNING;
    if (level_str == "ERROR" || level_str == "E") return LogLevel::ERROR;
    if (level_str == "FATAL" || level_str == "F") return LogLevel::FATAL;
    return LogLevel::UNKNOWN;
}

LogEntry LogAnalyzer::parseLine(const std::string& line, int line_number) const {
    LogEntry entry;
    entry.raw_line = line;
    entry.line_number = line_number;
    entry.timestamp = extractTimestamp(line);

    // Try to extract log level from line
    // Common patterns: [INFO], INFO:, [I], etc.
    std::regex level_regex(R"(\[(VERBOSE|DEBUG|INFO|WARNING|WARN|ERROR|FATAL|V|D|I|W|E|F)\])");
    std::smatch match;
    if (std::regex_search(line, match, level_regex)) {
        entry.level = parseLogLevel(match[1].str());
        // Extract message after log level
        size_t pos = line.find(match[0].str());
        if (pos != std::string::npos) {
            entry.message = line.substr(pos + match[0].length());
            // Trim leading whitespace
            entry.message.erase(0, entry.message.find_first_not_of(" \t"));
        }
    } else {
        entry.level = LogLevel::UNKNOWN;
        entry.message = line;
    }

    return entry;
}

std::string LogAnalyzer::extractTimestamp(const std::string& line) const {
    // Try to extract timestamp from common formats
    // Format 1: [2024-10-07 15:30:45.123]
    std::regex timestamp_regex1(R"(\[(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?)\])");
    // Format 2: 2024-10-07 15:30:45.123
    std::regex timestamp_regex2(R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?))");

    std::smatch match;
    if (std::regex_search(line, match, timestamp_regex1) || std::regex_search(line, match, timestamp_regex2)) {
        return match[1].str();
    }

    return "";
}

} // namespace automation
} // namespace cdmf
