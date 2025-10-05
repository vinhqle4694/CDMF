/**
 * @file consumer_activator.cpp
 * @brief Module activator for Consumer Module that uses Calculator Service
 */

#include "module/module_activator.h"
#include "module/module_context.h"
#include "service/service_tracker.h"
#include "utils/log.h"
#include <memory>
#include <thread>
#include <chrono>

// Forward declaration of calculator service interface
namespace cdmf {
class ICalculatorService {
public:
    virtual ~ICalculatorService() = default;
    virtual int32_t add(int32_t a, int32_t b) = 0;
    virtual int32_t subtract(int32_t a, int32_t b) = 0;
    virtual int32_t multiply(int32_t a, int32_t b) = 0;
    virtual int32_t divide(int32_t a, int32_t b) = 0;
};
}

namespace cdmf {
namespace modules {

/**
 * @brief Service tracker customizer for Calculator Service
 */
class CalculatorCustomizer : public IServiceTrackerCustomizer<ICalculatorService> {
public:
    CalculatorCustomizer(IModuleContext* context) : context_(context) {}

    ICalculatorService* addingService(const ServiceReference& ref) override {
        LOGI("Consumer: Calculator service added, running demo calculations...");
        auto servicePtr = context_->getService(ref);
        auto calc = static_cast<ICalculatorService*>(servicePtr.get());

        if (calc) {
            performCalculations(calc);
        }

        return calc;
    }

    void modifiedService(const ServiceReference&, ICalculatorService*) override {
        LOGI("Consumer: Calculator service modified");
    }

    void removedService(const ServiceReference& ref, ICalculatorService*) override {
        LOGI("Consumer: Calculator service removed");
        context_->ungetService(ref);
    }

private:
    void performCalculations(ICalculatorService* calc) {
        try {
            LOGI("===== Consumer Module - Demo Calculations =====");

            // Test addition
            int32_t result1 = calc->add(10, 20);
            LOGI_FMT("Consumer: 10 + 20 = " << result1);

            // Test subtraction
            int32_t result2 = calc->subtract(50, 30);
            LOGI_FMT("Consumer: 50 - 30 = " << result2);

            // Test multiplication
            int32_t result3 = calc->multiply(6, 7);
            LOGI_FMT("Consumer: 6 * 7 = " << result3);

            // Test division
            int32_t result4 = calc->divide(100, 5);
            LOGI_FMT("Consumer: 100 / 5 = " << result4);

            // Test with different numbers
            int32_t sum = calc->add(123, 456);
            LOGI_FMT("Consumer: 123 + 456 = " << sum);

            LOGI("===== Consumer Module - Demo Complete =====");

        } catch (const std::exception& e) {
            LOGE_FMT("Consumer: Error performing calculations: " << e.what());
        }
    }

    IModuleContext* context_;
};

/**
 * @brief Consumer Module Activator that uses Calculator Service
 */
class ConsumerActivator : public IModuleActivator {
public:
    void start(IModuleContext* context) override {
        LOGI("Starting Consumer Module...");

        context_ = context;

        // Get calculator service reference
        std::vector<ServiceReference> refs = context->getServiceReferences("cdmf::ICalculatorService");

        if (!refs.empty()) {
            LOGI_FMT("Consumer: Found " << refs.size() << " calculator service(s)");

            // Get the service
            auto servicePtr = context->getService(refs[0]);
            auto calc = static_cast<ICalculatorService*>(servicePtr.get());

            if (calc) {
                performCalculations(calc);
            } else {
                LOGE("Consumer: Failed to get calculator service");
            }

            context->ungetService(refs[0]);
        } else {
            LOGW("Consumer: Calculator service not found (will wait for it to be registered)");
        }

        LOGI("Consumer Module started successfully");
    }

    void stop(IModuleContext* context) override {
        LOGI("Stopping Consumer Module...");
        LOGI("Consumer Module stopped");
    }

private:
    void performCalculations(ICalculatorService* calc) {
        try {
            LOGI("===== Consumer Module - Demo Calculations =====");

            // Test addition
            int32_t result1 = calc->add(10, 20);
            LOGI_FMT("Consumer: 10 + 20 = " << result1);

            // Test subtraction
            int32_t result2 = calc->subtract(50, 30);
            LOGI_FMT("Consumer: 50 - 30 = " << result2);

            // Test multiplication
            int32_t result3 = calc->multiply(6, 7);
            LOGI_FMT("Consumer: 6 * 7 = " << result3);

            // Test division
            int32_t result4 = calc->divide(100, 5);
            LOGI_FMT("Consumer: 100 / 5 = " << result4);

            // Test with different numbers
            int32_t sum = calc->add(123, 456);
            LOGI_FMT("Consumer: 123 + 456 = " << sum);

            LOGI("===== Consumer Module - Demo Complete =====");

        } catch (const std::exception& e) {
            LOGE_FMT("Consumer: Error performing calculations: " << e.what());
        }
    }

    IModuleContext* context_;
};

} // namespace modules
} // namespace cdmf

// Module entry points
extern "C" {

cdmf::IModuleActivator* createModuleActivator() {
    return new cdmf::modules::ConsumerActivator();
}

void destroyModuleActivator(cdmf::IModuleActivator* activator) {
    delete activator;
}

} // extern "C"
