/**
 * @file main.cpp
 * @brief Main entry point for CDMF (Component-based Distributed Modular Framework)
 *
 * This file initializes and starts the CDMF framework with the following components:
 * - Framework Core (Platform Abstraction, Event Dispatcher)
 * - Module Management Layer (Module Registry, Loader, Dependency Resolver)
 * - Service Layer (Service Registry, Service Tracker)
 * - Framework Services (Configuration, Logging, Security, Event Management)
 * - IPC Infrastructure (Transport Manager, Serialization, Proxy/Stub)
 * - Security Subsystem (Code Verifier, Sandbox, Permission Manager)
 */

#include <memory>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
#endif

#include "core/framework.h"
#include "core/framework_properties.h"
#include "module/module.h"
#include "utils/properties.h"
#include "utils/log.h"
#include "utils/command_handler.h"
#include <nlohmann/json.hpp>

// Global framework instance
std::unique_ptr<cdmf::Framework> g_framework;
std::atomic<bool> g_running{true};
cdmf::CommandHandler* g_commandHandler = nullptr;

/**
 * Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    LOGW_FMT("Received signal " << signal << ", initiating graceful shutdown...");
    g_running = false;

    // Request command handler to exit (breaks out of stdin blocking)
    if (g_commandHandler) {
        g_commandHandler->requestExit();
        // Print newline to help readline exit cleanly
        std::cout << "\n";
    }

    if (g_framework) {
        g_framework->stop(5000); // 5 second timeout
    }
}

/**
 * Load framework properties from JSON configuration file
 */
cdmf::FrameworkProperties loadFrameworkConfig(const std::string& configPath) {
    cdmf::FrameworkProperties props;

    try {
        // Read JSON configuration file
        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            LOGW_FMT("Configuration file not found: " << configPath << ", using defaults");
            return props;
        }

        nlohmann::json config;
        configFile >> config;

        // Load framework settings
        if (config.contains("framework")) {
            auto& fw = config["framework"];
            if (fw.contains("id")) props.set("framework.id", fw["id"].get<std::string>());
            if (fw.contains("version")) props.set("framework.version", fw["version"].get<std::string>());
            if (fw.contains("vendor")) props.set("framework.vendor", fw["vendor"].get<std::string>());
        }

        // Load module settings
        if (config.contains("modules")) {
            auto& modules = config["modules"];
            if (modules.contains("config_path")) props.set("framework.module.config.path", modules["config_path"].get<std::string>());
            if (modules.contains("lib_path")) props.set("framework.module.lib.path", modules["lib_path"].get<std::string>());
            if (modules.contains("auto_install_path")) props.set("framework.auto.install.path", modules["auto_install_path"].get<std::string>());
        }

        // Load security settings
        if (config.contains("security")) {
            auto& security = config["security"];
            if (security.contains("enabled")) props.set("framework.security.enabled", security["enabled"].get<bool>() ? "true" : "false");
            if (security.contains("verify_signatures")) props.set("framework.security.verify.signatures", security["verify_signatures"].get<bool>() ? "true" : "false");
            if (security.contains("sandbox_enabled")) props.set("framework.security.sandbox.enabled", security["sandbox_enabled"].get<bool>() ? "true" : "false");
            if (security.contains("trust_store")) props.set("framework.trust.store", security["trust_store"].get<std::string>());
        }

        // Load IPC settings
        if (config.contains("ipc")) {
            auto& ipc = config["ipc"];
            if (ipc.contains("enabled")) props.set("framework.ipc.enabled", ipc["enabled"].get<bool>() ? "true" : "false");
            if (ipc.contains("default_transport")) props.set("framework.ipc.default.transport", ipc["default_transport"].get<std::string>());
            if (ipc.contains("socket_path")) props.set("framework.ipc.socket.path", ipc["socket_path"].get<std::string>());
            if (ipc.contains("shm_path")) props.set("framework.ipc.shm.path", ipc["shm_path"].get<std::string>());
        }

        // Load service layer settings
        if (config.contains("service")) {
            auto& service = config["service"];
            if (service.contains("cache_size")) props.set("framework.service.cache.size", std::to_string(service["cache_size"].get<int>()));
            if (service.contains("ranking_enabled")) props.set("framework.service.ranking.enabled", service["ranking_enabled"].get<bool>() ? "true" : "false");
        }

        // Load event system settings
        if (config.contains("event")) {
            auto& event = config["event"];
            if (event.contains("thread_pool_size")) props.set("framework.event.thread.pool.size", std::to_string(event["thread_pool_size"].get<int>()));
            if (event.contains("queue_size")) props.set("framework.event.queue.size", std::to_string(event["queue_size"].get<int>()));
            if (event.contains("async_delivery")) props.set("framework.event.async.delivery", event["async_delivery"].get<bool>() ? "true" : "false");
        }

        // Load resource limits
        if (config.contains("resource")) {
            auto& resource = config["resource"];
            if (resource.contains("module_max_memory")) props.set("framework.resource.module.max.memory", resource["module_max_memory"].get<std::string>());
            if (resource.contains("module_max_cpu")) props.set("framework.resource.module.max.cpu", resource["module_max_cpu"].get<std::string>());
        }

        // Load logging settings
        if (config.contains("logging")) {
            auto& logging = config["logging"];
            if (logging.contains("level")) props.set("framework.log.level", logging["level"].get<std::string>());
            if (logging.contains("file")) props.set("framework.log.file", logging["file"].get<std::string>());
            if (logging.contains("max_size")) props.set("framework.log.max.size", logging["max_size"].get<std::string>());
            if (logging.contains("max_backups")) props.set("framework.log.max.backups", std::to_string(logging["max_backups"].get<int>()));
            if (logging.contains("console_enabled")) props.set("framework.log.console.enabled", logging["console_enabled"].get<bool>() ? "true" : "false");
            if (logging.contains("syslog_enabled")) props.set("framework.log.syslog.enabled", logging["syslog_enabled"].get<bool>() ? "true" : "false");
        }

        // Load module configuration settings (storage_dir and auto_reload)
        if (config.contains("modules")) {
            auto& modules = config["modules"];
            if (modules.contains("storage_dir")) props.set("framework.modules.storage.dir", modules["storage_dir"].get<std::string>());
            if (modules.contains("auto_reload")) props.set("framework.modules.auto.reload", modules["auto_reload"].get<bool>() ? "true" : "false");
        }

        LOGI_FMT("Loaded framework configuration from: " << configPath);

    } catch (const std::exception& e) {
        LOGE_FMT("Failed to load configuration file: " << e.what() << ", using defaults");
    }

    return props;
}

/**
 * Setup framework properties from configuration file
 */
cdmf::FrameworkProperties setupFrameworkProperties() {
    // Try to load from config file, fall back to defaults if not found
    const char* configPath = std::getenv("CDMF_CONFIG");
    std::string configFile = configPath ? configPath : "./config/framework.json";

    return loadFrameworkConfig(configFile);
}

/**
 * Load and start framework services
 */
bool loadFrameworkServices(cdmf::Framework* framework) {
    auto context = framework->getContext();
    if (!context) {
        LOGE("Failed to get framework context");
        return false;
    }

    LOGI("Framework services will be auto-loaded from framework initialization");
    return true;
}

/**
 * Scan config/modules directory for module configuration files
 */
std::vector<std::string> scanForModuleManifests(const std::string& directory) {
    std::vector<std::string> manifests;

#ifdef _WIN32
    std::string searchPath = directory + "\\*.json";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string filename = findData.cFileName;
                // Skip framework.json
                if (filename != "framework.json") {
                    manifests.push_back(directory + "\\" + filename);
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(directory.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;

            // Skip . and ..
            if (filename == "." || filename == "..") {
                continue;
            }

            std::string fullPath = directory + "/" + filename;

            // Only process .json files (skip framework.json)
            if (filename.find(".json") != std::string::npos && filename != "framework.json") {
                std::ifstream manifestFile(fullPath);
                if (manifestFile.good()) {
                    manifests.push_back(fullPath);
                    LOGI_FMT("  Found module config: " << fullPath);
                }
            }
        }
        closedir(dir);
    }
#endif

    return manifests;
}

/**
 * Extract module library path from config filename
 */
std::string getModuleLibraryPath(const std::string& configPath) {
    // config: ./config/modules/config_service_module.json
    // library:  ./lib/config_service_module.so (Linux)
    //       or  ./lib/config_service_module.dll (Windows)

    // Extract module name from config filename
    size_t lastSlash = configPath.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? configPath.substr(lastSlash + 1) : configPath;

    // Remove .json extension
    std::string moduleName = filename.substr(0, filename.find(".json"));

    // Build library path
    std::string libPath = "./lib/" + moduleName;

#ifdef _WIN32
    return libPath + ".dll";
#elif __APPLE__
    return libPath + ".dylib";
#else
    return libPath + ".so";
#endif
}

/**
 * Install and start application modules
 */
bool installApplicationModules(cdmf::Framework* framework) {
    auto context = framework->getContext();
    if (!context) {
        LOGE("Failed to get framework context");
        return false;
    }

    LOGI("Scanning for application modules...");

    // Get module config directory from properties
    const auto& props = framework->getProperties();
    std::string configBasePath = props.getString("framework.module.config.path", "./config");
    std::string libBasePath = props.getString("framework.module.lib.path", "./lib");
    std::string configDir = configBasePath + "/modules";

    LOGI_FMT("Module config directory: " << configDir);
    LOGI_FMT("Module library directory: " << libBasePath);
    auto manifests = scanForModuleManifests(configDir);
    LOGI_FMT("Found " << manifests.size() << " module config(s)");

    int successCount = 0;
    int failCount = 0;

    // Install each module
    for (const auto& configPath : manifests) {
        try {
            LOGI_FMT("  - Processing config: " << configPath);

            // Read config to get library path
            std::string libraryPath;
            std::ifstream configFile(configPath);
            if (configFile.is_open()) {
                nlohmann::json configJson;
                configFile >> configJson;

                // Get library from config, or derive from config filename
                if (configJson.contains("module") && configJson["module"].contains("library")) {
                    std::string libName = configJson["module"]["library"].get<std::string>();

                    // If library is just a filename (no path), prepend lib base path
                    if (libName.find('/') == std::string::npos && libName.find('\\') == std::string::npos) {
                        libraryPath = libBasePath + "/" + libName;
                    } else {
                        libraryPath = libName;
                    }
                    LOGI_FMT("    Library (from config): " << libraryPath);
                } else {
                    // Fallback to convention-based approach (use lib base path)
                    size_t lastSlash = configPath.find_last_of("/\\");
                    std::string filename = (lastSlash != std::string::npos) ? configPath.substr(lastSlash + 1) : configPath;
                    std::string moduleName = filename.substr(0, filename.find(".json"));

#ifdef _WIN32
                    libraryPath = libBasePath + "/" + moduleName + ".dll";
#elif __APPLE__
                    libraryPath = libBasePath + "/" + moduleName + ".dylib";
#else
                    libraryPath = libBasePath + "/" + moduleName + ".so";
#endif
                    LOGI_FMT("    Library (derived): " << libraryPath);
                }
            } else {
                // Fallback to convention-based approach
                size_t lastSlash = configPath.find_last_of("/\\");
                std::string filename = (lastSlash != std::string::npos) ? configPath.substr(lastSlash + 1) : configPath;
                std::string moduleName = filename.substr(0, filename.find(".json"));

#ifdef _WIN32
                libraryPath = libBasePath + "/" + moduleName + ".dll";
#elif __APPLE__
                libraryPath = libBasePath + "/" + moduleName + ".dylib";
#else
                libraryPath = libBasePath + "/" + moduleName + ".so";
#endif
                LOGI_FMT("    Library (derived): " << libraryPath);
            }

            // Install the module with explicit config/manifest path
            auto module = framework->installModule(libraryPath, configPath);
            if (module) {
                LOGI_FMT("    Module installed: " << module->getSymbolicName()
                         << " v" << module->getVersion().toString());

                // Start the module
                module->start();
                LOGI_FMT("    Module started: " << module->getSymbolicName());
                successCount++;
            } else {
                LOGW("    Failed to install module");
                failCount++;
            }
        } catch (const std::exception& e) {
            LOGE_FMT("    Error loading module: " << e.what());
            failCount++;
        }
    }

    LOGI_FMT("Module installation complete: " << successCount << " succeeded, " << failCount << " failed");

    return failCount == 0;
}

/**
 * Print framework status
 */
void printFrameworkStatus(cdmf::Framework* framework) {
    LOGI("========================================");
    LOGI("CDMF Framework Status");
    LOGI("========================================");
    LOGI_FMT("State: " << static_cast<int>(framework->getState()));

    // Only access modules if framework is active
    if (framework->getState() == cdmf::FrameworkState::ACTIVE) {
        auto modules = framework->getModules();
        LOGI_FMT("Loaded Modules: " << modules.size());

        for (const auto& module : modules) {
            LOGI_FMT("  - " << module->getSymbolicName()
                      << " v" << module->getVersion().toString()
                      << " [" << static_cast<int>(module->getState()) << "]");
        }
    } else {
        LOGI("Framework not active - module information unavailable");
    }

    LOGI("========================================");
}

/**
 * Main entry point
 */
int main(int argc, char* argv[]) {
    // Set log level to hide verbose logs (uncomment to enable)
    // Options: VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL
    utils::setLogLevel(utils::LogLevel::DEBUG);  // Hide VERBOSE and DEBUG logs

    LOGI("========================================");
    LOGI("CDMF - C++ Dynamic Module Framework");
    LOGI("       Version 1.0.0");
    LOGI("========================================");

    // Install signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        // Setup framework properties
        LOGI("Setting up framework properties...");
        auto properties = setupFrameworkProperties();

        // Create framework instance
        LOGI("Creating framework instance...");
        g_framework = cdmf::createFramework(properties);

        if (!g_framework) {
            LOGE("Failed to create framework instance");
            return 1;
        }

        // Initialize framework
        LOGI("Initializing framework...");
        g_framework->init();

        // Start framework
        LOGI("Starting framework...");
        g_framework->start();

        LOGI("Framework started successfully");

        // Load framework services
        LOGI("Loading framework services...");
        if (!loadFrameworkServices(g_framework.get())) {
            LOGE("Failed to load framework services");
            g_framework->stop(5000);
            return 1;
        }

        // Install application modules
        LOGI("Installing application modules...");
        if (!installApplicationModules(g_framework.get())) {
            LOGW("Warning: Failed to install some application modules");
            // Continue execution
        }

        // Print framework status
        printFrameworkStatus(g_framework.get());

        LOGI("Framework is running. Starting interactive command interface...\n");

        // Create command handler and run interactive mode
        cdmf::CommandHandler commandHandler(g_framework.get());
        g_commandHandler = &commandHandler; // Set global pointer for signal handler

        // Run in separate thread to allow Ctrl+C handling
        std::thread commandThread([&commandHandler]() {
            commandHandler.runInteractive();
            g_running = false;
        });

        // Main loop - wait for shutdown signal or command exit
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Check framework state
            if (g_framework->getState() == cdmf::FrameworkState::STOPPED) {
                LOGW("Framework stopped unexpectedly");
                break;
            }
        }

        // Wait for command thread to finish (with timeout)
        if (commandThread.joinable()) {
            // Give readline 2 seconds to finish, then detach
            auto start = std::chrono::steady_clock::now();
            while (commandThread.joinable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto elapsed = std::chrono::steady_clock::now() - start;
                if (elapsed > std::chrono::seconds(2)) {
                    LOGW("Command thread did not exit cleanly, detaching...");
                    commandThread.detach();
                    break;
                }

                // Check if thread finished
                if (!g_running && !commandThread.joinable()) {
                    break;
                }
            }

            // Try to join if still joinable
            if (commandThread.joinable()) {
                try {
                    commandThread.join();
                } catch (...) {
                    // Ignore join errors
                }
            }
        }

        // Clear global pointer
        g_commandHandler = nullptr;

        // Graceful shutdown
        LOGI("Stopping framework...");
        g_framework->stop(5000); // 5 second timeout

        LOGI("Waiting for framework shutdown...");
        g_framework->waitForStop();

        // Final status
        printFrameworkStatus(g_framework.get());

        LOGI("Framework shutdown complete");

    } catch (const std::exception& e) {
        LOGF_FMT("Fatal error: " << e.what());

        if (g_framework) {
            g_framework->stop(5000);
        }

        return 1;
    }

    return 0;
}
