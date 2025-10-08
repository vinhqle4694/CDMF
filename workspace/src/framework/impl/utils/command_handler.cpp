/**
 * @file command_handler.cpp
 * @brief Implementation of command-line interface handler
 */

#include "utils/command_handler.h"
#include "core/framework.h"
#include "module/module.h"
#include "module/module_types.h"
#include "service/command_dispatcher.h"
#include "config/configuration_admin.h"
#include "config/configuration.h"
#include "utils/log.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstring>

// Include readline for command history support
#ifdef _WIN32
    // Windows doesn't have readline by default
    #define NO_READLINE
#else
    #include <readline/readline.h>
    #include <readline/history.h>
#endif

namespace cdmf {

CommandHandler::CommandHandler(Framework* framework)
    : framework_(framework)
    , exitRequested_(false) {
    registerCommands();
}

CommandHandler::~CommandHandler() {
}

void CommandHandler::requestExit() {
    exitRequested_ = true;
}

void CommandHandler::registerCommands() {
    commands_["start"] = [this](const std::vector<std::string>& args) { return handleStart(args); };
    commands_["stop"] = [this](const std::vector<std::string>& args) { return handleStop(args); };
    commands_["update"] = [this](const std::vector<std::string>& args) { return handleUpdate(args); };
    commands_["list"] = [this](const std::vector<std::string>& args) { return handleList(args); };
    commands_["info"] = [this](const std::vector<std::string>& args) { return handleInfo(args); };
    commands_["call"] = [this](const std::vector<std::string>& args) { return handleCall(args); };
    commands_["config"] = [this](const std::vector<std::string>& args) { return handleConfig(args); };
    commands_["help"] = [this](const std::vector<std::string>& args) { return handleHelp(args); };
    commands_["exit"] = [this](const std::vector<std::string>& args) { return handleExit(args); };
}

std::vector<std::string> CommandHandler::parseCommandLine(const std::string& commandLine) {
    std::vector<std::string> tokens;
    std::istringstream iss(commandLine);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

CommandResult CommandHandler::processCommand(const std::string& commandLine) {
    // Parse command line
    auto tokens = parseCommandLine(commandLine);

    if (tokens.empty()) {
        return CommandResult(true, "");
    }

    // Extract command and arguments
    std::string command = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    // Find and execute command
    auto it = commands_.find(command);
    if (it != commands_.end()) {
        try {
            return it->second(args);
        } catch (const std::exception& e) {
            return CommandResult(false, std::string("Error executing command: ") + e.what());
        }
    } else {
        return CommandResult(false, "Unknown command: " + command + ". Type 'help' for available commands.");
    }
}

std::string CommandHandler::getHelpText() const {
    std::ostringstream oss;
    oss << "Available commands:\n";
    oss << "  start <module_name>               - Start a module\n";
    oss << "  stop <module_name>                - Stop a module\n";
    oss << "  update <module_name> <path>       - Update a module to a new version\n";
    oss << "  list                              - List all modules with status\n";
    oss << "  info <module_name>                - Show detailed module information and APIs\n";
    oss << "  call <service> <method> [args...] - Call a service method\n";
    oss << "  config [list|get|set|modules]     - Manage system configurations\n";
    oss << "    config list                     - List all configurations\n";
    oss << "    config get <pid>                - Get specific configuration\n";
    oss << "    config set <pid> <key> <value>  - Set configuration property\n";
    oss << "    config modules                  - Show module configurations\n";
    oss << "  help                              - Show this help message\n";
    oss << "  exit                              - Exit the command interface\n";
    return oss.str();
}

void CommandHandler::runInteractive() {
    std::cout << "CDMF Interactive Command Interface\n";
    std::cout << "Type 'help' for available commands, 'exit' to quit.\n";

#ifndef NO_READLINE
    std::cout << "Use UP/DOWN arrow keys to navigate command history.\n\n";
#else
    std::cout << "\n";
#endif

    exitRequested_ = false;

#ifdef NO_READLINE
    // Fallback for systems without readline (e.g., Windows)
    while (!exitRequested_) {
        std::cout << "cdmf> " << std::flush;

        std::string commandLine;
        if (!std::getline(std::cin, commandLine)) {
            // EOF or error
            break;
        }

        // Trim whitespace
        commandLine.erase(0, commandLine.find_first_not_of(" \t\n\r"));
        commandLine.erase(commandLine.find_last_not_of(" \t\n\r") + 1);

        if (commandLine.empty()) {
            continue;
        }

        // Process command
        CommandResult result = processCommand(commandLine);

        if (!result.message.empty()) {
            std::cout << result.message << "\n";
        }

        if (!result.success) {
            std::cout << "[ERROR] Command failed\n";
        }
    }
#else
    // Use readline for better command line editing with history
    while (!exitRequested_) {
        char* line = readline("cdmf> ");

        if (!line) {
            // EOF or error (Ctrl+D)
            std::cout << "\n";
            break;
        }

        std::string commandLine(line);

        // Trim whitespace
        commandLine.erase(0, commandLine.find_first_not_of(" \t\n\r"));
        commandLine.erase(commandLine.find_last_not_of(" \t\n\r") + 1);

        // Add non-empty commands to history
        if (!commandLine.empty()) {
            add_history(line);

            // Process command
            CommandResult result = processCommand(commandLine);

            if (!result.message.empty()) {
                std::cout << result.message << "\n";
            }

            if (!result.success) {
                std::cout << "[ERROR] Command failed\n";
            }
        }

        free(line);
    }
#endif

    std::cout << "Exiting command interface.\n";
}

Module* CommandHandler::findModule(const std::string& symbolicName) {
    if (!framework_) {
        return nullptr;
    }

    return framework_->getModule(symbolicName);
}

std::vector<Module*> CommandHandler::getActiveModules() {
    std::vector<Module*> activeModules;

    if (!framework_) {
        return activeModules;
    }

    auto allModules = framework_->getModules();
    for (auto* module : allModules) {
        if (module && module->getState() == ModuleState::ACTIVE) {
            activeModules.push_back(module);
        }
    }

    return activeModules;
}

CommandResult CommandHandler::handleStart(const std::vector<std::string>& args) {
    if (args.empty()) {
        return CommandResult(false, "Usage: start <module_name>");
    }

    std::string moduleName = args[0];
    Module* module = findModule(moduleName);

    if (!module) {
        return CommandResult(false, "Module not found: " + moduleName);
    }

    ModuleState currentState = module->getState();

    if (currentState == ModuleState::ACTIVE) {
        return CommandResult(true, "Module '" + moduleName + "' is already running");
    }

    if (currentState != ModuleState::RESOLVED && currentState != ModuleState::INSTALLED) {
        return CommandResult(false, "Module '" + moduleName + "' cannot be started from current state: " +
                           std::string(moduleStateToString(currentState)));
    }

    try {
        module->start();
        return CommandResult(true, "Module '" + moduleName + "' started successfully");
    } catch (const std::exception& e) {
        return CommandResult(false, "Failed to start module '" + moduleName + "': " + e.what());
    }
}

CommandResult CommandHandler::handleStop(const std::vector<std::string>& args) {
    if (args.empty()) {
        return CommandResult(false, "Usage: stop <module_name>");
    }

    std::string moduleName = args[0];
    Module* module = findModule(moduleName);

    if (!module) {
        return CommandResult(false, "Module not found: " + moduleName);
    }

    ModuleState currentState = module->getState();

    if (currentState != ModuleState::ACTIVE) {
        return CommandResult(true, "Module '" + moduleName + "' is not running");
    }

    try {
        module->stop();
        return CommandResult(true, "Module '" + moduleName + "' stopped successfully");
    } catch (const std::exception& e) {
        return CommandResult(false, "Failed to stop module '" + moduleName + "': " + e.what());
    }
}

CommandResult CommandHandler::handleUpdate(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return CommandResult(false, "Usage: update <module_name> <path>");
    }

    std::string moduleName = args[0];
    std::string newPath = args[1];

    Module* module = findModule(moduleName);

    if (!module) {
        return CommandResult(false, "Module not found: " + moduleName);
    }

    try {
        module->update(newPath);
        return CommandResult(true, "Module '" + moduleName + "' updated successfully to version " +
                           module->getVersion().toString());
    } catch (const std::exception& e) {
        return CommandResult(false, "Failed to update module '" + moduleName + "': " + e.what());
    }
}

CommandResult CommandHandler::handleList(const std::vector<std::string>& args) {
    if (!framework_) {
        return CommandResult(true, "No modules installed");
    }

    auto allModules = framework_->getModules();

    if (allModules.empty()) {
        return CommandResult(true, "No modules installed");
    }

    std::ostringstream oss;
    oss << "Installed modules (" << allModules.size() << "):\n\n";

    // Count modules by state
    int activeCount = 0;
    int resolvedCount = 0;
    int installedCount = 0;
    int otherCount = 0;

    for (auto* module : allModules) {
        if (!module) continue;

        ModuleState state = module->getState();
        std::string stateTag;

        switch (state) {
            case ModuleState::ACTIVE:
                stateTag = "[RUNNING]";
                activeCount++;
                break;
            case ModuleState::RESOLVED:
                stateTag = "[STOPPED]";
                resolvedCount++;
                break;
            case ModuleState::INSTALLED:
                stateTag = "[INSTALLED]";
                installedCount++;
                break;
            case ModuleState::STARTING:
                stateTag = "[STARTING]";
                otherCount++;
                break;
            case ModuleState::STOPPING:
                stateTag = "[STOPPING]";
                otherCount++;
                break;
            case ModuleState::UNINSTALLED:
                stateTag = "[UNINSTALLED]";
                otherCount++;
                break;
            default:
                stateTag = "[UNKNOWN]";
                otherCount++;
                break;
        }

        oss << "  " << stateTag << " "
            << module->getSymbolicName()
            << " (v" << module->getVersion().toString() << ")"
            << " [ID: " << module->getModuleId() << "]";

        // Show registered services count
        auto services = module->getRegisteredServices();
        if (!services.empty()) {
            oss << " [Services: " << services.size() << "]";
        }

        oss << "\n";
    }

    // Summary
    oss << "\nSummary:\n";
    oss << "  Running:    " << activeCount << "\n";
    oss << "  Stopped:    " << resolvedCount << "\n";
    oss << "  Installed:  " << installedCount << "\n";
    if (otherCount > 0) {
        oss << "  Other:      " << otherCount << "\n";
    }
    oss << "  Total:      " << allModules.size() << "\n";

    return CommandResult(true, oss.str());
}

CommandResult CommandHandler::handleInfo(const std::vector<std::string>& args) {
    if (args.empty()) {
        return CommandResult(false, "Usage: info <module_name>");
    }

    std::string moduleName = args[0];
    Module* module = findModule(moduleName);

    if (!module) {
        return CommandResult(false, "Module not found: " + moduleName);
    }

    std::ostringstream oss;

    // Module Identity
    oss << "\n";
    oss << "================================================================================\n";
    oss << " Module Information\n";
    oss << "================================================================================\n\n";

    oss << "Symbolic Name: " << module->getSymbolicName() << "\n";
    oss << "Version:       " << module->getVersion().toString() << "\n";
    oss << "Module ID:     " << module->getModuleId() << "\n";
    oss << "State:         " << moduleStateToString(module->getState()) << "\n";
    oss << "Location:      " << module->getLocation() << "\n";

    // Manifest metadata
    try {
        const auto& manifest = module->getManifest();
        if (manifest.contains("module")) {
            const auto& modInfo = manifest["module"];
            if (modInfo.contains("name"))
                oss << "Name:          " << modInfo["name"].get<std::string>() << "\n";
            if (modInfo.contains("description"))
                oss << "Description:   " << modInfo["description"].get<std::string>() << "\n";
            if (modInfo.contains("vendor"))
                oss << "Vendor:        " << modInfo["vendor"].get<std::string>() << "\n";
            if (modInfo.contains("category"))
                oss << "Category:      " << modInfo["category"].get<std::string>() << "\n";
        }

        oss << "\n";

        // Provided APIs (Exports)
        oss << "================================================================================\n";
        oss << " Provided APIs (Exports)\n";
        oss << "================================================================================\n\n";

        if (manifest.contains("exports") && manifest["exports"].is_array() && !manifest["exports"].empty()) {
            const auto& exports = manifest["exports"];
            for (const auto& exp : exports) {
                if (exp.contains("interface")) {
                    oss << "  * " << exp["interface"].get<std::string>();
                    if (exp.contains("version")) {
                        oss << " (v" << exp["version"].get<std::string>() << ")";
                    }
                    oss << "\n";
                }
            }
        } else {
            oss << "  (No APIs exported)\n";
        }

        oss << "\n";

        // Registered Services (Runtime)
        oss << "================================================================================\n";
        oss << " Registered Services (Runtime)\n";
        oss << "================================================================================\n\n";

        auto services = module->getRegisteredServices();
        if (!services.empty()) {
            oss << "  " << services.size() << " service(s) registered\n";
            oss << "  (Use 'list' command to see detailed service information)\n";
        } else {
            oss << "  (No services registered)\n";
        }

        oss << "\n";

        // Dependencies
        oss << "================================================================================\n";
        oss << " Dependencies\n";
        oss << "================================================================================\n\n";

        if (manifest.contains("dependencies") && manifest["dependencies"].is_array() && !manifest["dependencies"].empty()) {
            const auto& deps = manifest["dependencies"];
            for (const auto& dep : deps) {
                if (dep.contains("symbolic-name")) {
                    oss << "  * " << dep["symbolic-name"].get<std::string>();
                    if (dep.contains("version-range")) {
                        oss << " " << dep["version-range"].get<std::string>();
                    }
                    if (dep.contains("optional") && dep["optional"].get<bool>()) {
                        oss << " (optional)";
                    }
                    oss << "\n";
                }
            }
        } else {
            oss << "  (No dependencies)\n";
        }

        oss << "\n";

        // Services In Use
        auto servicesInUse = module->getServicesInUse();
        if (!servicesInUse.empty()) {
            oss << "================================================================================\n";
            oss << " Services In Use\n";
            oss << "================================================================================\n\n";

            oss << "  " << servicesInUse.size() << " service(s) in use\n";
            oss << "\n";
        }

        // Module Properties
        if (manifest.contains("properties") && manifest["properties"].is_object() && !manifest["properties"].empty()) {
            oss << "================================================================================\n";
            oss << " Module Properties\n";
            oss << "================================================================================\n\n";

            const auto& props = manifest["properties"];
            for (auto it = props.begin(); it != props.end(); ++it) {
                oss << "  * " << it.key() << " = ";
                if (it.value().is_string()) {
                    oss << it.value().get<std::string>();
                } else {
                    oss << it.value().dump();
                }
                oss << "\n";
            }
            oss << "\n";
        }

        oss << "================================================================================\n";

    } catch (const std::exception& e) {
        oss << "\nError reading module manifest: " << e.what() << "\n";
    }

    return CommandResult(true, oss.str());
}

CommandResult CommandHandler::handleCall(const std::vector<std::string>& args) {
    // Parse: call <service_interface> <method_name> [args...]

    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        std::ostringstream oss;
        oss << "Usage: call <service> <method> [args...]\n\n";
        oss << "Call a service method that has been registered via module manifest.\n\n";
        oss << "Special commands:\n";
        oss << "  call --list                   - List all services with callable methods\n";
        oss << "  call <service> --help         - List methods for a service\n\n";
        oss << "Examples:\n";
        oss << "  call cdmf::IConfigurationAdmin createConfiguration com.myapp.db\n";
        oss << "  call cdmf::IConfigurationAdmin listConfigurations\n";
        oss << "  call cdmf::IConfigurationAdmin deleteConfiguration com.myapp.db\n";
        return CommandResult(true, oss.str());
    }

    if (args[0] == "--list") {
        // List all modules with CLI methods
        if (!framework_) {
            return CommandResult(false, "Framework not available");
        }

        auto modules = framework_->getModules();
        std::ostringstream oss;
        oss << "Services with callable methods:\n\n";

        int count = 0;
        for (auto* module : modules) {
            if (!module) continue;

            const auto& manifest = module->getManifest();
            if (manifest.contains("cli-methods") && manifest["cli-methods"].is_array() &&
                !manifest["cli-methods"].empty()) {

                // Group methods by service interface
                std::map<std::string, int> methodsByInterface;
                for (const auto& method : manifest["cli-methods"]) {
                    if (method.contains("interface")) {
                        std::string iface = method["interface"].get<std::string>();
                        methodsByInterface[iface]++;
                    }
                }

                for (const auto& pair : methodsByInterface) {
                    oss << "  * " << pair.first << " (" << pair.second << " method(s))\n";
                    count++;
                }
            }
        }

        if (count == 0) {
            oss << "  (No services with callable methods found)\n";
        }

        oss << "\nUse 'call <service> --help' to see methods for a service.\n";
        return CommandResult(true, oss.str());
    }

    // Parse service interface and method
    if (args.size() < 2) {
        return CommandResult(false, "Usage: call <service> <method> [args...]\n"
                                   "Use 'call --help' for more information.");
    }

    std::string serviceInterface = args[0];
    std::string methodName = args[1];

    // Handle service --help
    if (methodName == "--help" || methodName == "-h") {
        if (!framework_) {
            return CommandResult(false, "Framework not available");
        }

        auto modules = framework_->getModules();
        std::ostringstream oss;
        oss << "Available methods for " << serviceInterface << ":\n\n";

        int methodCount = 0;
        for (auto* module : modules) {
            if (!module) continue;

            const auto& manifest = module->getManifest();
            if (manifest.contains("cli-methods") && manifest["cli-methods"].is_array()) {
                for (const auto& method : manifest["cli-methods"]) {
                    if (!method.contains("interface") || !method.contains("method")) continue;

                    if (method["interface"].get<std::string>() == serviceInterface) {
                        oss << "  " << method["method"].get<std::string>();
                        if (method.contains("signature")) {
                            oss << " " << method["signature"].get<std::string>();
                        }
                        oss << "\n";
                        if (method.contains("description")) {
                            oss << "    " << method["description"].get<std::string>() << "\n";
                        }
                        methodCount++;
                    }
                }
            }
        }

        if (methodCount == 0) {
            return CommandResult(false, "Service not found or has no callable methods: " + serviceInterface);
        }

        return CommandResult(true, oss.str());
    }

    // Find the module that provides this service
    if (!framework_) {
        return CommandResult(false, "Framework not available");
    }

    auto modules = framework_->getModules();
    Module* targetModule = nullptr;

    for (auto* module : modules) {
        if (!module) continue;

        const auto& manifest = module->getManifest();
        if (manifest.contains("cli-methods") && manifest["cli-methods"].is_array()) {
            for (const auto& method : manifest["cli-methods"]) {
                if (!method.contains("interface") || !method.contains("method")) continue;

                if (method["interface"].get<std::string>() == serviceInterface &&
                    method["method"].get<std::string>() == methodName) {
                    targetModule = module;
                    break;
                }
            }
        }

        if (targetModule) break;
    }

    if (!targetModule) {
        return CommandResult(false, "Method '" + methodName + "' not found for service '" +
                                   serviceInterface + "'\nUse 'call " + serviceInterface +
                                   " --help' to see available methods.");
    }

    // Get the service instance from the module's context
    IModuleContext* context = targetModule->getContext();
    if (!context) {
        return CommandResult(false, "Module context not available");
    }

    // Get service reference for the interface
    ServiceReference serviceRef = context->getServiceReference(serviceInterface);
    if (!serviceRef.isValid()) {
        return CommandResult(false, "Service '" + serviceInterface + "' not registered by module");
    }

    // Get the actual service instance
    std::shared_ptr<void> servicePtr = context->getService(serviceRef);
    if (!servicePtr) {
        return CommandResult(false, "Failed to get service instance for '" + serviceInterface + "'");
    }

    // Cast to ICommandDispatcher and invoke
    ICommandDispatcher* dispatcher = static_cast<ICommandDispatcher*>(servicePtr.get());
    if (!dispatcher) {
        context->ungetService(serviceRef);
        return CommandResult(false, "Service does not implement ICommandDispatcher");
    }

    // Extract method arguments (skip service and method name)
    std::vector<std::string> methodArgs(args.begin() + 2, args.end());

    // Invoke the method
    CommandResult result;
    try {
        std::string output = dispatcher->dispatchCommand(methodName, methodArgs);
        result = CommandResult(true, output);
    } catch (const std::exception& e) {
        result = CommandResult(false, std::string("Method invocation failed: ") + e.what());
    }

    // Release the service
    context->ungetService(serviceRef);

    return result;
}

CommandResult CommandHandler::handleHelp(const std::vector<std::string>& args) {
    return CommandResult(true, getHelpText());
}

CommandResult CommandHandler::handleConfig(const std::vector<std::string>& args) {
    if (!framework_) {
        return CommandResult(false, "Framework not available");
    }

    auto configAdmin = framework_->getConfigurationAdmin();
    if (!configAdmin) {
        return CommandResult(false, "Configuration Admin not available");
    }

    // Default action is 'list' if no arguments provided
    std::string action = args.empty() ? "list" : args[0];

    if (action == "list") {
        // List all configurations
        auto configs = configAdmin->listConfigurations();

        if (configs.empty()) {
            return CommandResult(true, "No configurations found");
        }

        std::ostringstream oss;
        oss << "System configurations (" << configs.size() << "):\n\n";

        for (auto* config : configs) {
            if (!config) continue;

            std::string pid = config->getPid();
            std::string factoryPid = config->getFactoryPid();
            bool isFactory = config->isFactoryConfiguration();
            size_t propCount = config->size();

            oss << "  PID: " << pid << "\n";
            if (isFactory) {
                oss << "  Factory PID: " << factoryPid << "\n";
            }
            oss << "  Properties: " << propCount << "\n";

            // Display all properties
            const auto& props = config->getProperties();
            auto allKeys = props.keys();

            if (!allKeys.empty()) {
                oss << "  Values:\n";
                for (const auto& key : allKeys) {
                    // Try different types and format accordingly
                    std::string value;

                    // Try as string first
                    auto strVal = props.getAs<std::string>(key);
                    if (strVal.has_value()) {
                        value = strVal.value();
                    } else {
                        // Try as const char*
                        auto cstrVal = props.getAs<const char*>(key);
                        if (cstrVal.has_value()) {
                            value = cstrVal.value();
                        } else {
                            // Try as int
                            auto intVal = props.getAs<int>(key);
                            if (intVal.has_value()) {
                                value = std::to_string(intVal.value());
                            } else {
                                // Try as bool
                                auto boolVal = props.getAs<bool>(key);
                                if (boolVal.has_value()) {
                                    value = boolVal.value() ? "true" : "false";
                                } else {
                                    // Try as double
                                    auto doubleVal = props.getAs<double>(key);
                                    if (doubleVal.has_value()) {
                                        value = std::to_string(doubleVal.value());
                                    } else {
                                        // Try as long
                                        auto longVal = props.getAs<long>(key);
                                        if (longVal.has_value()) {
                                            value = std::to_string(longVal.value());
                                        } else {
                                            value = "<unknown type>";
                                        }
                                    }
                                }
                            }
                        }
                    }

                    oss << "    " << key << " = " << value << "\n";
                }
            }
            oss << "\n";
        }

        // Show statistics
        oss << "Total: " << configs.size() << " configuration(s)\n";
        oss << "Factory configurations: " << configAdmin->getFactoryConfigurationCount() << "\n";

        return CommandResult(true, oss.str());

    } else if (action == "get") {
        // Get a specific configuration by PID
        if (args.size() < 2) {
            return CommandResult(false, "Usage: config get <pid>");
        }

        std::string pid = args[1];
        auto* config = configAdmin->getConfiguration(pid);

        if (!config) {
            return CommandResult(false, "Configuration not found: " + pid);
        }

        std::ostringstream oss;
        oss << "Configuration: " << pid << "\n";

        if (config->isFactoryConfiguration()) {
            oss << "Factory PID: " << config->getFactoryPid() << "\n";
        }

        oss << "Properties (" << config->size() << "):\n";

        const auto& props = config->getProperties();
        auto allKeys = props.keys();

        for (const auto& key : allKeys) {
            // Try different types and format accordingly
            std::string value;

            // Try as string first
            auto strVal = props.getAs<std::string>(key);
            if (strVal.has_value()) {
                value = strVal.value();
            } else {
                // Try as const char*
                auto cstrVal = props.getAs<const char*>(key);
                if (cstrVal.has_value()) {
                    value = cstrVal.value();
                } else {
                    // Try as int
                    auto intVal = props.getAs<int>(key);
                    if (intVal.has_value()) {
                        value = std::to_string(intVal.value());
                    } else {
                        // Try as bool
                        auto boolVal = props.getAs<bool>(key);
                        if (boolVal.has_value()) {
                            value = boolVal.value() ? "true" : "false";
                        } else {
                            // Try as double
                            auto doubleVal = props.getAs<double>(key);
                            if (doubleVal.has_value()) {
                                value = std::to_string(doubleVal.value());
                            } else {
                                // Try as long
                                auto longVal = props.getAs<long>(key);
                                if (longVal.has_value()) {
                                    value = std::to_string(longVal.value());
                                } else {
                                    value = "<unknown type>";
                                }
                            }
                        }
                    }
                }
            }

            oss << "  " << key << " = " << value << "\n";
        }

        return CommandResult(true, oss.str());

    } else if (action == "set") {
        // Set a property in a configuration
        if (args.size() < 4) {
            return CommandResult(false, "Usage: config set <pid> <key> <value>");
        }

        std::string pid = args[1];
        std::string key = args[2];
        std::string value = args[3];

        // Get or create configuration
        auto* config = configAdmin->getConfiguration(pid);
        if (!config) {
            config = configAdmin->createConfiguration(pid);
        }

        // Update property
        Properties props = config->getProperties();
        props.set(key, value);
        config->update(props);

        return CommandResult(true, "Configuration updated: " + pid + " [" + key + " = " + value + "]");

    } else if (action == "modules") {
        // List configurations for all loaded modules
        if (!framework_) {
            return CommandResult(false, "Framework not available");
        }

        auto allModules = framework_->getModules();
        if (allModules.empty()) {
            return CommandResult(true, "No modules loaded");
        }

        std::ostringstream oss;
        oss << "Module configurations (" << allModules.size() << " module(s)):\n\n";

        int configuredCount = 0;
        int notConfiguredCount = 0;

        for (auto* module : allModules) {
            if (!module) continue;

            std::string symbolicName = module->getSymbolicName();
            std::string version = module->getVersion().toString();
            ModuleState state = module->getState();

            // Module PID format: "module.{symbolic-name}" or just the symbolic name
            std::string modulePid = symbolicName;

            oss << "  Module: " << symbolicName << " (v" << version << ")\n";
            oss << "  State: " << static_cast<int>(state) << " ";

            switch (state) {
                case ModuleState::ACTIVE: oss << "[RUNNING]"; break;
                case ModuleState::RESOLVED: oss << "[STOPPED]"; break;
                case ModuleState::INSTALLED: oss << "[INSTALLED]"; break;
                default: oss << "[OTHER]"; break;
            }
            oss << "\n";

            // Check if configuration exists
            auto* config = configAdmin->getConfiguration(modulePid);
            if (config && config->size() > 0) {
                oss << "  Configuration: " << modulePid << "\n";
                oss << "  Properties: " << config->size() << "\n";

                const auto& props = config->getProperties();
                auto allKeys = props.keys();

                if (!allKeys.empty()) {
                    oss << "  Values:\n";
                    for (const auto& key : allKeys) {
                        // Try different types and format accordingly
                        std::string value;

                        auto strVal = props.getAs<std::string>(key);
                        if (strVal.has_value()) {
                            value = strVal.value();
                        } else {
                            auto cstrVal = props.getAs<const char*>(key);
                            if (cstrVal.has_value()) {
                                value = cstrVal.value();
                            } else {
                                auto intVal = props.getAs<int>(key);
                                if (intVal.has_value()) {
                                    value = std::to_string(intVal.value());
                                } else {
                                    auto boolVal = props.getAs<bool>(key);
                                    if (boolVal.has_value()) {
                                        value = boolVal.value() ? "true" : "false";
                                    } else {
                                        auto doubleVal = props.getAs<double>(key);
                                        if (doubleVal.has_value()) {
                                            value = std::to_string(doubleVal.value());
                                        } else {
                                            auto longVal = props.getAs<long>(key);
                                            if (longVal.has_value()) {
                                                value = std::to_string(longVal.value());
                                            } else {
                                                value = "<unknown type>";
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        oss << "    " << key << " = " << value << "\n";
                    }
                }
                configuredCount++;
            } else {
                oss << "  Configuration: Not configured\n";
                notConfiguredCount++;
            }
            oss << "\n";
        }

        oss << "Summary:\n";
        oss << "  Configured:     " << configuredCount << "\n";
        oss << "  Not configured: " << notConfiguredCount << "\n";
        oss << "  Total modules:  " << allModules.size() << "\n";

        return CommandResult(true, oss.str());

    } else if (action == "help") {
        std::ostringstream oss;
        oss << "Configuration commands:\n";
        oss << "  config list              - List all configurations\n";
        oss << "  config get <pid>         - Get configuration details\n";
        oss << "  config set <pid> <k> <v> - Set configuration property\n";
        oss << "  config modules           - Show configurations for all loaded modules\n";
        oss << "  config help              - Show this help\n";
        return CommandResult(true, oss.str());

    } else {
        return CommandResult(false, "Unknown config action: " + action + ". Use 'config help' for usage.");
    }
}

CommandResult CommandHandler::handleExit(const std::vector<std::string>& args) {
    exitRequested_ = true;
    return CommandResult(true, "Exiting...");
}

} // namespace cdmf
