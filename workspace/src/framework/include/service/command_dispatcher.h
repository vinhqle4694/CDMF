/**
 * @file command_dispatcher.h
 * @brief Interface for services that dispatch CLI commands
 */

#ifndef CDMF_COMMAND_DISPATCHER_H
#define CDMF_COMMAND_DISPATCHER_H

#include <string>
#include <vector>

namespace cdmf {

/**
 * @brief Interface for services that can dispatch CLI commands
 *
 * Services that want to be callable from the command-line interface
 * should implement this interface. The framework will automatically
 * route CLI commands to the service's dispatchCommand() method based
 * on the cli-methods declared in the module manifest.
 *
 * This approach allows:
 * - Declarative method registration via manifest JSON
 * - Simple implementation with a single dispatch method
 * - Framework-managed argument validation and routing
 * - No dependency on ServiceMethodRegistry in service code
 *
 * Usage:
 * @code
 * class ConfigurationAdmin : public ICommandDispatcher {
 * public:
 *     std::string dispatchCommand(const std::string& methodName,
 *                                const std::vector<std::string>& args) override {
 *         if (methodName == "createConfiguration") {
 *             if (args.size() != 1) {
 *                 return "Error: createConfiguration requires 1 argument";
 *             }
 *             createConfiguration(args[0]);
 *             return "Created configuration: " + args[0];
 *         }
 *         else if (methodName == "listConfigurations") {
 *             auto configs = listConfigurations(args.empty() ? "" : args[0]);
 *             // Format and return results...
 *         }
 *         return "Unknown method: " + methodName;
 *     }
 * };
 * @endcode
 */
class ICommandDispatcher {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ICommandDispatcher() = default;

    /**
     * @brief Dispatch a command to the service
     *
     * The framework calls this method when a user executes a CLI command
     * that targets this service. The implementation should:
     * 1. Validate the method name
     * 2. Validate argument count and types
     * 3. Invoke the appropriate service method
     * 4. Format and return the result as a string
     * 5. Handle exceptions and return error messages
     *
     * @param methodName Name of the method to invoke (e.g., "createConfiguration")
     * @param args Vector of string arguments passed from CLI
     * @return Result message to display to user (success or error message)
     */
    virtual std::string dispatchCommand(const std::string& methodName,
                                       const std::vector<std::string>& args) = 0;
};

} // namespace cdmf

#endif // CDMF_COMMAND_DISPATCHER_H
