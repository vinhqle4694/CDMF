#include "unix_socket_client_service_activator.h"
#include "utils/log.h"

namespace cdmf::modules {

void UnixSocketClientServiceActivator::start(IModuleContext* context) {
    // 1. Create and start service
    service_ = std::make_unique<services::UnixSocketClientServiceImpl>();
    service_->start();

    // 2. Register service with properties
    Properties props;
    props.set("service.description", "Unix Socket Client Service");
    props.set("service.vendor", "CDMF");
    props.set("service.type", "client");
    props.set("transport.type", "unix_socket");

    registration_ = context->registerService("cdmf::IUnixSocketClientService",
                                            service_.get(),
                                            props);
    LOGI("Unix Socket Client Service Module started");
}

void UnixSocketClientServiceActivator::stop(IModuleContext* context) {
    // 1. Unregister service
    if (registration_.isValid()) {
        registration_.unregister();
    }

    // 2. Stop and cleanup
    if (service_) {
        service_->stop();
        service_.reset();
    }

    LOGI("Unix Socket Client Service Module stopped");
}

}

extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::UnixSocketClientServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
