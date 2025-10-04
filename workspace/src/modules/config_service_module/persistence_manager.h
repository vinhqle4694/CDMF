#ifndef CDMF_SERVICES_CONFIG_SERVICE_PERSISTENCE_MANAGER_H
#define CDMF_SERVICES_CONFIG_SERVICE_PERSISTENCE_MANAGER_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "utils/properties.h"

namespace cdmf {
namespace services {

/**
 * @class PersistenceManager
 * @brief Manages persistent storage of configuration properties
 *
 * This class handles loading and saving configuration properties
 * to/from the file system.
 */
class PersistenceManager {
public:
    /**
     * @brief Constructor
     * @param storageDir Directory where configuration files are stored
     */
    explicit PersistenceManager(const std::string& storageDir);

    /**
     * @brief Destructor
     */
    ~PersistenceManager();

    /**
     * @brief Load configuration properties from storage
     * @param pid Persistent identifier
     * @return Loaded properties
     */
    cdmf::Properties load(const std::string& pid);

    /**
     * @brief Save configuration properties to storage
     * @param pid Persistent identifier
     * @param properties Properties to save
     */
    void save(const std::string& pid, const cdmf::Properties& properties);

    /**
     * @brief Remove configuration from storage
     * @param pid Persistent identifier
     */
    void remove(const std::string& pid);

    /**
     * @brief List all configuration PIDs in storage
     * @return Vector of PIDs
     */
    std::vector<std::string> listAll();

private:
    /**
     * @brief Get the file path for a configuration
     * @param pid Persistent identifier
     * @return Full file path
     */
    std::string getFilePath(const std::string& pid) const;

    /**
     * @brief Ensure storage directory exists
     */
    void ensureStorageDirectory();

    std::string storageDir_;  ///< Directory for storing configuration files
};

} // namespace services
} // namespace cdmf

#endif // CDMF_SERVICES_CONFIG_SERVICE_PERSISTENCE_MANAGER_H
