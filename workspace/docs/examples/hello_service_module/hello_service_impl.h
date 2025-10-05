/**
 * @file hello_service_impl.h
 * @brief Hello Service Implementation
 */

#pragma once

#include "hello_service.h"
#include <mutex>
#include <atomic>

namespace cdmf {
namespace services {

/**
 * @brief Implementation of IHelloService interface
 */
class HelloServiceImpl : public IHelloService {
public:
    HelloServiceImpl();
    ~HelloServiceImpl() override;

    /**
     * @brief Start the service work (called after BOOT_COMPLETED event)
     */
    void startWork();

    /**
     * @brief Stop the service work
     */
    void stopWork();

    // IHelloService interface
    std::string greet(const std::string& name) override;
    std::string getStatus() const override;
    int getGreetingCount() const override;

private:
    mutable std::mutex mutex_;
    bool workStarted_;
    std::atomic<int> greetingCount_;
};

} // namespace services
} // namespace cdmf
