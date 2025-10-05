#include "shm_consumer_service_activator.h"
#include "utils/log.h"

namespace cdmf::modules {

void ShmConsumerServiceActivator::start(IModuleContext* context) {
    // 1. Create and start service
    service_ = std::make_unique<services::ShmConsumerServiceImpl>();
    service_->start();

    // 2. Register service with properties
    Properties props;
    props.set("service.description", "Shared Memory Consumer Service");
    props.set("service.vendor", "CDMF");
    props.set("service.type", "consumer");
    props.set("shm.type", "ring_buffer");

    registration_ = context->registerService("cdmf::IShmConsumerService",
                                            service_.get(),
                                            props);
    LOGI("Shared Memory Consumer Service Module started");
}

void ShmConsumerServiceActivator::stop(IModuleContext* context) {
    // 1. Unregister service
    if (registration_.isValid()) {
        registration_.unregister();
    }

    // 2. Stop and cleanup
    if (service_) {
        service_->stop();
        service_.reset();
    }

    LOGI("Shared Memory Consumer Service Module stopped");
}

}

extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::ShmConsumerServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
