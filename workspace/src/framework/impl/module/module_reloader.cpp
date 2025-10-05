#include "module/module_reloader.h"
#include "core/framework.h"
#include "module/module.h"
#include "utils/log.h"
#include <algorithm>

namespace cdmf {

ModuleReloader::ModuleReloader(Framework* framework, int pollIntervalMs)
    : framework_(framework)
    , fileWatcher_(std::make_unique<FileWatcher>(pollIntervalMs))
    , enabled_(false) {
    // Framework can be null for testing basic functionality
    // It's only required when actually reloading modules
}

ModuleReloader::~ModuleReloader() {
    stop();
}

void ModuleReloader::start() {
    if (fileWatcher_->isRunning()) {
        LOGW("ModuleReloader already running");
        return;
    }

    fileWatcher_->start();
    LOGI("ModuleReloader started");
}

void ModuleReloader::stop() {
    if (!fileWatcher_->isRunning()) {
        return;
    }

    fileWatcher_->stop();
    LOGI("ModuleReloader stopped");
}

void ModuleReloader::setEnabled(bool enabled) {
    enabled_ = enabled;

    if (enabled) {
        LOGI("Module auto-reload enabled");
    } else {
        LOGI("Module auto-reload disabled");
    }
}

bool ModuleReloader::isEnabled() const {
    return enabled_;
}

bool ModuleReloader::registerModule(Module* module,
                                   const std::string& libraryPath,
                                   const std::string& manifestPath) {
    if (!module) {
        LOGE("ModuleReloader: module cannot be null");
        return false;
    }

    if (libraryPath.empty()) {
        LOGE("ModuleReloader: library path cannot be empty");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already registered
    if (registeredModules_.find(module) != registeredModules_.end()) {
        LOGW_FMT("ModuleReloader: module " << module->getSymbolicName() << " already registered");
        return false;
    }

    // Create reload info
    ModuleReloadInfo info;
    info.module = module;
    info.libraryPath = libraryPath;
    info.manifestPath = manifestPath;
    info.autoReloadEnabled = true;

    // Register library file with file watcher
    bool watchAdded = fileWatcher_->watch(libraryPath,
        [this](const std::string& path, FileEvent event) {
            onFileChanged(path, event);
        });

    if (!watchAdded) {
        LOGE_FMT("ModuleReloader: failed to watch " << libraryPath);
        return false;
    }

    // Also register manifest file if provided
    if (!manifestPath.empty()) {
        bool manifestWatchAdded = fileWatcher_->watch(manifestPath,
            [this](const std::string& path, FileEvent event) {
                onFileChanged(path, event);
            });

        if (!manifestWatchAdded) {
            LOGW_FMT("ModuleReloader: failed to watch manifest " << manifestPath);
            // Don't fail registration, manifest watching is optional
        } else {
            pathToModuleMap_[manifestPath] = module;
            LOGI_FMT("ModuleReloader: watching manifest " << manifestPath);
        }
    }

    // Store module info
    registeredModules_[module] = info;
    pathToModuleMap_[libraryPath] = module;

    LOGI_FMT("ModuleReloader: registered " << module->getSymbolicName()
             << " (library: " << libraryPath << ")");

    return true;
}

void ModuleReloader::unregisterModule(Module* module) {
    if (!module) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = registeredModules_.find(module);
    if (it == registeredModules_.end()) {
        return;
    }

    // Remove library file from file watcher
    fileWatcher_->unwatch(it->second.libraryPath);

    // Remove manifest file from file watcher if it was registered
    if (!it->second.manifestPath.empty()) {
        fileWatcher_->unwatch(it->second.manifestPath);
        pathToModuleMap_.erase(it->second.manifestPath);
    }

    // Remove from maps
    pathToModuleMap_.erase(it->second.libraryPath);
    registeredModules_.erase(it);

    LOGI_FMT("ModuleReloader: unregistered " << module->getSymbolicName());
}

bool ModuleReloader::isRegistered(Module* module) const {
    if (!module) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return registeredModules_.find(module) != registeredModules_.end();
}

size_t ModuleReloader::getRegisteredCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return registeredModules_.size();
}

bool ModuleReloader::isRunning() const {
    return fileWatcher_->isRunning();
}

std::string ModuleReloader::getManifestPath(Module* module) const {
    if (!module) {
        return "";
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = registeredModules_.find(module);
    if (it == registeredModules_.end()) {
        return "";
    }

    return it->second.manifestPath;
}

void ModuleReloader::onFileChanged(const std::string& path, FileEvent event) {
    // Handle MODIFIED and CREATED events (CREATED happens after rebuild)
    // DELETED is ignored as it's usually followed by CREATED during rebuild
    if (event != FileEvent::MODIFIED && event != FileEvent::CREATED) {
        return;
    }

    // Check if auto-reload is enabled
    if (!enabled_) {
        LOGI_FMT("ModuleReloader: file changed but auto-reload disabled: " << path);
        return;
    }

    // Copy module info while holding lock, then release lock before calling reloadModule
    // This prevents deadlock when updateModule calls back into getManifestPath
    Module* modulePtr = nullptr;
    std::string libraryPath;
    std::string manifestPath;
    std::string symbolicName;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find module for this path
        auto pathIt = pathToModuleMap_.find(path);
        if (pathIt == pathToModuleMap_.end()) {
            LOGW_FMT("ModuleReloader: file changed but no module found: " << path);
            return;
        }

        Module* module = pathIt->second;

        // Get module info
        auto moduleIt = registeredModules_.find(module);
        if (moduleIt == registeredModules_.end()) {
            LOGW_FMT("ModuleReloader: module info not found for " << path);
            return;
        }

        const ModuleReloadInfo& info = moduleIt->second;

        if (!info.autoReloadEnabled) {
            LOGI_FMT("ModuleReloader: auto-reload disabled for module " << module->getSymbolicName());
            return;
        }

        // Copy all needed data before releasing lock
        modulePtr = module;
        symbolicName = module->getSymbolicName();
        libraryPath = info.libraryPath;
        manifestPath = info.manifestPath;
    } // Lock released here!

    LOGI_FMT("ModuleReloader: reloading module " << symbolicName
             << " (library changed: " << path << ")");

    // Reload the module (lock is now released to prevent deadlock)
    try {
        reloadModule(modulePtr, libraryPath, manifestPath);
    } catch (const std::exception& e) {
        LOGE_FMT("ModuleReloader: failed to reload module " << symbolicName
                 << ": " << e.what());
    }
}

void ModuleReloader::reloadModule(Module* module, const std::string& libraryPath, const std::string& manifestPath) {
    if (!module || !framework_) {
        return;
    }

    // Record module state
    bool wasActive = (module->getState() == ModuleState::ACTIVE);
    std::string symbolicName = module->getSymbolicName();
    std::string version = module->getVersion().toString();

    LOGI_FMT("ModuleReloader: reloading " << symbolicName << " v" << version
             << " (was " << (wasActive ? "ACTIVE" : "INACTIVE") << ")");

    // Use framework's updateModule method (handles both library and manifest changes)
    framework_->updateModule(module, libraryPath);

    LOGI_FMT("ModuleReloader: successfully reloaded " << symbolicName
             << " (now " << (module->getState() == ModuleState::ACTIVE ? "ACTIVE" : "INACTIVE") << ")");
}

} // namespace cdmf
