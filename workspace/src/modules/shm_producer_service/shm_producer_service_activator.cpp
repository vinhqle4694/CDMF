#include "shm_producer_service_activator.h"
#include "utils/log.h"

namespace cdmf::modules {

void ShmProducerServiceActivator::start(IModuleContext* context) {
    // 1. Create and start service
    service_ = std::make_unique<services::ShmProducerServiceImpl>();
    service_->start();

    // 2. Register service with properties
    Properties props;
    props.set("service.description", "Shared Memory Producer Service");
    props.set("service.vendor", "CDMF");
    props.set("service.type", "producer");
    props.set("shm.type", "ring_buffer");

    registration_ = context->registerService("cdmf::IShmProducerService",
                                            service_.get(),
                                            props);
    LOGI("Shared Memory Producer Service Module started");
}

void ShmProducerServiceActivator::stop(IModuleContext* context) {
    // 1. Unregister service
    if (registration_.isValid()) {
        registration_.unregister();
    }

    // 2. Stop and cleanup
    if (service_) {
        service_->stop();
        service_.reset();
    }

    LOGI("Shared Memory Producer Service Module stopped");
}

}

extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::ShmProducerServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
