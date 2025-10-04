#include "platform/platform_abstraction.h"

#ifdef __linux__
#include "platform/linux_loader.h"
#elif defined(_WIN32) || defined(_WIN64)
#include "platform/windows_loader.h"
#endif

#include <stdexcept>
#include <sstream>

namespace cdmf {
namespace platform {

PlatformAbstraction::PlatformAbstraction()
    : currentPlatform_(getCurrentPlatform())
{
    loader_ = createLoader(currentPlatform_);
}

PlatformAbstraction::~PlatformAbstraction() {
    // Loader destructor will clean up remaining libraries
}

LibraryHandle PlatformAbstraction::loadLibrary(const std::string& path) {
    if (!loader_) {
        throw std::runtime_error("Platform loader not initialized");
    }

    LibraryHandle handle = loader_->load(path);

    // Track the loaded library
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loadedLibraries_[handle] = path;
    }

    return handle;
}

void PlatformAbstraction::unloadLibrary(LibraryHandle handle) {
    if (!loader_) {
        throw std::runtime_error("Platform loader not initialized");
    }

    loader_->unload(handle);

    // Remove from tracking
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loadedLibraries_.erase(handle);
    }
}

void* PlatformAbstraction::getSymbol(LibraryHandle handle, const std::string& symbolName) {
    if (!loader_) {
        throw std::runtime_error("Platform loader not initialized");
    }

    return loader_->getSymbol(handle, symbolName);
}

std::string PlatformAbstraction::getLastError() const {
    if (!loader_) {
        return "Platform loader not initialized";
    }

    return loader_->getLastError();
}

bool PlatformAbstraction::isLibraryLoaded(LibraryHandle handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadedLibraries_.find(handle) != loadedLibraries_.end();
}

std::string PlatformAbstraction::getLibraryPath(LibraryHandle handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = loadedLibraries_.find(handle);
    if (it == loadedLibraries_.end()) {
        throw std::out_of_range("Library handle not found");
    }
    return it->second;
}

size_t PlatformAbstraction::getLoadedLibraryCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadedLibraries_.size();
}

std::unique_ptr<IDynamicLoader> PlatformAbstraction::createLoader(Platform platform) {
    switch (platform) {
#ifdef __linux__
        case Platform::LINUX:
            return std::make_unique<LinuxLoader>();
#endif

#if defined(_WIN32) || defined(_WIN64)
        case Platform::WINDOWS:
            return std::make_unique<WindowsLoader>();
#endif

        case Platform::MACOS:
            // macOS uses the same dlopen API as Linux
#ifdef __APPLE__
            return std::make_unique<LinuxLoader>();  // Reuse Linux loader for macOS
#else
            throw std::runtime_error("macOS platform loader not available on this platform");
#endif

        case Platform::UNKNOWN:
        default: {
            std::ostringstream oss;
            oss << "Unsupported platform: " << platformToString(platform);
            throw std::runtime_error(oss.str());
        }
    }
}

} // namespace platform
} // namespace cdmf
