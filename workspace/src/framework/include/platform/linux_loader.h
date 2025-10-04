#ifndef CDMF_LINUX_LOADER_H
#define CDMF_LINUX_LOADER_H

#include "dynamic_loader.h"
#include <map>
#include <mutex>

namespace cdmf {
namespace platform {

/**
 * @brief Linux-specific dynamic library loader using dlopen/dlsym/dlclose
 *
 * This implementation uses POSIX dynamic linking API (dlfcn.h) to load
 * shared objects (.so files) on Linux systems.
 *
 * Features:
 * - RTLD_LAZY symbol resolution (symbols resolved on first use)
 * - RTLD_LOCAL visibility (symbols not available to subsequently loaded libraries)
 * - Thread-safe handle tracking with mutex protection
 * - Detailed error reporting via dlerror()
 *
 * Thread-Safety: All methods are thread-safe
 */
class LinuxLoader : public IDynamicLoader {
public:
    /**
     * @brief Construct a Linux dynamic loader
     */
    LinuxLoader();

    /**
     * @brief Destructor - unloads all remaining libraries
     */
    ~LinuxLoader() override;

    // Prevent copying
    LinuxLoader(const LinuxLoader&) = delete;
    LinuxLoader& operator=(const LinuxLoader&) = delete;

    // Allow moving
    LinuxLoader(LinuxLoader&&) noexcept = default;
    LinuxLoader& operator=(LinuxLoader&&) noexcept = default;

    /**
     * @brief Load a shared object using dlopen()
     *
     * @param path Path to .so file (can include lib prefix and .so extension)
     * @return LibraryHandle Handle to the loaded library
     * @throws DynamicLoaderException if dlopen() fails
     */
    LibraryHandle load(const std::string& path) override;

    /**
     * @brief Unload a shared object using dlclose()
     *
     * @param handle Valid library handle
     * @throws DynamicLoaderException if handle is invalid or dlclose() fails
     */
    void unload(LibraryHandle handle) override;

    /**
     * @brief Resolve a symbol using dlsym()
     *
     * @param handle Valid library handle
     * @param symbolName Symbol name (unmangled for C functions)
     * @return void* Symbol address, or nullptr if not found
     * @throws DynamicLoaderException if handle is invalid
     */
    void* getSymbol(LibraryHandle handle, const std::string& symbolName) override;

    /**
     * @brief Get last error from dlerror()
     *
     * @return std::string Error description
     */
    std::string getLastError() const override;

    /**
     * @brief Get platform identifier
     *
     * @return Platform::LINUX
     */
    Platform getPlatform() const override { return Platform::LINUX; }

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
     * @brief Last error message from dlerror()
     */
    mutable std::string lastError_;

    /**
     * @brief Validate that a handle exists in our tracking map
     *
     * @param handle Handle to validate
     * @return true if valid, false otherwise
     */
    bool isValidHandle(LibraryHandle handle) const;
};

} // namespace platform
} // namespace cdmf

#endif // CDMF_LINUX_LOADER_H
