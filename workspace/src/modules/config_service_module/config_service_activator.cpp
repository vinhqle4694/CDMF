/**
 * @file config_service_activator.cpp
 * @brief Module activator for Configuration Service
 */

#include "module/module_activator.h"
#include "module/module_context.h"
#include "configuration_admin.h"
#include "utils/log.h"
#include <memory>

namespace cdmf {
namespace modules {

/**
 * @brief Configuration Service Module Activator
 */
class ConfigServiceActivator : public IModuleActivator {
public:
    void start(IModuleContext* context) override {
        LOGI("Starting Configuration Service Module...");

        // Get storage directory from context properties
        std::string storageDir = context->getProperty("framework.modules.storage.dir");
        if (storageDir.empty()) {
            storageDir = "./config/store";
        }

        LOGI_FMT("  Configuration storage directory: " << storageDir);

        // Create configuration admin service
        configAdmin_ = std::make_unique<services::ConfigurationAdmin>(storageDir);

        // Register Configuration Admin service
        Properties props;
        props.set("service.description", "Configuration Administration Service");
        props.set("service.vendor", "CDMF Project");

        configAdminRegistration_ = context->registerService(
            "cdmf::IConfigurationAdmin",
            configAdmin_.get(),
            props
        );

        LOGI("Configuration Service Module started successfully");
    }

    void stop(IModuleContext* context) override {
        LOGI("Stopping Configuration Service Module...");

        // Unregister service
        if (configAdminRegistration_.isValid()) {
            configAdminRegistration_.unregister();
        }

        // Cleanup
        configAdmin_.reset();

        LOGI("Configuration Service Module stopped");
    }

private:
    std::unique_ptr<services::ConfigurationAdmin> configAdmin_;
    ServiceRegistration configAdminRegistration_;
};

} // namespace modules
} // namespace cdmf

// Module entry points
extern "C" {

cdmf::IModuleActivator* createModuleActivator() {
    return new cdmf::modules::ConfigServiceActivator();
}

void destroyModuleActivator(cdmf::IModuleActivator* activator) {
    delete activator;
}

} // extern "C"
