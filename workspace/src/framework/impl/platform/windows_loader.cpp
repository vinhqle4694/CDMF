#include "platform/windows_loader.h"

#if defined(_WIN32) || defined(_WIN64)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sstream>

namespace cdmf {
namespace platform {

WindowsLoader::WindowsLoader() = default;

WindowsLoader::~WindowsLoader() {
    // Unload all remaining libraries
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [handle, path] : handles_) {
        if (handle != INVALID_LIBRARY_HANDLE) {
            ::FreeLibrary(static_cast<HMODULE>(handle));
        }
    }
    handles_.clear();
}

LibraryHandle WindowsLoader::load(const std::string& path) {
    if (path.empty()) {
        throw DynamicLoaderException("Cannot load library: path is empty");
    }

    // Load library with LOAD_WITH_ALTERED_SEARCH_PATH flag
    // This allows the DLL to find its dependencies in its own directory
    HMODULE hModule = ::LoadLibraryExA(
        path.c_str(),
        nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH
    );

    if (hModule == nullptr) {
        DWORD errorCode = ::GetLastError();
        lastError_ = formatWindowsError(errorCode);

        std::ostringstream oss;
        oss << "Failed to load library '" << path << "': " << lastError_;
        throw DynamicLoaderException(oss.str());
    }

    // Cast HMODULE to void* for our handle type
    LibraryHandle handle = static_cast<LibraryHandle>(hModule);

    // Track the loaded handle
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handles_[handle] = path;
    }

    return handle;
}

void WindowsLoader::unload(LibraryHandle handle) {
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

    // Unload the library
    HMODULE hModule = static_cast<HMODULE>(handle);
    if (!::FreeLibrary(hModule)) {
        DWORD errorCode = ::GetLastError();
        lastError_ = formatWindowsError(errorCode);

        std::ostringstream oss;
        oss << "Failed to unload library '" << path << "': " << lastError_;
        throw DynamicLoaderException(oss.str());
    }
}

void* WindowsLoader::getSymbol(LibraryHandle handle, const std::string& symbolName) {
    if (handle == INVALID_LIBRARY_HANDLE) {
        throw DynamicLoaderException("Cannot get symbol: invalid library handle");
    }

    if (!isValidHandle(handle)) {
        throw DynamicLoaderException("Cannot get symbol: library handle not found");
    }

    if (symbolName.empty()) {
        throw DynamicLoaderException("Cannot get symbol: symbol name is empty");
    }

    // Look up the symbol
    HMODULE hModule = static_cast<HMODULE>(handle);
    FARPROC procAddress = ::GetProcAddress(hModule, symbolName.c_str());

    if (procAddress == nullptr) {
        DWORD errorCode = ::GetLastError();
        if (errorCode != 0) {
            lastError_ = formatWindowsError(errorCode);
        }
        // Don't throw - symbol simply doesn't exist, return nullptr
        return nullptr;
    }

    // Cast FARPROC to void*
    return reinterpret_cast<void*>(procAddress);
}

std::string WindowsLoader::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

bool WindowsLoader::isValidHandle(LibraryHandle handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handles_.find(handle) != handles_.end();
}

std::string WindowsLoader::formatWindowsError(unsigned long errorCode) const {
    if (errorCode == 0) {
        return "No error";
    }

    char* messageBuffer = nullptr;
    size_t size = ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr
    );

    std::string message;
    if (size > 0 && messageBuffer != nullptr) {
        message = std::string(messageBuffer, size);
        // Remove trailing newlines
        while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }
    } else {
        std::ostringstream oss;
        oss << "Windows error code: " << errorCode;
        message = oss.str();
    }

    if (messageBuffer != nullptr) {
        ::LocalFree(messageBuffer);
    }

    return message;
}

} // namespace platform
} // namespace cdmf

#endif // _WIN32 || _WIN64
