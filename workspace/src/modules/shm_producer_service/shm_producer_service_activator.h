#pragma once
#include "module/module_activator.h"
#include "module/module_context.h"
#include "shm_producer_service_impl.h"
#include <memory>

namespace cdmf::modules {

class ShmProducerServiceActivator : public IModuleActivator {
public:
    void start(IModuleContext* context) override;
    void stop(IModuleContext* context) override;

private:
    std::unique_ptr<services::ShmProducerServiceImpl> service_;
    ServiceRegistration registration_;
};

}

extern "C" {
    cdmf::IModuleActivator* createModuleActivator();
    void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
