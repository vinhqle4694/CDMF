#pragma once
#include <string>
#include <cstddef>
#include <functional>

namespace cdmf::services {

/**
 * @brief Shared Memory Consumer Service Interface
 *
 * Provides methods for reading data from shared memory ring buffer
 */
class IShmConsumerService {
public:
    virtual ~IShmConsumerService() = default;

    /**
     * @brief Callback function type for data received
     * @param data Pointer to received data
     * @param size Size of received data in bytes
     */
    using DataCallback = std::function<void(const void* data, size_t size)>;

    /**
     * @brief Read data from shared memory
     * @param buffer Buffer to store read data
     * @param buffer_size Size of the buffer
     * @param bytes_read Output parameter for actual bytes read
     * @return true if read successful, false otherwise
     */
    virtual bool read(void* buffer, size_t buffer_size, size_t& bytes_read) = 0;

    /**
     * @brief Register a callback for when data is available
     * @param callback Function to call when data is received
     */
    virtual void setDataCallback(DataCallback callback) = 0;

    /**
     * @brief Get the available data size in the ring buffer
     * @return Number of bytes available for reading
     */
    virtual size_t getAvailableData() const = 0;

    /**
     * @brief Get service status
     * @return Status string ("Running", "Stopped", etc.)
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief Get shared memory statistics
     * @return Statistics string with read count, bytes read, etc.
     */
    virtual std::string getStatistics() const = 0;
};

}
