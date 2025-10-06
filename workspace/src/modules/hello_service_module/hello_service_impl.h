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
    /**
     * @brief Constructor
     * @param logFilePath Path to greeting log file (read from manifest)
     * @param moduleId Module symbolic name for permission checks (read from manifest)
     */
    explicit HelloServiceImpl(const std::string& logFilePath = "/tmp/hello_service_greetings.log",
                             const std::string& moduleId = "cdmf.hello_service");
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
    /**
     * @brief Write greeting to log file (demonstrates FILE_WRITE permission)
     * @param greeting The greeting message to log
     */
    void writeGreetingToFile(const std::string& greeting);

    mutable std::mutex mutex_;
    bool workStarted_;
    std::atomic<int> greetingCount_;
    std::string logFilePath_;
    std::string moduleId_;
};

} // namespace services
} // namespace cdmf
