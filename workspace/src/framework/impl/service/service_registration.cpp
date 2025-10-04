#include "service/service_registration.h"
#include "service/service_registry.h"
#include "service/service_entry.h"
#include <stdexcept>

namespace cdmf {

ServiceRegistration::ServiceRegistration()
    : entry_(nullptr)
    , registry_(nullptr) {
}

ServiceRegistration::ServiceRegistration(std::shared_ptr<ServiceEntry> entry,
                                         ServiceRegistry* registry)
    : entry_(entry)
    , registry_(registry) {
}

bool ServiceRegistration::isValid() const {
    return entry_ != nullptr && registry_ != nullptr;
}

uint64_t ServiceRegistration::getServiceId() const {
    if (!entry_) {
        return 0;
    }
    return entry_->getServiceId();
}

ServiceReference ServiceRegistration::getReference() const {
    if (!entry_) {
        return ServiceReference();
    }
    return ServiceReference(entry_);
}

void ServiceRegistration::setProperties(const Properties& props) {
    if (!isValid()) {
        throw std::runtime_error("Cannot set properties on invalid registration");
    }

    // Delegate to registry
    registry_->setServiceProperties(entry_->getServiceId(), props);
}

void ServiceRegistration::unregister() {
    if (!isValid()) {
        return; // Already unregistered or invalid
    }

    // Delegate to registry
    registry_->unregisterService(entry_->getServiceId());

    // Invalidate this registration
    entry_.reset();
    registry_ = nullptr;
}

bool ServiceRegistration::operator==(const ServiceRegistration& other) const {
    return entry_ == other.entry_ && registry_ == other.registry_;
}

bool ServiceRegistration::operator!=(const ServiceRegistration& other) const {
    return !(*this == other);
}

} // namespace cdmf
