/**
 * @file hello_service_activator.cpp
 * @brief Module activator implementation for Hello Service
 */

#include "hello_service_activator.h"
#include "module/module_context.h"
#include "core/event.h"
#include "core/event_filter.h"
#include "utils/log.h"
#include "utils/properties.h"

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

    // 1. Create service instance (but don't start work yet)
    service_ = new services::HelloServiceImpl();

    // 2. Register service with framework
    Properties props;
    props.set("service.description", "Simple hello/greeting service");
    props.set("service.vendor", "CDMF");
    props.set("service.version", "1.0.0");

    registration_ = context->registerService("cdmf::IHelloService", service_, props);
    LOGI("HelloServiceActivator: Service registered");

    // 3. Create BOOT_COMPLETED listener
    bootListener_ = new HelloBootCompletedListener(service_);

    // 4. Register listener for BOOT_COMPLETED event
    EventFilter filter("(type=BOOT_COMPLETED)");
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
