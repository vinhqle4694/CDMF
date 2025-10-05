#include "module/module_impl.h"
#include "core/framework.h"
#include <stdexcept>
#include <mutex>
#include <algorithm>

namespace cdmf {

// Stub context implementation for now (full implementation in PHASE_5)
class ModuleContextImpl : public IModuleContext {
public:
    ModuleContextImpl(Module* module, Framework* framework)
        : module_(module), framework_(framework) {}

    Module* getModule() override { return module_; }

    const FrameworkProperties& getProperties() const override {
        throw std::runtime_error("Not implemented - requires Framework integration");
    }

    std::string getProperty(const std::string& key) const override {
        if (framework_) {
            return framework_->getProperties().getString(key, "");
        }
        return "";
    }

    ServiceRegistration registerService(const std::string& serviceName, void* serviceInstance, const Properties& props) override {
        if (framework_) {
            // Delegate to framework context which will register the service
            IModuleContext* frameworkContext = framework_->getContext();
            if (frameworkContext) {
                return frameworkContext->registerService(serviceName, serviceInstance, props);
            }
        }
        return ServiceRegistration();
    }

    std::vector<ServiceReference> getServiceReferences(const std::string& serviceName, const std::string& filter) const override {
        if (framework_) {
            IModuleContext* frameworkContext = framework_->getContext();
            if (frameworkContext) {
                return frameworkContext->getServiceReferences(serviceName, filter);
            }
        }
        return {};
    }

    ServiceReference getServiceReference(const std::string& serviceName) const override {
        if (framework_) {
            IModuleContext* frameworkContext = framework_->getContext();
            if (frameworkContext) {
                return frameworkContext->getServiceReference(serviceName);
            }
        }
        return ServiceReference();
    }

    std::shared_ptr<void> getService(const ServiceReference& serviceRef) override {
        if (framework_) {
            IModuleContext* frameworkContext = framework_->getContext();
            if (frameworkContext) {
                return frameworkContext->getService(serviceRef);
            }
        }
        return nullptr;
    }

    bool ungetService(const ServiceReference& serviceRef) override {
        if (framework_) {
            IModuleContext* frameworkContext = framework_->getContext();
            if (frameworkContext) {
                return frameworkContext->ungetService(serviceRef);
            }
        }
        return false;
    }

    void addEventListener(IEventListener*, const EventFilter&, int, bool) override {}
    void removeEventListener(IEventListener*) override {}
    void fireEvent(const Event&) override {}
    void fireEventSync(const Event&) override {}
    Module* installModule(const std::string&) override { return nullptr; }
    std::vector<Module*> getModules() const override { return {}; }
    Module* getModule(uint64_t) const override { return nullptr; }
    Module* getModule(const std::string&) const override { return nullptr; }

private:
    Module* module_;
    Framework* framework_;
};

ModuleImpl::ModuleImpl(uint64_t moduleId,
                       std::unique_ptr<ModuleHandle> handle,
                       const ModuleManifest& manifest,
                       Framework* framework)
    : moduleId_(moduleId)
    , manifest_(manifest)
    , state_(ModuleState::INSTALLED)
    , handle_(std::move(handle))
    , activator_(nullptr)
    , context_(nullptr)
    , framework_(framework)
    , listeners_()
    , mutex_() {
}

ModuleImpl::~ModuleImpl() {
    try {
        if (state_ == ModuleState::ACTIVE) {
            stop();
        }
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

std::string ModuleImpl::getSymbolicName() const {
    return manifest_.symbolicName;
}

Version ModuleImpl::getVersion() const {
    return manifest_.version;
}

std::string ModuleImpl::getLocation() const {
    if (handle_) {
        return handle_->getLocation();
    }
    return "";
}

uint64_t ModuleImpl::getModuleId() const {
    return moduleId_;
}

void ModuleImpl::start() {
    std::unique_lock lock(mutex_);

    ModuleState currentState = state_.load();

    if (currentState == ModuleState::ACTIVE) {
        return; // Already active
    }

    if (currentState != ModuleState::RESOLVED) {
        throw ModuleException("Module must be RESOLVED before starting", currentState);
    }

    try {
        transitionTo(ModuleState::STARTING);

        // Create activator
        if (!activator_) {
            createActivator();
        }

        // Create context
        context_ = std::make_unique<ModuleContextImpl>(this, framework_);

        // Call activator
        if (activator_) {
            activator_->start(context_.get());
        }

        transitionTo(ModuleState::ACTIVE);

    } catch (const std::exception& e) {
        transitionTo(ModuleState::RESOLVED);
        throw ModuleException(std::string("Failed to start module: ") + e.what());
    }
}

void ModuleImpl::stop() {
    std::unique_lock lock(mutex_);

    ModuleState currentState = state_.load();

    if (currentState != ModuleState::ACTIVE) {
        return; // Not active
    }

    try {
        transitionTo(ModuleState::STOPPING);

        // Call activator
        if (activator_ && context_) {
            activator_->stop(context_.get());
        }

        // Destroy context
        context_.reset();

        // Destroy activator
        destroyActivator();

        transitionTo(ModuleState::RESOLVED);

    } catch (const std::exception&) {
        transitionTo(ModuleState::RESOLVED);
        throw;
    }
}

void ModuleImpl::update(const std::string& location) {
    std::unique_lock lock(mutex_);

    // Stop if active
    if (state_ == ModuleState::ACTIVE) {
        lock.unlock();
        stop();
        lock.lock();
    }

    // Reload module handle
    handle_ = std::make_unique<ModuleHandle>(location);

    // Activator will be recreated on next start
    activator_.reset();
}

void ModuleImpl::uninstall() {
    std::unique_lock lock(mutex_);

    // Stop if active
    if (state_ == ModuleState::ACTIVE) {
        lock.unlock();
        stop();
        lock.lock();
    }

    transitionTo(ModuleState::UNINSTALLED);

    // Cleanup
    handle_.reset();
    activator_.reset();
    context_.reset();
}

ModuleState ModuleImpl::getState() const {
    return state_.load();
}

IModuleContext* ModuleImpl::getContext() {
    if (state_ == ModuleState::ACTIVE) {
        return context_.get();
    }
    return nullptr;
}

std::vector<ServiceRegistration> ModuleImpl::getRegisteredServices() const {
    // Stub - implemented in PHASE_5
    return {};
}

std::vector<ServiceReference> ModuleImpl::getServicesInUse() const {
    // Stub - implemented in PHASE_5
    return {};
}

const nlohmann::json& ModuleImpl::getManifest() const {
    return manifest_.rawJson;
}

std::map<std::string, std::string> ModuleImpl::getHeaders() const {
    std::map<std::string, std::string> headers;
    headers["symbolic-name"] = manifest_.symbolicName;
    headers["version"] = manifest_.version.toString();
    headers["name"] = manifest_.name;
    headers["description"] = manifest_.description;
    headers["vendor"] = manifest_.vendor;
    headers["category"] = manifest_.category;
    return headers;
}

void ModuleImpl::addModuleListener(IModuleListener* listener) {
    if (!listener) {
        return;
    }

    std::unique_lock lock(mutex_);
    listeners_.push_back(listener);
}

void ModuleImpl::removeModuleListener(IModuleListener* listener) {
    if (!listener) {
        return;
    }

    std::unique_lock lock(mutex_);
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end()
    );
}

void ModuleImpl::transitionTo(ModuleState newState) {
    ModuleState oldState = state_.exchange(newState);

    // Fire appropriate event based on transition
    if (oldState == ModuleState::RESOLVED && newState == ModuleState::STARTING) {
        fireModuleEvent(ModuleEventType::MODULE_STARTING);
    } else if (oldState == ModuleState::STARTING && newState == ModuleState::ACTIVE) {
        fireModuleEvent(ModuleEventType::MODULE_STARTED);
    } else if (oldState == ModuleState::ACTIVE && newState == ModuleState::STOPPING) {
        fireModuleEvent(ModuleEventType::MODULE_STOPPING);
    } else if (oldState == ModuleState::STOPPING && newState == ModuleState::RESOLVED) {
        fireModuleEvent(ModuleEventType::MODULE_STOPPED);
    } else if (newState == ModuleState::UNINSTALLED) {
        fireModuleEvent(ModuleEventType::MODULE_UNINSTALLED);
    }
}

void ModuleImpl::fireModuleEvent(ModuleEventType eventType) {
    Event event(moduleEventTypeToString(eventType), this);
    event.setProperty("module.id", static_cast<int>(moduleId_));
    event.setProperty("module.symbolic-name", manifest_.symbolicName);
    event.setProperty("module.version", manifest_.version.toString());

    // Notify module-specific listeners
    for (auto* listener : listeners_) {
        if (listener) {
            listener->moduleChanged(event);
        }
    }

    // TODO: Fire event to framework event dispatcher (PHASE_5)
}

void ModuleImpl::createActivator() {
    if (!handle_) {
        throw std::runtime_error("Module handle not available");
    }

    activator_.reset(handle_->createActivator());

    if (!activator_) {
        throw std::runtime_error("Failed to create module activator");
    }
}

void ModuleImpl::destroyActivator() {
    if (activator_ && handle_) {
        handle_->destroyActivator(activator_.release());
    }
    activator_.reset();
}

} // namespace cdmf
