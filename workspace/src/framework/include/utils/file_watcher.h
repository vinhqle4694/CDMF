#ifndef CDMF_FILE_WATCHER_H
#define CDMF_FILE_WATCHER_H

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <condition_variable>

namespace cdmf {

/**
 * @brief File change event types
 */
enum class FileEvent {
    MODIFIED,   ///< File was modified
    CREATED,    ///< File was created
    DELETED     ///< File was deleted
};

/**
 * @brief File change callback function
 *
 * @param path Path to the file that changed
 * @param event Type of change event
 */
using FileChangeCallback = std::function<void(const std::string& path, FileEvent event)>;

/**
 * @brief Watches files for changes and triggers callbacks
 *
 * Monitors files for modifications, creation, and deletion.
 * Uses polling-based approach for cross-platform compatibility.
 *
 * Usage:
 * @code
 * FileWatcher watcher(1000); // Check every 1 second
 * watcher.start();
 *
 * watcher.watch("/path/to/module.so", [](const std::string& path, FileEvent event) {
 *     if (event == FileEvent::MODIFIED) {
 *         std::cout << "Module changed: " << path << std::endl;
 *     }
 * });
 *
 * // Later...
 * watcher.stop();
 * @endcode
 */
class FileWatcher {
public:
    /**
     * @brief Construct file watcher with specified polling interval
     *
     * @param pollIntervalMs Polling interval in milliseconds (default: 1000ms)
     */
    explicit FileWatcher(int pollIntervalMs = 1000);

    /**
     * @brief Destructor - stops watching if active
     */
    ~FileWatcher();

    /**
     * @brief Start the file watcher thread
     */
    void start();

    /**
     * @brief Stop the file watcher thread
     */
    void stop();

    /**
     * @brief Watch a file for changes
     *
     * @param path Path to file to watch
     * @param callback Callback to invoke on file changes
     * @return true if watch was added successfully
     */
    bool watch(const std::string& path, FileChangeCallback callback);

    /**
     * @brief Stop watching a file
     *
     * @param path Path to file to stop watching
     */
    void unwatch(const std::string& path);

    /**
     * @brief Check if a file is being watched
     *
     * @param path Path to check
     * @return true if file is being watched
     */
    bool isWatching(const std::string& path) const;

    /**
     * @brief Get number of files being watched
     *
     * @return Number of watched files
     */
    size_t getWatchCount() const;

    /**
     * @brief Check if watcher is running
     *
     * @return true if watcher thread is active
     */
    bool isRunning() const;

private:
    /**
     * @brief File metadata for change detection
     */
    struct FileMetadata {
        std::filesystem::file_time_type lastWriteTime;
        uintmax_t fileSize;
        bool exists;
        FileChangeCallback callback;
    };

    /**
     * @brief Watcher thread main loop
     */
    void watcherLoop();

    /**
     * @brief Check a single file for changes
     *
     * @param path File path
     * @param metadata File metadata
     */
    void checkFile(const std::string& path, FileMetadata& metadata);

    int pollIntervalMs_;
    std::atomic<bool> running_;
    std::thread watcherThread_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, FileMetadata> watchedFiles_;
};

} // namespace cdmf

#endif // CDMF_FILE_WATCHER_H
