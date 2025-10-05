/**
 * @file calculator_activator.cpp
 * @brief Module activator for Calculator Service
 */

#include "module/module_activator.h"
#include "module/module_context.h"
#include "calculator_service.h"
#include "utils/log.h"
#include <memory>

namespace cdmf {
namespace modules {

/**
 * @brief Calculator Service Module Activator
 */
class CalculatorActivator : public IModuleActivator {
public:
    void start(IModuleContext* context) override {
        LOGI("Starting Calculator Service Module...");

        // Create calculator service
        calculatorService_ = std::make_unique<services::CalculatorService>();

        // Register Calculator service
        Properties props;
        props.set("service.description", "Calculator Service");
        props.set("service.vendor", "CDMF Project");
        props.set("service.version", "1.0.0");

        calculatorRegistration_ = context->registerService(
            "cdmf::ICalculatorService",
            calculatorService_.get(),
            props
        );

        LOGI("Calculator Service Module started successfully");
    }

    void stop(IModuleContext* context) override {
        LOGI("Stopping Calculator Service Module...");

        // Unregister service
        if (calculatorRegistration_.isValid()) {
            calculatorRegistration_.unregister();
        }

        // Cleanup
        calculatorService_.reset();

        LOGI("Calculator Service Module stopped");
    }

private:
    std::unique_ptr<services::CalculatorService> calculatorService_;
    ServiceRegistration calculatorRegistration_;
};

} // namespace modules
} // namespace cdmf

// Module entry points
extern "C" {

cdmf::IModuleActivator* createModuleActivator() {
    return new cdmf::modules::CalculatorActivator();
}

void destroyModuleActivator(cdmf::IModuleActivator* activator) {
    delete activator;
}

} // extern "C"
