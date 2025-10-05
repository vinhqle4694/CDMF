/**
 * @file command_handler.cpp
 * @brief Implementation of command-line interface handler
 */

#include "utils/command_handler.h"
#include "core/framework.h"
#include "module/module.h"
#include "module/module_types.h"
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

void CommandHandler::registerCommands() {
    commands_["start"] = [this](const std::vector<std::string>& args) { return handleStart(args); };
    commands_["stop"] = [this](const std::vector<std::string>& args) { return handleStop(args); };
    commands_["update"] = [this](const std::vector<std::string>& args) { return handleUpdate(args); };
    commands_["list"] = [this](const std::vector<std::string>& args) { return handleList(args); };
    commands_["info"] = [this](const std::vector<std::string>& args) { return handleInfo(args); };
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
    oss << "  start <module_name>           - Start a module\n";
    oss << "  stop <module_name>            - Stop a module\n";
    oss << "  update <module_name> <path>   - Update a module to a new version\n";
    oss << "  list                          - List all modules with status\n";
    oss << "  info <module_name>            - Show detailed module information and APIs\n";
    oss << "  help                          - Show this help message\n";
    oss << "  exit                          - Exit the command interface\n";
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

CommandResult CommandHandler::handleHelp(const std::vector<std::string>& args) {
    return CommandResult(true, getHelpText());
}

CommandResult CommandHandler::handleExit(const std::vector<std::string>& args) {
    exitRequested_ = true;
    return CommandResult(true, "Exiting...");
}

} // namespace cdmf
