#ifndef CDMF_MODULE_RELOADER_H
#define CDMF_MODULE_RELOADER_H

#include "utils/file_watcher.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>

namespace cdmf {

// Forward declarations
class Framework;
class Module;

/**
 * @brief Module reload information
 */
struct ModuleReloadInfo {
    Module* module;                 ///< Module pointer
    std::string libraryPath;        ///< Library file path
    std::string manifestPath;       ///< Manifest file path (optional)
    bool autoReloadEnabled;         ///< Auto-reload enabled for this module
};

/**
 * @brief Manages automatic reloading of modules when their libraries change
 *
 * The ModuleReloader works with FileWatcher to detect changes to module
 * library files and automatically reload them when auto_reload is enabled.
 *
 * Features:
 * - Monitors module library files for changes
 * - Automatically reloads modules when library is updated
 * - Preserves module state (restarts if was active)
 * - Thread-safe reload operations
 * - Configurable via framework.modules.auto.reload property
 *
 * Usage:
 * @code
 * ModuleReloader reloader(framework);
 * reloader.setEnabled(true);
 * reloader.start();
 *
 * // Register modules for auto-reload
 * reloader.registerModule(module, "/path/to/module.so");
 *
 * // Modules will now auto-reload when library files change
 * @endcode
 */
class ModuleReloader {
public:
    /**
     * @brief Construct module reloader
     *
     * @param framework Framework instance
     * @param pollIntervalMs File polling interval in milliseconds (default: 1000ms)
     */
    explicit ModuleReloader(Framework* framework, int pollIntervalMs = 1000);

    /**
     * @brief Destructor - stops reloader if active
     */
    ~ModuleReloader();

    /**
     * @brief Start the module reloader
     */
    void start();

    /**
     * @brief Stop the module reloader
     */
    void stop();

    /**
     * @brief Enable or disable auto-reload
     *
     * @param enabled true to enable auto-reload
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if auto-reload is enabled
     *
     * @return true if auto-reload is enabled
     */
    bool isEnabled() const;

    /**
     * @brief Register a module for auto-reload
     *
     * @param module Module to monitor
     * @param libraryPath Path to module library file
     * @param manifestPath Path to module manifest file (optional)
     * @return true if registration succeeded
     */
    bool registerModule(Module* module,
                       const std::string& libraryPath,
                       const std::string& manifestPath = "");

    /**
     * @brief Unregister a module from auto-reload
     *
     * @param module Module to unregister
     */
    void unregisterModule(Module* module);

    /**
     * @brief Check if a module is registered for auto-reload
     *
     * @param module Module to check
     * @return true if module is registered
     */
    bool isRegistered(Module* module) const;

    /**
     * @brief Get number of registered modules
     *
     * @return Number of modules being monitored
     */
    size_t getRegisteredCount() const;

    /**
     * @brief Check if reloader is running
     *
     * @return true if reloader is active
     */
    bool isRunning() const;

private:
    /**
     * @brief Handle file change event
     *
     * @param path File path that changed
     * @param event Type of change event
     */
    void onFileChanged(const std::string& path, FileEvent event);

    /**
     * @brief Reload a module
     *
     * @param module Module to reload
     * @param libraryPath New library path
     */
    void reloadModule(Module* module, const std::string& libraryPath);

    Framework* framework_;
    std::unique_ptr<FileWatcher> fileWatcher_;
    std::atomic<bool> enabled_;

    mutable std::mutex mutex_;
    std::map<Module*, ModuleReloadInfo> registeredModules_;
    std::map<std::string, Module*> pathToModuleMap_;
};

} // namespace cdmf

#endif // CDMF_MODULE_RELOADER_H
