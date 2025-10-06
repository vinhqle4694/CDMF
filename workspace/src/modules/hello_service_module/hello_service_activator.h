/**
 * @file hello_service_activator.h
 * @brief Module activator for Hello Service
 */

#pragma once

#include "module/module_activator.h"
#include "core/event_listener.h"
#include "hello_service_impl.h"
#include "service/service_registration.h"

namespace cdmf {
namespace modules {

/**
 * @brief Event listener for BOOT_COMPLETED event
 */
class HelloBootCompletedListener : public IEventListener {
public:
    explicit HelloBootCompletedListener(services::HelloServiceImpl* service);

    void handleEvent(const Event& event) override;

private:
    services::HelloServiceImpl* service_;
};

/**
 * @brief Module activator for Hello Service
 *
 * Manages the lifecycle of the Hello Service module:
 * 1. Creates service instance in start()
 * 2. Registers service with framework
 * 3. Listens for BOOT_COMPLETED event
 * 4. Starts actual work after BOOT_COMPLETED
 * 5. Cleans up in stop()
 */
class HelloServiceActivator : public IModuleActivator {
public:
    HelloServiceActivator();
    ~HelloServiceActivator() override;

    void start(IModuleContext* context) override;
    void stop(IModuleContext* context) override;

private:
    services::HelloServiceImpl* service_;
    HelloBootCompletedListener* bootListener_;
    ServiceRegistration registration_;
};

} // namespace modules
} // namespace cdmf

// Module factory functions
extern "C" {
    cdmf::IModuleActivator* createModuleActivator();
    void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
