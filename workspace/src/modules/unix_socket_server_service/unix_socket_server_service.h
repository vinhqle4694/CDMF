#pragma once
#include <string>
#include <cstddef>

namespace cdmf::services {

/**
 * @brief Unix Socket Server Service Interface
 *
 * Provides methods for processing requests via Unix Socket IPC
 */
class IUnixSocketServerService {
public:
    virtual ~IUnixSocketServerService() = default;

    /**
     * @brief Process data received from client
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @return true if processing successful, false otherwise
     */
    virtual bool processData(const void* data, size_t size) = 0;

    /**
     * @brief Echo data back to client
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @param response Output buffer for response
     * @param response_size Size of response buffer
     * @return Number of bytes in response
     */
    virtual size_t echo(const void* data, size_t size, void* response, size_t response_size) = 0;

    /**
     * @brief Get service status
     * @return Status string ("Running", "Stopped", etc.)
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief Get server statistics
     * @return Statistics string with request count, bytes processed, etc.
     */
    virtual std::string getStatistics() const = 0;
};

}
