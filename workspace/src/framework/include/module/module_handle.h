#ifndef CDMF_MODULE_HANDLE_H
#define CDMF_MODULE_HANDLE_H

#include "platform/dynamic_loader.h"
#include "platform/platform_abstraction.h"
#include "module/module_activator.h"
#include <string>
#include <memory>

namespace cdmf {

/**
 * @brief Module handle wrapping dynamic library
 *
 * Manages the lifetime of a module's shared library and provides
 * access to the module activator factory functions.
 *
 * Responsibilities:
 * - Load/unload module shared library
 * - Resolve activator factory functions
 * - Create/destroy module activator instances
 * - Manage library symbols
 *
 * Thread safety:
 * - Not thread-safe, caller must synchronize
 * - Typically used within Module implementation which provides locking
 */
class ModuleHandle {
public:
    /**
     * @brief Load module from shared library
     *
     * @param location Absolute path to module shared library
     * @throws std::runtime_error if library cannot be loaded
     * @throws std::runtime_error if activator functions not found
     */
    explicit ModuleHandle(const std::string& location);

    /**
     * @brief Destructor - unloads library
     */
    ~ModuleHandle();

    // Prevent copying
    ModuleHandle(const ModuleHandle&) = delete;
    ModuleHandle& operator=(const ModuleHandle&) = delete;

    // Allow moving
    ModuleHandle(ModuleHandle&& other) noexcept;
    ModuleHandle& operator=(ModuleHandle&& other) noexcept;

    /**
     * @brief Get module location
     * @return Path to shared library
     */
    const std::string& getLocation() const { return location_; }

    /**
     * @brief Check if library is loaded
     * @return true if library handle is valid
     */
    bool isLoaded() const { return handle_ != nullptr; }

    /**
     * @brief Create module activator instance
     *
     * Calls the module's createModuleActivator() function.
     *
     * @return Pointer to new activator (caller must destroy via destroyActivator)
     * @throws std::runtime_error if activator creation fails
     */
    IModuleActivator* createActivator();

    /**
     * @brief Destroy module activator instance
     *
     * Calls the module's destroyModuleActivator() function.
     *
     * @param activator Activator to destroy
     */
    void destroyActivator(IModuleActivator* activator);

    /**
     * @brief Get symbol from library
     *
     * @param symbolName Symbol name to resolve
     * @return Pointer to symbol, or nullptr if not found
     */
    void* getSymbol(const std::string& symbolName);

private:
    std::string location_;
    std::unique_ptr<platform::PlatformAbstraction> platform_;
    platform::LibraryHandle handle_;

    CreateModuleActivatorFunc createFunc_;
    DestroyModuleActivatorFunc destroyFunc_;

    /**
     * @brief Resolve activator factory functions
     * @throws std::runtime_error if functions not found
     */
    void resolveActivatorFunctions();
};

} // namespace cdmf

#endif // CDMF_MODULE_HANDLE_H
