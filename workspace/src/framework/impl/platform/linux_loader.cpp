#include "platform/linux_loader.h"

#ifdef __linux__

#include <dlfcn.h>
#include <sstream>

namespace cdmf {
namespace platform {

LinuxLoader::LinuxLoader() = default;

LinuxLoader::~LinuxLoader() {
    // Unload all remaining libraries
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [handle, path] : handles_) {
        if (handle != INVALID_LIBRARY_HANDLE) {
            ::dlclose(handle);
        }
    }
    handles_.clear();
}

LibraryHandle LinuxLoader::load(const std::string& path) {
    if (path.empty()) {
        throw DynamicLoaderException("Cannot load library: path is empty");
    }

    // Clear any previous errors
    ::dlerror();

    // Load library with RTLD_LAZY (resolve symbols on first use) and
    // RTLD_LOCAL (symbols not available for symbol resolution of subsequently loaded libraries)
    void* handle = ::dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);

    if (handle == nullptr) {
        const char* errorMsg = ::dlerror();
        lastError_ = errorMsg ? errorMsg : "Unknown dlopen error";

        std::ostringstream oss;
        oss << "Failed to load library '" << path << "': " << lastError_;
        throw DynamicLoaderException(oss.str());
    }

    // Track the loaded handle
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_[handle] = path;
    }

    return handle;
}

void LinuxLoader::unload(LibraryHandle handle) {
    if (handle == INVALID_LIBRARY_HANDLE) {
        throw DynamicLoaderException("Cannot unload library: invalid handle");
    }

    std::string path;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handles_.find(handle);
        if (it == handles_.end()) {
            throw DynamicLoaderException("Cannot unload library: handle not found");
        }
        path = it->second;
        handles_.erase(it);
    }

    // Clear any previous errors
    ::dlerror();

    // Unload the library
    if (::dlclose(handle) != 0) {
        const char* errorMsg = ::dlerror();
        lastError_ = errorMsg ? errorMsg : "Unknown dlclose error";

        std::ostringstream oss;
        oss << "Failed to unload library '" << path << "': " << lastError_;
        throw DynamicLoaderException(oss.str());
    }
}

void* LinuxLoader::getSymbol(LibraryHandle handle, const std::string& symbolName) {
    if (handle == INVALID_LIBRARY_HANDLE) {
        throw DynamicLoaderException("Cannot get symbol: invalid library handle");
    }

    if (!isValidHandle(handle)) {
        throw DynamicLoaderException("Cannot get symbol: library handle not found");
    }

    if (symbolName.empty()) {
        throw DynamicLoaderException("Cannot get symbol: symbol name is empty");
    }

    // Clear any previous errors
    ::dlerror();

    // Look up the symbol
    void* symbol = ::dlsym(handle, symbolName.c_str());

    // Check for errors (note: symbol can legitimately be nullptr)
    const char* errorMsg = ::dlerror();
    if (errorMsg != nullptr) {
        lastError_ = errorMsg;
        // Don't throw - symbol simply doesn't exist, return nullptr
        return nullptr;
    }

    return symbol;
}

std::string LinuxLoader::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

bool LinuxLoader::isValidHandle(LibraryHandle handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handles_.find(handle) != handles_.end();
}

} // namespace platform
} // namespace cdmf

#endif // __linux__
