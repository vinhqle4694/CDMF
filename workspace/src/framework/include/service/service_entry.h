#ifndef CDMF_SERVICE_ENTRY_H
#define CDMF_SERVICE_ENTRY_H

#include "utils/properties.h"
#include <cstdint>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <any>

namespace cdmf {

// Forward declaration
class Module;

/**
 * @brief Service entry
 *
 * Internal representation of a registered service.
 * Tracks service metadata, usage count, and lifecycle.
 *
 * Thread safety:
 * - Thread-safe reference counting
 * - Properties are immutable after construction
 * - Service pointer is const after construction
 */
class ServiceEntry {
public:
    /**
     * @brief Construct service entry
     *
     * @param serviceId Unique service ID
     * @param interfaceName Service interface name
     * @param service Pointer to service implementation
     * @param props Service properties
     * @param module Module that registered the service
     */
    ServiceEntry(uint64_t serviceId,
                 const std::string& interfaceName,
                 void* service,
                 const Properties& props,
                 Module* module);

    /**
     * @brief Destructor
     */
    ~ServiceEntry() = default;

    // Prevent copying
    ServiceEntry(const ServiceEntry&) = delete;
    ServiceEntry& operator=(const ServiceEntry&) = delete;

    /**
     * @brief Get service ID
     * @return Unique service ID
     */
    uint64_t getServiceId() const { return serviceId_; }

    /**
     * @brief Get interface name
     * @return Interface name (e.g., "com.example.ILogger")
     */
    const std::string& getInterface() const { return interfaceName_; }

    /**
     * @brief Get service implementation pointer
     * @return Pointer to service object
     */
    void* getService() const { return service_; }

    /**
     * @brief Get service properties
     * @return Service properties (metadata)
     */
    const Properties& getProperties() const { return properties_; }

    /**
     * @brief Get a specific property
     * @param key Property key
     * @return Property value, or empty std::any if not found
     */
    std::any getProperty(const std::string& key) const {
        return properties_.get(key);
    }

    /**
     * @brief Get service ranking
     *
     * Ranking determines priority when multiple services match.
     * Default is 0.
     *
     * @return Service ranking (from "service.ranking" property)
     */
    int getRanking() const;

    /**
     * @brief Get owning module
     * @return Pointer to module that registered this service
     */
    Module* getModule() const { return module_; }

    /**
     * @brief Increment usage count
     *
     * Called when a module acquires this service via getService().
     *
     * @return New usage count
     */
    int incrementUsageCount();

    /**
     * @brief Decrement usage count
     *
     * Called when a module releases this service via ungetService().
     *
     * @return New usage count
     */
    int decrementUsageCount();

    /**
     * @brief Get current usage count
     * @return Number of modules currently using this service
     */
    int getUsageCount() const { return usageCount_.load(); }

    /**
     * @brief Check if service is in use
     * @return true if usage count > 0
     */
    bool isInUse() const { return usageCount_.load() > 0; }

private:
    uint64_t serviceId_;
    std::string interfaceName_;
    void* service_;
    Properties properties_;
    Module* module_;
    std::atomic<int> usageCount_;
};

} // namespace cdmf

#endif // CDMF_SERVICE_ENTRY_H
