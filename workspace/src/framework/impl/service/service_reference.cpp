#include "service/service_reference.h"
#include "service/service_entry.h"

namespace cdmf {

ServiceReference::ServiceReference()
    : entry_(nullptr) {
}

ServiceReference::ServiceReference(std::shared_ptr<ServiceEntry> entry)
    : entry_(entry) {
}

bool ServiceReference::isValid() const {
    return entry_ != nullptr;
}

uint64_t ServiceReference::getServiceId() const {
    if (!entry_) {
        return 0;
    }
    return entry_->getServiceId();
}

std::string ServiceReference::getInterface() const {
    if (!entry_) {
        return "";
    }
    return entry_->getInterface();
}

Module* ServiceReference::getModule() const {
    if (!entry_) {
        return nullptr;
    }
    return entry_->getModule();
}

Properties ServiceReference::getProperties() const {
    if (!entry_) {
        return Properties();
    }
    return entry_->getProperties();
}

std::any ServiceReference::getProperty(const std::string& key) const {
    if (!entry_) {
        return std::any();
    }
    return entry_->getProperty(key);
}

int ServiceReference::getRanking() const {
    if (!entry_) {
        return 0;
    }
    return entry_->getRanking();
}

bool ServiceReference::operator==(const ServiceReference& other) const {
    return entry_ == other.entry_;
}

bool ServiceReference::operator!=(const ServiceReference& other) const {
    return !(*this == other);
}

bool ServiceReference::operator<(const ServiceReference& other) const {
    // Invalid references are always "greater" (sorted to end)
    if (!entry_) return false;
    if (!other.entry_) return true;

    // Compare by ranking first (higher ranking = higher priority = "less")
    int thisRanking = getRanking();
    int otherRanking = other.getRanking();

    if (thisRanking != otherRanking) {
        return thisRanking > otherRanking; // Higher ranking comes first
    }

    // Same ranking: compare by service ID (lower ID = older = higher priority)
    return getServiceId() < other.getServiceId();
}

} // namespace cdmf
