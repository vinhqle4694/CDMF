#include "utils/file_watcher.h"
#include "utils/log.h"
#include <algorithm>

namespace cdmf {

FileWatcher::FileWatcher(int pollIntervalMs)
    : pollIntervalMs_(pollIntervalMs)
    , running_(false) {
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start() {
    if (running_) {
        LOGW("FileWatcher already running");
        return;
    }

    running_ = true;
    watcherThread_ = std::thread(&FileWatcher::watcherLoop, this);

    LOGI_FMT("FileWatcher started (poll interval: " << pollIntervalMs_ << "ms)");
}

void FileWatcher::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }

    LOGI("FileWatcher stopped");
}

bool FileWatcher::watch(const std::string& path, FileChangeCallback callback) {
    if (!callback) {
        LOGE("FileWatcher: callback cannot be null");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already watching
    if (watchedFiles_.find(path) != watchedFiles_.end()) {
        LOGW_FMT("FileWatcher: already watching " << path);
        return false;
    }

    // Initialize file metadata
    FileMetadata metadata;
    metadata.callback = callback;
    metadata.exists = false;

    try {
        std::filesystem::path filePath(path);

        if (std::filesystem::exists(filePath)) {
            metadata.exists = true;
            metadata.lastWriteTime = std::filesystem::last_write_time(filePath);
            metadata.fileSize = std::filesystem::file_size(filePath);
        }

        watchedFiles_[path] = metadata;

        LOGI_FMT("FileWatcher: watching " << path);
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        LOGE_FMT("FileWatcher: failed to watch " << path << ": " << e.what());
        return false;
    }
}

void FileWatcher::unwatch(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = watchedFiles_.find(path);
    if (it != watchedFiles_.end()) {
        watchedFiles_.erase(it);
        LOGI_FMT("FileWatcher: stopped watching " << path);
    }
}

bool FileWatcher::isWatching(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return watchedFiles_.find(path) != watchedFiles_.end();
}

size_t FileWatcher::getWatchCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return watchedFiles_.size();
}

bool FileWatcher::isRunning() const {
    return running_;
}

void FileWatcher::watcherLoop() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Check each watched file
            for (auto& entry : watchedFiles_) {
                checkFile(entry.first, entry.second);
            }
        }

        // Sleep for poll interval
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs_));
    }
}

void FileWatcher::checkFile(const std::string& path, FileMetadata& metadata) {
    try {
        std::filesystem::path filePath(path);
        bool exists = std::filesystem::exists(filePath);

        // File deleted
        if (metadata.exists && !exists) {
            LOGI_FMT("FileWatcher: file deleted: " << path);
            metadata.exists = false;

            if (metadata.callback) {
                try {
                    metadata.callback(path, FileEvent::DELETED);
                } catch (const std::exception& e) {
                    LOGE_FMT("FileWatcher: callback exception for " << path << ": " << e.what());
                }
            }
            return;
        }

        // File created
        if (!metadata.exists && exists) {
            LOGI_FMT("FileWatcher: file created: " << path);
            metadata.exists = true;
            metadata.lastWriteTime = std::filesystem::last_write_time(filePath);
            metadata.fileSize = std::filesystem::file_size(filePath);

            if (metadata.callback) {
                try {
                    metadata.callback(path, FileEvent::CREATED);
                } catch (const std::exception& e) {
                    LOGE_FMT("FileWatcher: callback exception for " << path << ": " << e.what());
                }
            }
            return;
        }

        // File modified
        if (exists) {
            auto lastWriteTime = std::filesystem::last_write_time(filePath);
            auto fileSize = std::filesystem::file_size(filePath);

            if (lastWriteTime != metadata.lastWriteTime || fileSize != metadata.fileSize) {
                LOGI_FMT("FileWatcher: file modified: " << path);
                metadata.lastWriteTime = lastWriteTime;
                metadata.fileSize = fileSize;

                if (metadata.callback) {
                    try {
                        metadata.callback(path, FileEvent::MODIFIED);
                    } catch (const std::exception& e) {
                        LOGE_FMT("FileWatcher: callback exception for " << path << ": " << e.what());
                    }
                }
            }
        }

    } catch (const std::filesystem::filesystem_error& e) {
        LOGE_FMT("FileWatcher: error checking " << path << ": " << e.what());
    }
}

} // namespace cdmf
