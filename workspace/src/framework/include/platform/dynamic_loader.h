#ifndef CDMF_DYNAMIC_LOADER_H
#define CDMF_DYNAMIC_LOADER_H

#include "platform_types.h"
#include <string>
#include <stdexcept>

namespace cdmf {
namespace platform {

/**
 * @brief Exception thrown when dynamic library operations fail
 */
class DynamicLoaderException : public std::runtime_error {
public:
    explicit DynamicLoaderException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Abstract interface for platform-specific dynamic library loading
 *
 * This interface provides a uniform API for loading and managing
 * dynamic libraries across different platforms (Linux, Windows, macOS).
 *
 * Platform implementations:
 * - Linux: Uses dlopen/dlsym/dlclose
 * - Windows: Uses LoadLibrary/GetProcAddress/FreeLibrary
 * - macOS: Uses dlopen/dlsym/dlclose (similar to Linux)
 *
 * Thread-Safety:
 * - Implementations must be thread-safe for concurrent operations
 * - Multiple threads can safely load different libraries
 * - Symbol lookup on the same library must be thread-safe
 */
class IDynamicLoader {
public:
    virtual ~IDynamicLoader() = default;

    /**
     * @brief Load a dynamic library from the specified path
     *
     * Loads a shared library (.so, .dll, .dylib) into the process
     * address space and returns a handle for symbol resolution.
     *
     * @param path Absolute or relative path to the library file
     * @return LibraryHandle Opaque handle to the loaded library
     * @throws DynamicLoaderException if loading fails (file not found,
     *         missing dependencies, architecture mismatch, etc.)
     *
     * @note The library remains loaded until unload() is called
     * @note Multiple calls with the same path may return different handles
     *       or increment a reference count, depending on platform
     */
    virtual LibraryHandle load(const std::string& path) = 0;

    /**
     * @brief Unload a previously loaded dynamic library
     *
     * Decrements the reference count or unloads the library if no
     * longer referenced. After unloading, the handle becomes invalid
     * and all symbols from the library are no longer accessible.
     *
     * @param handle Valid library handle from load()
     * @throws DynamicLoaderException if handle is invalid or unload fails
     *
     * @warning Calling functions from an unloaded library results in
     *          undefined behavior (typically segmentation fault)
     * @note On some platforms, the library may not be immediately
     *       unloaded if other references exist
     */
    virtual void unload(LibraryHandle handle) = 0;

    /**
     * @brief Resolve a symbol (function or variable) from a loaded library
     *
     * Retrieves the address of a named symbol from the specified library.
     * The returned pointer can be cast to the appropriate function pointer
     * or variable type.
     *
     * @param handle Valid library handle from load()
     * @param symbolName Name of the symbol to resolve (e.g., "createModule")
     * @return void* Pointer to the symbol, or nullptr if not found
     * @throws DynamicLoaderException if handle is invalid
     *
     * @note Symbol names must match exactly (case-sensitive)
     * @note C++ symbols are typically mangled; use extern "C" for
     *       predictable symbol names
     * @note Returns nullptr (not throwing) if symbol doesn't exist,
     *       allowing optional symbol detection
     *
     * Example:
     * @code
     * LibraryHandle handle = loader->load("libmodule.so");
     * auto* createFn = reinterpret_cast<CreateModuleFn>(
     *     loader->getSymbol(handle, "createModule")
     * );
     * if (createFn) {
     *     createFn();
     * }
     * @endcode
     */
    virtual void* getSymbol(LibraryHandle handle, const std::string& symbolName) = 0;

    /**
     * @brief Get the last error message from the platform loader
     *
     * @return std::string Description of the last error, or empty if no error
     *
     * @note This is platform-specific and may be cleared after retrieval
     * @note Useful for debugging symbol resolution failures
     */
    virtual std::string getLastError() const = 0;

    /**
     * @brief Get the platform this loader supports
     *
     * @return Platform The target platform (LINUX, WINDOWS, MACOS)
     */
    virtual Platform getPlatform() const = 0;
};

} // namespace platform
} // namespace cdmf

#endif // CDMF_DYNAMIC_LOADER_H
