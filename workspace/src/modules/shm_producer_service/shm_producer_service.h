#pragma once
#include <string>
#include <cstddef>

namespace cdmf::services {

/**
 * @brief Shared Memory Producer Service Interface
 *
 * Provides methods for writing data to shared memory ring buffer
 */
class IShmProducerService {
public:
    virtual ~IShmProducerService() = default;

    /**
     * @brief Write data to shared memory
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @return true if write successful, false otherwise
     */
    virtual bool write(const void* data, size_t size) = 0;

    /**
     * @brief Get the available space in the ring buffer
     * @return Number of bytes available for writing
     */
    virtual size_t getAvailableSpace() const = 0;

    /**
     * @brief Get service status
     * @return Status string ("Running", "Stopped", etc.)
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief Get shared memory statistics
     * @return Statistics string with write count, bytes written, etc.
     */
    virtual std::string getStatistics() const = 0;
};

}
