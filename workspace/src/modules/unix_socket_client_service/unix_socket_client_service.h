#pragma once
#include <string>
#include <cstddef>
#include <functional>

namespace cdmf::services {

/**
 * @brief Unix Socket Client Service Interface
 *
 * Provides methods for sending requests to Unix Socket server
 */
class IUnixSocketClientService {
public:
    virtual ~IUnixSocketClientService() = default;

    using ResponseCallback = std::function<void(const void*, size_t)>;

    /**
     * @brief Send data to server for processing
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @return true if send successful, false otherwise
     */
    virtual bool sendData(const void* data, size_t size) = 0;

    /**
     * @brief Send echo request to server
     * @param data Pointer to data buffer
     * @param size Size of data in bytes
     * @param response Output buffer for response
     * @param response_size Size of response buffer
     * @param bytes_received Output parameter for bytes received
     * @return true if echo successful, false otherwise
     */
    virtual bool echoRequest(const void* data, size_t size, void* response,
                            size_t response_size, size_t& bytes_received) = 0;

    /**
     * @brief Get remote server status
     * @return Server status string
     */
    virtual std::string getRemoteStatus() = 0;

    /**
     * @brief Get remote server statistics
     * @return Server statistics string
     */
    virtual std::string getRemoteStatistics() = 0;

    /**
     * @brief Get client status
     * @return Client status string
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief Get client statistics
     * @return Statistics string with request count, bytes sent, etc.
     */
    virtual std::string getStatistics() const = 0;
};

}
