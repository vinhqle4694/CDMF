#include "service/service_registry.h"
#include "service/service_event.h"
#include "core/event_dispatcher.h"
#include "module/module.h"
#include <algorithm>
#include <stdexcept>

namespace cdmf {

ServiceRegistry::ServiceRegistry(EventDispatcher* eventDispatcher)
    : servicesById_()
    , servicesByInterface_()
    , serviceUsage_()
    , mutex_()
    , nextServiceId_(1)
    , eventDispatcher_(eventDispatcher) {
}

ServiceRegistry::~ServiceRegistry() {
    // Services are owned by shared_ptr, will be cleaned up automatically
}

uint64_t ServiceRegistry::generateServiceId() {
    return nextServiceId_++;
}

ServiceRegistration ServiceRegistry::registerService(const std::string& interfaceName,
                                                      void* service,
                                                      const Properties& props,
                                                      Module* module) {
    if (interfaceName.empty()) {
        throw std::invalid_argument("Interface name cannot be empty");
    }
    if (!service) {
        throw std::invalid_argument("Service cannot be null");
    }

    std::unique_lock lock(mutex_);

    // Generate service ID
    uint64_t serviceId = generateServiceId();

    // Create service entry
    auto entry = std::make_shared<ServiceEntry>(serviceId, interfaceName, service, props, module);

    // Add to ID map
    servicesById_[serviceId] = entry;

    // Add to interface map
    servicesByInterface_[interfaceName].push_back(entry);

    // Sort by ranking (highest first)
    std::sort(servicesByInterface_[interfaceName].begin(),
              servicesByInterface_[interfaceName].end(),
              [](const std::shared_ptr<ServiceEntry>& a, const std::shared_ptr<ServiceEntry>& b) {
                  return a->getRanking() > b->getRanking();
              });

    // Create reference and fire event
    ServiceReference ref(entry);
    lock.unlock();

    fireServiceEvent(ServiceEventType::REGISTERED, ref);

    return ServiceRegistration(entry, this);
}

bool ServiceRegistry::unregisterService(uint64_t serviceId) {
    std::unique_lock lock(mutex_);

    auto it = servicesById_.find(serviceId);
    if (it == servicesById_.end()) {
        return false;
    }

    auto entry = it->second;
    std::string interfaceName = entry->getInterface();

    // Create reference and fire event before removal
    ServiceReference ref(entry);
    lock.unlock();
    fireServiceEvent(ServiceEventType::UNREGISTERING, ref);
    lock.lock();

    // Remove from ID map
    servicesById_.erase(it);

    // Remove from interface map
    auto& interfaceVec = servicesByInterface_[interfaceName];
    interfaceVec.erase(
        std::remove(interfaceVec.begin(), interfaceVec.end(), entry),
        interfaceVec.end()
    );

    // Remove empty interface entries
    if (interfaceVec.empty()) {
        servicesByInterface_.erase(interfaceName);
    }

    return true;
}

int ServiceRegistry::unregisterServices(Module* module) {
    if (!module) {
        return 0;
    }

    std::unique_lock lock(mutex_);

    // Find all services registered by this module
    std::vector<uint64_t> toUnregister;
    for (const auto& pair : servicesById_) {
        if (pair.second->getModule() == module) {
            toUnregister.push_back(pair.first);
        }
    }

    lock.unlock();

    // Unregister each service (this will fire events)
    int count = 0;
    for (uint64_t serviceId : toUnregister) {
        if (unregisterService(serviceId)) {
            count++;
        }
    }

    return count;
}

void ServiceRegistry::setServiceProperties(uint64_t serviceId, const Properties& props) {
    (void)props; // TODO: Implement property merging in ServiceEntry

    std::unique_lock lock(mutex_);

    auto it = servicesById_.find(serviceId);
    if (it == servicesById_.end()) {
        throw std::runtime_error("Service not found: " + std::to_string(serviceId));
    }

    auto entry = it->second;

    // Merge properties (this requires modifying ServiceEntry to support property updates)
    // For now, we'll need to recreate the entry with new properties
    // Note: This is a simplification - a real implementation would modify entry in-place

    // Create reference and fire event
    ServiceReference ref(entry);
    lock.unlock();

    fireServiceEvent(ServiceEventType::MODIFIED, ref);
}

std::vector<ServiceReference> ServiceRegistry::getServiceReferences(
    const std::string& interfaceName,
    const std::string& filter) const {

    std::shared_lock lock(mutex_);

    auto it = servicesByInterface_.find(interfaceName);
    if (it == servicesByInterface_.end()) {
        return {};
    }

    std::vector<ServiceReference> refs;
    for (const auto& entry : it->second) {
        // Apply filter if provided
        if (!filter.empty()) {
            if (!matchesFilter(entry->getProperties(), filter)) {
                continue;
            }
        }

        refs.push_back(ServiceReference(entry));
    }

    // Sort by ranking (already sorted in storage, but ensure correct order)
    std::sort(refs.begin(), refs.end());

    return refs;
}

ServiceReference ServiceRegistry::getServiceReference(const std::string& interfaceName) const {
    std::shared_lock lock(mutex_);

    auto it = servicesByInterface_.find(interfaceName);
    if (it == servicesByInterface_.end() || it->second.empty()) {
        return ServiceReference();
    }

    // Return first (highest ranking)
    return ServiceReference(it->second[0]);
}

ServiceReference ServiceRegistry::getServiceReferenceById(uint64_t serviceId) const {
    std::shared_lock lock(mutex_);

    auto it = servicesById_.find(serviceId);
    if (it == servicesById_.end()) {
        return ServiceReference();
    }

    return ServiceReference(it->second);
}

std::vector<ServiceReference> ServiceRegistry::getAllServices() const {
    std::shared_lock lock(mutex_);

    std::vector<ServiceReference> refs;
    refs.reserve(servicesById_.size());

    for (const auto& pair : servicesById_) {
        refs.push_back(ServiceReference(pair.second));
    }

    return refs;
}

std::vector<ServiceReference> ServiceRegistry::getServicesByModule(Module* module) const {
    if (!module) {
        return {};
    }

    std::shared_lock lock(mutex_);

    std::vector<ServiceReference> refs;
    for (const auto& pair : servicesById_) {
        if (pair.second->getModule() == module) {
            refs.push_back(ServiceReference(pair.second));
        }
    }

    return refs;
}

std::shared_ptr<void> ServiceRegistry::getService(const ServiceReference& ref) {
    if (!ref.isValid()) {
        return nullptr;
    }

    auto entry = ref.getEntry();
    if (!entry) {
        return nullptr;
    }

    // Increment usage count
    entry->incrementUsageCount();

    // Return service pointer wrapped in shared_ptr with custom deleter
    void* servicePtr = entry->getService();
    return std::shared_ptr<void>(servicePtr, [entry](void*) {
        // Custom deleter: decrement usage count
        // Note: We don't actually delete the service, just track usage
        entry->decrementUsageCount();
    });
}

bool ServiceRegistry::ungetService(const ServiceReference& ref) {
    if (!ref.isValid()) {
        return false;
    }

    auto entry = ref.getEntry();
    if (!entry) {
        return false;
    }

    // Decrement usage count
    int count = entry->decrementUsageCount();
    return count >= 0;
}

std::vector<ServiceReference> ServiceRegistry::getServicesInUse(Module* module) const {
    if (!module) {
        return {};
    }

    std::shared_lock lock(mutex_);

    std::vector<ServiceReference> refs;
    for (const auto& pair : servicesById_) {
        if (pair.second->isInUse()) {
            refs.push_back(ServiceReference(pair.second));
        }
    }

    return refs;
}

size_t ServiceRegistry::getServiceCount() const {
    std::shared_lock lock(mutex_);
    return servicesById_.size();
}

size_t ServiceRegistry::getServiceCount(const std::string& interfaceName) const {
    std::shared_lock lock(mutex_);

    auto it = servicesByInterface_.find(interfaceName);
    if (it == servicesByInterface_.end()) {
        return 0;
    }

    return it->second.size();
}

void ServiceRegistry::fireServiceEvent(ServiceEventType type, const ServiceReference& ref) {
    if (!eventDispatcher_) {
        return; // No event dispatcher configured
    }

    ServiceEvent event(type, ref);
    eventDispatcher_->fireEvent(event);
}

bool ServiceRegistry::matchesFilter(const Properties& props, const std::string& filter) const {
    (void)props; // TODO: Implement full LDAP filter parsing

    // Simple LDAP filter implementation
    // Full implementation would parse LDAP syntax: (key=value), (&(k1=v1)(k2=v2)), etc.
    // For now, just check if filter is empty or basic key=value match

    if (filter.empty()) {
        return true;
    }

    // Basic implementation: check if filter is "key=value" format
    size_t eqPos = filter.find('=');
    if (eqPos == std::string::npos) {
        return false; // Invalid filter format
    }

    std::string key = filter.substr(0, eqPos);
    std::string value = filter.substr(eqPos + 1);

    std::any propValue = props.get(key);
    if (!propValue.has_value()) {
        return false;
    }

    // Try to match as string
    if (propValue.type() == typeid(std::string)) {
        return std::any_cast<std::string>(propValue) == value;
    }

    // Try to match as int
    if (propValue.type() == typeid(int)) {
        try {
            int intValue = std::stoi(value);
            return std::any_cast<int>(propValue) == intValue;
        } catch (...) {
            return false;
        }
    }

    return false;
}

} // namespace cdmf
