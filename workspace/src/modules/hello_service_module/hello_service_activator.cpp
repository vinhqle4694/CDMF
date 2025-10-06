/**
 * @file hello_service_activator.cpp
 * @brief Module activator implementation for Hello Service
 */

#include "hello_service_activator.h"
#include "module/module_context.h"
#include "module/module.h"
#include "core/event.h"
#include "core/event_filter.h"
#include "utils/log.h"
#include "utils/properties.h"
#include "security/permission_manager.h"
#include "security/permission_types.h"

namespace cdmf {
namespace modules {

// HelloBootCompletedListener implementation

HelloBootCompletedListener::HelloBootCompletedListener(services::HelloServiceImpl* service)
    : service_(service) {
}

void HelloBootCompletedListener::handleEvent(const Event& event) {
    LOGI_FMT("HelloBootCompletedListener: handleEvent called with type='" << event.getType() << "'");

    if (event.getType() == "BOOT_COMPLETED") {
        LOGI("HelloBootCompletedListener: Received BOOT_COMPLETED event");

        // Start the actual service work now that all services are ready
        if (service_) {
            service_->startWork();
        } else {
            LOGW("HelloBootCompletedListener: service_ is null!");
        }
    } else {
        LOGW_FMT("HelloBootCompletedListener: Event type mismatch - expected 'BOOT_COMPLETED', got '" << event.getType() << "'");
    }
}

// HelloServiceActivator implementation

HelloServiceActivator::HelloServiceActivator()
    : service_(nullptr)
    , bootListener_(nullptr)
    , registration_() {
}

HelloServiceActivator::~HelloServiceActivator() {
    // Cleanup is done in stop()
}

void HelloServiceActivator::start(IModuleContext* context) {
    LOGI("HelloServiceActivator: Starting module...");

    // Get module and read manifest
    Module* module = context->getModule();
    const auto& manifest = module->getManifest();

    // Get module ID from manifest (symbolic-name)
    std::string moduleId = manifest["module"]["symbolic-name"].get<std::string>();
    LOGI_FMT("HelloServiceActivator: Module ID from manifest: " << moduleId);

    // Get service interface from manifest exports
    std::string serviceInterface = manifest["exports"][0]["interface"].get<std::string>();
    LOGI_FMT("HelloServiceActivator: Service interface from manifest: " << serviceInterface);

    // Get log file path from security configuration
    std::string logFilePath = manifest["security"]["permissions"][2]["target"].get<std::string>();
    LOGI_FMT("HelloServiceActivator: Log file path from manifest: " << logFilePath);

    auto& permMgr = security::PermissionManager::getInstance();

    // 1. Check SERVICE_REGISTER permission before registering service
    if (!permMgr.checkPermission(moduleId, security::PermissionType::SERVICE_REGISTER, serviceInterface)) {
        LOGE_FMT("HelloServiceActivator: Missing SERVICE_REGISTER permission for " << serviceInterface);
        throw std::runtime_error("Permission denied: SERVICE_REGISTER for " + serviceInterface);
    }
    LOGI("HelloServiceActivator: Permission check passed for SERVICE_REGISTER");

    // 2. Create service instance with log file path and module ID from manifest
    service_ = new services::HelloServiceImpl(logFilePath, moduleId);

    // 3. Register service with framework
    Properties props;
    props.set("service.description", manifest["module"]["description"].get<std::string>());
    props.set("service.vendor", "CDMF");
    props.set("service.version", manifest["module"]["version"].get<std::string>());

    registration_ = context->registerService(serviceInterface, service_, props);
    LOGI("HelloServiceActivator: Service registered");

    // 4. Check EVENT_SUBSCRIBE permission before subscribing to events
    std::string bootEventTarget = manifest["security"]["permissions"][1]["target"].get<std::string>();
    if (!permMgr.checkPermission(moduleId, security::PermissionType::EVENT_SUBSCRIBE, bootEventTarget)) {
        LOGE_FMT("HelloServiceActivator: Missing EVENT_SUBSCRIBE permission for " << bootEventTarget);
        throw std::runtime_error("Permission denied: EVENT_SUBSCRIBE for " + bootEventTarget);
    }
    LOGI("HelloServiceActivator: Permission check passed for EVENT_SUBSCRIBE");

    // 5. Create BOOT_COMPLETED listener
    bootListener_ = new HelloBootCompletedListener(service_);

    // 6. Register listener for BOOT_COMPLETED event
    EventFilter filter("(type=" + bootEventTarget + ")");
    LOGI_FMT("HelloServiceActivator: Created filter with string: '" << filter.toString() << "'");
    context->addEventListener(bootListener_, filter, 0, true);  // synchronous=true
    LOGI("HelloServiceActivator: BOOT_COMPLETED listener registered (synchronous=true)");

    LOGI("HelloServiceActivator: Module started (waiting for BOOT_COMPLETED)");
}

void HelloServiceActivator::stop(IModuleContext* context) {
    LOGI("HelloServiceActivator: Stopping module...");

    // 1. Remove event listener
    if (bootListener_) {
        context->removeEventListener(bootListener_);
        delete bootListener_;
        bootListener_ = nullptr;
    }

    // 2. Unregister service
    if (registration_.isValid()) {
        registration_.unregister();
    }

    // 3. Stop service work and cleanup
    if (service_) {
        service_->stopWork();
        delete service_;
        service_ = nullptr;
    }

    LOGI("HelloServiceActivator: Module stopped");
}

} // namespace modules
} // namespace cdmf

// Module factory functions
extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::HelloServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
