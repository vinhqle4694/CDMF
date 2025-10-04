#ifndef CDMF_SERVICE_REFERENCE_H
#define CDMF_SERVICE_REFERENCE_H

#include "utils/properties.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <any>

namespace cdmf {

// Forward declarations
class Module;
class ServiceEntry;

/**
 * @brief Service reference
 *
 * Represents a reference to a service registered in the framework.
 * Service references are comparable and can be sorted by service ranking.
 *
 * Thread safety:
 * - Immutable after construction
 * - Safe to use across threads
 * - Service lookup operations are thread-safe via ServiceRegistry
 */
class ServiceReference {
public:
    /**
     * @brief Default constructor - creates invalid reference
     */
    ServiceReference();

    /**
     * @brief Construct from service entry
     * @param entry Pointer to service entry (must not be null)
     */
    explicit ServiceReference(std::shared_ptr<ServiceEntry> entry);

    /**
     * @brief Copy constructor
     */
    ServiceReference(const ServiceReference& other) = default;

    /**
     * @brief Copy assignment
     */
    ServiceReference& operator=(const ServiceReference& other) = default;

    /**
     * @brief Check if reference is valid
     * @return true if reference points to a registered service
     */
    bool isValid() const;

    /**
     * @brief Get service ID
     * @return Unique service ID (0 if invalid)
     */
    uint64_t getServiceId() const;

    /**
     * @brief Get service interface name
     * @return Interface name (e.g., "com.example.ILogger")
     */
    std::string getInterface() const;

    /**
     * @brief Get module that registered the service
     * @return Pointer to module, or nullptr if invalid
     */
    Module* getModule() const;

    /**
     * @brief Get service properties
     * @return Service properties (metadata)
     */
    Properties getProperties() const;

    /**
     * @brief Get a specific property value
     * @param key Property key
     * @return Property value, or empty std::any if not found
     */
    std::any getProperty(const std::string& key) const;

    /**
     * @brief Get service ranking
     *
     * Service ranking determines priority when multiple services
     * match the same interface. Higher ranking = higher priority.
     * Default ranking is 0.
     *
     * @return Service ranking (from "service.ranking" property)
     */
    int getRanking() const;

    /**
     * @brief Equality comparison
     */
    bool operator==(const ServiceReference& other) const;

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const ServiceReference& other) const;

    /**
     * @brief Less-than comparison (for sorting)
     *
     * Comparison order:
     * 1. Higher ranking first
     * 2. Lower service ID first (older services first)
     */
    bool operator<(const ServiceReference& other) const;

    /**
     * @brief Get internal service entry (for framework use)
     * @return Shared pointer to service entry
     */
    std::shared_ptr<ServiceEntry> getEntry() const { return entry_; }

private:
    std::shared_ptr<ServiceEntry> entry_;
};

} // namespace cdmf

#endif // CDMF_SERVICE_REFERENCE_H
