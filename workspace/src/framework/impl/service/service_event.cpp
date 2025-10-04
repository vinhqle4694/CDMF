#include "service/service_event.h"
#include "module/module.h"

namespace cdmf {

ServiceEvent::ServiceEvent(ServiceEventType type, const ServiceReference& ref)
    : Event(serviceEventTypeToString(type), ref.getModule())
    , type_(type)
    , serviceRef_(ref) {

    // Set service-specific properties
    if (ref.isValid()) {
        setProperty("service.id", static_cast<int>(ref.getServiceId()));
        setProperty("service.interface", ref.getInterface());

        if (ref.getModule()) {
            setProperty("service.module.id", static_cast<int>(ref.getModule()->getModuleId()));
            setProperty("service.module.name", ref.getModule()->getSymbolicName());
        }
    }
}

} // namespace cdmf
