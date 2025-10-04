#include "service/service_entry.h"
#include "module/module.h"

namespace cdmf {

ServiceEntry::ServiceEntry(uint64_t serviceId,
                           const std::string& interfaceName,
                           void* service,
                           const Properties& props,
                           Module* module)
    : serviceId_(serviceId)
    , interfaceName_(interfaceName)
    , service_(service)
    , properties_(props)
    , module_(module)
    , usageCount_(0) {
}

int ServiceEntry::getRanking() const {
    std::any rankingValue = properties_.get("service.ranking");
    if (rankingValue.has_value() && rankingValue.type() == typeid(int)) {
        return std::any_cast<int>(rankingValue);
    }
    return 0; // Default ranking
}

int ServiceEntry::incrementUsageCount() {
    return ++usageCount_;
}

int ServiceEntry::decrementUsageCount() {
    int count = --usageCount_;
    if (count < 0) {
        usageCount_ = 0;
        return 0;
    }
    return count;
}

} // namespace cdmf
