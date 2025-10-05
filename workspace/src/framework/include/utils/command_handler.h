/**
 * @file command_handler.h
 * @brief Command-line interface handler for CDMF framework
 */

#ifndef CDMF_COMMAND_HANDLER_H
#define CDMF_COMMAND_HANDLER_H

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace cdmf {

// Forward declarations
class Framework;
class Module;

/**
 * @brief Command result structure
 */
struct CommandResult {
    bool success;
    std::string message;

    CommandResult(bool s = true, const std::string& m = "")
        : success(s), message(m) {}
};

/**
 * @brief Command handler for processing terminal user input
 *
 * Provides interactive command-line interface for controlling the framework:
 * - start <module_name> - Start a module
 * - stop <module_name> - Stop a module
 * - update <module_name> <path> - Update a module
 * - list - List all running modules
 * - info <module_name> - Show detailed module information and APIs
 * - call <service> <method> [args...] - Call a service method (if registered via manifest)
 * - help - Show help text
 * - exit - Exit the command interface
 */
class CommandHandler {
public:
    /**
     * @brief Constructor
     * @param framework Framework instance to control
     */
    explicit CommandHandler(Framework* framework);

    /**
     * @brief Destructor
     */
    ~CommandHandler();

    /**
     * @brief Process a command line
     * @param commandLine Full command line string
     * @return Command result
     */
    CommandResult processCommand(const std::string& commandLine);

    /**
     * @brief Get help text for all commands
     * @return Help text
     */
    std::string getHelpText() const;

    /**
     * @brief Run interactive command loop
     *
     * Reads commands from stdin and processes them until 'exit' command.
     */
    void runInteractive();

private:
    // Command handler type
    using CommandFunc = std::function<CommandResult(const std::vector<std::string>&)>;

    // Framework instance
    Framework* framework_;

    // Command registry
    std::map<std::string, CommandFunc> commands_;

    // Exit flag for interactive mode
    bool exitRequested_;

    /**
     * @brief Register all built-in commands
     */
    void registerCommands();

    /**
     * @brief Parse command line into tokens
     * @param commandLine Command line string
     * @return Vector of tokens
     */
    std::vector<std::string> parseCommandLine(const std::string& commandLine);

    // Command handlers
    CommandResult handleStart(const std::vector<std::string>& args);
    CommandResult handleStop(const std::vector<std::string>& args);
    CommandResult handleUpdate(const std::vector<std::string>& args);
    CommandResult handleList(const std::vector<std::string>& args);
    CommandResult handleInfo(const std::vector<std::string>& args);
    CommandResult handleCall(const std::vector<std::string>& args);
    CommandResult handleHelp(const std::vector<std::string>& args);
    CommandResult handleExit(const std::vector<std::string>& args);

    /**
     * @brief Find module by symbolic name
     * @param symbolicName Module symbolic name
     * @return Pointer to module or nullptr if not found
     */
    Module* findModule(const std::string& symbolicName);

    /**
     * @brief Get all modules in ACTIVE state
     * @return Vector of active modules
     */
    std::vector<Module*> getActiveModules();
};

} // namespace cdmf

#endif // CDMF_COMMAND_HANDLER_H
