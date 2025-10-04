#include "configuration_event.h"

namespace cdmf {
namespace services {

ConfigurationEvent::ConfigurationEvent(ConfigurationEventType type, const std::string& pid)
    : type_(type), pid_(pid), factoryPid_(), reference_(nullptr) {
}

ConfigurationEvent::ConfigurationEvent(ConfigurationEventType type, const std::string& pid,
                                       const std::string& factoryPid)
    : type_(type), pid_(pid), factoryPid_(factoryPid), reference_(nullptr) {
}

ConfigurationEvent::ConfigurationEvent(ConfigurationEventType type, const std::string& pid,
                                       Configuration* reference)
    : type_(type), pid_(pid), factoryPid_(), reference_(reference) {
}

} // namespace services
} // namespace cdmf
