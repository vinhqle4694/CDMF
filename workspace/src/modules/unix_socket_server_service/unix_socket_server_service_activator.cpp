#include "unix_socket_server_service_activator.h"
#include "utils/log.h"

namespace cdmf::modules {

void UnixSocketServerServiceActivator::start(IModuleContext* context) {
    // 1. Create and start service
    service_ = std::make_unique<services::UnixSocketServerServiceImpl>();
    service_->start();

    // 2. Register service with properties
    Properties props;
    props.set("service.description", "Unix Socket Server Service");
    props.set("service.vendor", "CDMF");
    props.set("service.type", "server");
    props.set("transport.type", "unix_socket");

    registration_ = context->registerService("cdmf::IUnixSocketServerService",
                                            service_.get(),
                                            props);
    LOGI("Unix Socket Server Service Module started");
}

void UnixSocketServerServiceActivator::stop(IModuleContext* context) {
    // 1. Unregister service
    if (registration_.isValid()) {
        registration_.unregister();
    }

    // 2. Stop and cleanup
    if (service_) {
        service_->stop();
        service_.reset();
    }

    LOGI("Unix Socket Server Service Module stopped");
}

}

extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::UnixSocketServerServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
