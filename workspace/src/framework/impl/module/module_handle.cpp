#include "module/module_handle.h"
#include <stdexcept>

namespace cdmf {

ModuleHandle::ModuleHandle(const std::string& location)
    : location_(location)
    , platform_(nullptr)
    , handle_(nullptr)
    , createFunc_(nullptr)
    , destroyFunc_(nullptr) {

    if (location.empty()) {
        throw std::invalid_argument("Module location cannot be empty");
    }

    // Create platform abstraction
    platform_ = std::make_unique<platform::PlatformAbstraction>();

    // Load the library
    try {
        handle_ = platform_->loadLibrary(location);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load module library: " + location +
                                " - " + std::string(e.what()));
    }

    // Resolve activator factory functions
    try {
        resolveActivatorFunctions();
    } catch (...) {
        platform_->unloadLibrary(handle_);
        throw;
    }
}

ModuleHandle::~ModuleHandle() {
    if (platform_ && handle_) {
        try {
            platform_->unloadLibrary(handle_);
        } catch (...) {
            // Suppress exceptions in destructor
        }
    }
}

ModuleHandle::ModuleHandle(ModuleHandle&& other) noexcept
    : location_(std::move(other.location_))
    , platform_(std::move(other.platform_))
    , handle_(other.handle_)
    , createFunc_(other.createFunc_)
    , destroyFunc_(other.destroyFunc_) {

    other.handle_ = nullptr;
    other.createFunc_ = nullptr;
    other.destroyFunc_ = nullptr;
}

ModuleHandle& ModuleHandle::operator=(ModuleHandle&& other) noexcept {
    if (this != &other) {
        // Unload current library
        if (platform_ && handle_) {
            try {
                platform_->unloadLibrary(handle_);
            } catch (...) {
                // Suppress exceptions
            }
        }

        location_ = std::move(other.location_);
        platform_ = std::move(other.platform_);
        handle_ = other.handle_;
        createFunc_ = other.createFunc_;
        destroyFunc_ = other.destroyFunc_;

        other.handle_ = nullptr;
        other.createFunc_ = nullptr;
        other.destroyFunc_ = nullptr;
    }
    return *this;
}

IModuleActivator* ModuleHandle::createActivator() {
    if (!createFunc_) {
        throw std::runtime_error("createModuleActivator function not available");
    }

    IModuleActivator* activator = createFunc_();
    if (!activator) {
        throw std::runtime_error("createModuleActivator returned nullptr");
    }

    return activator;
}

void ModuleHandle::destroyActivator(IModuleActivator* activator) {
    if (!destroyFunc_) {
        throw std::runtime_error("destroyModuleActivator function not available");
    }

    if (activator) {
        destroyFunc_(activator);
    }
}

void* ModuleHandle::getSymbol(const std::string& symbolName) {
    if (!platform_ || !handle_) {
        return nullptr;
    }

    return platform_->getSymbol(handle_, symbolName);
}

void ModuleHandle::resolveActivatorFunctions() {
    // Resolve createModuleActivator
    void* createSym = platform_->getSymbol(handle_, "createModuleActivator");
    if (!createSym) {
        throw std::runtime_error(
            "Module does not export 'createModuleActivator' function");
    }
    createFunc_ = reinterpret_cast<CreateModuleActivatorFunc>(createSym);

    // Resolve destroyModuleActivator
    void* destroySym = platform_->getSymbol(handle_, "destroyModuleActivator");
    if (!destroySym) {
        throw std::runtime_error(
            "Module does not export 'destroyModuleActivator' function");
    }
    destroyFunc_ = reinterpret_cast<DestroyModuleActivatorFunc>(destroySym);
}

} // namespace cdmf
