/**
 * @file hello_service.h
 * @brief Hello Service Interface - Pure virtual interface for greeting functionality
 */

#pragma once

#include <string>

namespace cdmf {
namespace services {

/**
 * @brief Interface for Hello Service
 *
 * This is a simple service that provides greeting functionality.
 * It demonstrates basic service interface design in CDMF.
 */
class IHelloService {
public:
    virtual ~IHelloService() = default;

    /**
     * @brief Get a greeting message
     * @param name The name to greet
     * @return A personalized greeting message
     */
    virtual std::string greet(const std::string& name) = 0;

    /**
     * @brief Get service status
     * @return Current service status (Running/Stopped)
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief Get greeting count
     * @return Number of greetings provided since service started
     */
    virtual int getGreetingCount() const = 0;
};

} // namespace services
} // namespace cdmf
