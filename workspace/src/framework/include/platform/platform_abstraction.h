#ifndef CDMF_PLATFORM_ABSTRACTION_H
#define CDMF_PLATFORM_ABSTRACTION_H

#include "platform_types.h"
#include "dynamic_loader.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>

namespace cdmf {
namespace platform {

/**
 * @brief Unified platform abstraction layer for dynamic loading operations
 *
 * This class provides a single, platform-independent interface for all
 * dynamic library loading operations. It automatically selects the appropriate
 * platform-specific loader (LinuxLoader, WindowsLoader, etc.) based on the
 * compile-time platform detection.
 *
 * Features:
 * - Automatic platform detection and loader selection
 * - Singleton-style access for framework-wide usage
 * - Thread-safe operations
 * - Consistent API across all platforms
 * - Centralized error handling
 *
 * Usage:
 * @code
 * PlatformAbstraction platform;
 * LibraryHandle handle = platform.loadLibrary("/path/to/module.so");
 * auto* createFn = platform.getSymbol<CreateModuleFn>(handle, "createModule");
 * createFn();
 * platform.unloadLibrary(handle);
 * @endcode
 */
class PlatformAbstraction {
public:
    /**
     * @brief Construct platform abstraction with auto-detected platform
     *
     * Automatically creates the appropriate platform-specific loader
     * based on compile-time platform detection.
     *
     * @throws std::runtime_error if platform is unsupported
     */
    PlatformAbstraction();

    /**
     * @brief Destructor - cleans up all loaded libraries
     */
    ~PlatformAbstraction();

    // Prevent copying
    PlatformAbstraction(const PlatformAbstraction&) = delete;
    PlatformAbstraction& operator=(const PlatformAbstraction&) = delete;

    // Allow moving
    PlatformAbstraction(PlatformAbstraction&&) noexcept = default;
    PlatformAbstraction& operator=(PlatformAbstraction&&) noexcept = default;

    /**
     * @brief Load a dynamic library
     *
     * @param path Absolute or relative path to library file
     * @return LibraryHandle Handle to loaded library
     * @throws DynamicLoaderException on load failure
     */
    LibraryHandle loadLibrary(const std::string& path);

    /**
     * @brief Unload a previously loaded library
     *
     * @param handle Valid library handle from loadLibrary()
     * @throws DynamicLoaderException if handle is invalid or unload fails
     */
    void unloadLibrary(LibraryHandle handle);

    /**
     * @brief Get a symbol from a loaded library
     *
     * @param handle Valid library handle
     * @param symbolName Name of the symbol to resolve
     * @return void* Pointer to symbol, or nullptr if not found
     * @throws DynamicLoaderException if handle is invalid
     */
    void* getSymbol(LibraryHandle handle, const std::string& symbolName);

    /**
     * @brief Get a typed symbol from a loaded library
     *
     * Template convenience method that casts the symbol to the specified type.
     *
     * @tparam T Function pointer or data pointer type
     * @param handle Valid library handle
     * @param symbolName Name of the symbol to resolve
     * @return T Typed pointer to symbol, or nullptr if not found
     * @throws DynamicLoaderException if handle is invalid
     *
     * Example:
     * @code
     * using CreateFn = IModule* (*)();
     * auto createModule = platform.getSymbol<CreateFn>(handle, "createModule");
     * if (createModule) {
     *     IModule* module = createModule();
     * }
     * @endcode
     */
    template<typename T>
    T getSymbol(LibraryHandle handle, const std::string& symbolName) {
        void* symbol = getSymbol(handle, symbolName);
        return reinterpret_cast<T>(symbol);
    }

    /**
     * @brief Get the current platform
     *
     * @return Platform Detected platform (LINUX, WINDOWS, MACOS)
     */
    Platform getPlatform() const { return currentPlatform_; }

    /**
     * @brief Get platform-specific library file extension
     *
     * @return const char* Extension (.so, .dll, .dylib)
     */
    const char* getLibraryExtension() const {
        return platform::getLibraryExtension(currentPlatform_);
    }

    /**
     * @brief Get platform-specific library prefix
     *
     * @return const char* Prefix (lib on Unix, empty on Windows)
     */
    const char* getLibraryPrefix() const {
        return platform::getLibraryPrefix(currentPlatform_);
    }

    /**
     * @brief Get the last error message from the underlying loader
     *
     * @return std::string Error description
     */
    std::string getLastError() const;

    /**
     * @brief Check if a library handle is valid
     *
     * @param handle Handle to check
     * @return true if handle is valid and library is loaded
     */
    bool isLibraryLoaded(LibraryHandle handle) const;

    /**
     * @brief Get the path of a loaded library
     *
     * @param handle Valid library handle
     * @return std::string Original path used to load the library
     * @throws std::out_of_range if handle not found
     */
    std::string getLibraryPath(LibraryHandle handle) const;

    /**
     * @brief Get count of currently loaded libraries
     *
     * @return size_t Number of loaded libraries
     */
    size_t getLoadedLibraryCount() const;

private:
    /**
     * @brief Current platform
     */
    Platform currentPlatform_;

    /**
     * @brief Platform-specific loader implementation
     */
    std::unique_ptr<IDynamicLoader> loader_;

    /**
     * @brief Map of library handles to their original paths
     */
    std::map<LibraryHandle, std::string> loadedLibraries_;

    /**
     * @brief Mutex for thread-safe operations
     */
    mutable std::mutex mutex_;

    /**
     * @brief Create the appropriate platform-specific loader
     *
     * @param platform Target platform
     * @return std::unique_ptr<IDynamicLoader> Platform loader instance
     * @throws std::runtime_error if platform is unsupported
     */
    std::unique_ptr<IDynamicLoader> createLoader(Platform platform);
};

} // namespace platform
} // namespace cdmf

#endif // CDMF_PLATFORM_ABSTRACTION_H
