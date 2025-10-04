#ifndef CDMF_WINDOWS_LOADER_H
#define CDMF_WINDOWS_LOADER_H

#include "dynamic_loader.h"
#include <map>
#include <mutex>

namespace cdmf {
namespace platform {

/**
 * @brief Windows-specific dynamic library loader using LoadLibrary/GetProcAddress/FreeLibrary
 *
 * This implementation uses Windows API to load Dynamic Link Libraries (.dll files).
 *
 * Features:
 * - LoadLibraryExA with LOAD_WITH_ALTERED_SEARCH_PATH for proper dependency loading
 * - Thread-safe handle tracking with mutex protection
 * - Detailed error reporting via GetLastError() and FormatMessage()
 * - Automatic conversion of Windows error codes to readable messages
 *
 * Thread-Safety: All methods are thread-safe
 */
class WindowsLoader : public IDynamicLoader {
public:
    /**
     * @brief Construct a Windows dynamic loader
     */
    WindowsLoader();

    /**
     * @brief Destructor - unloads all remaining libraries
     */
    ~WindowsLoader() override;

    // Prevent copying
    WindowsLoader(const WindowsLoader&) = delete;
    WindowsLoader& operator=(const WindowsLoader&) = delete;

    // Allow moving
    WindowsLoader(WindowsLoader&&) noexcept = default;
    WindowsLoader& operator=(WindowsLoader&&) noexcept = default;

    /**
     * @brief Load a DLL using LoadLibraryExA()
     *
     * @param path Path to .dll file
     * @return LibraryHandle Handle to the loaded library (HMODULE cast to void*)
     * @throws DynamicLoaderException if LoadLibraryExA() fails
     */
    LibraryHandle load(const std::string& path) override;

    /**
     * @brief Unload a DLL using FreeLibrary()
     *
     * @param handle Valid library handle
     * @throws DynamicLoaderException if handle is invalid or FreeLibrary() fails
     */
    void unload(LibraryHandle handle) override;

    /**
     * @brief Resolve a symbol using GetProcAddress()
     *
     * @param handle Valid library handle
     * @param symbolName Symbol name (function or variable name)
     * @return void* Symbol address, or nullptr if not found
     * @throws DynamicLoaderException if handle is invalid
     */
    void* getSymbol(LibraryHandle handle, const std::string& symbolName) override;

    /**
     * @brief Get last error from GetLastError() and FormatMessage()
     *
     * @return std::string Error description
     */
    std::string getLastError() const override;

    /**
     * @brief Get platform identifier
     *
     * @return Platform::WINDOWS
     */
    Platform getPlatform() const override { return Platform::WINDOWS; }

private:
    /**
     * @brief Storage for loaded library handles with their paths
     *
     * Maps LibraryHandle -> original path for error reporting and debugging
     */
    std::map<LibraryHandle, std::string> handles_;

    /**
     * @brief Mutex for thread-safe handle tracking
     */
    mutable std::mutex mutex_;

    /**
     * @brief Last error message
     */
    mutable std::string lastError_;

    /**
     * @brief Validate that a handle exists in our tracking map
     *
     * @param handle Handle to validate
     * @return true if valid, false otherwise
     */
    bool isValidHandle(LibraryHandle handle) const;

    /**
     * @brief Convert Windows error code to readable string
     *
     * @param errorCode Error code from GetLastError()
     * @return std::string Formatted error message
     */
    std::string formatWindowsError(unsigned long errorCode) const;
};

} // namespace platform
} // namespace cdmf

#endif // CDMF_WINDOWS_LOADER_H
