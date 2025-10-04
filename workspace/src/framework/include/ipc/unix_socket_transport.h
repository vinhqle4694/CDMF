/**
 * @file unix_socket_transport.h
 * @brief Unix domain socket transport implementation
 *
 * Provides IPC transport using Unix domain sockets (AF_UNIX).
 * Supports both SOCK_STREAM (connection-oriented) and SOCK_DGRAM
 * (connectionless) modes with non-blocking I/O.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Transport Agent
 */

#ifndef CDMF_IPC_UNIX_SOCKET_TRANSPORT_H
#define CDMF_IPC_UNIX_SOCKET_TRANSPORT_H

#include "transport.h"
#include "serializer.h"
#include <sys/un.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <string>

namespace cdmf {
namespace ipc {

/**
 * @brief Unix socket type
 */
enum class UnixSocketType {
    /** Stream socket (SOCK_STREAM) - connection-oriented, reliable */
    STREAM,
    /** Datagram socket (SOCK_DGRAM) - connectionless, unreliable */
    DGRAM
};

/**
 * @brief Unix socket specific configuration
 */
struct UnixSocketConfig {
    /** Socket type (STREAM or DGRAM) */
    UnixSocketType socket_type = UnixSocketType::STREAM;

    /** Socket path (must be unique) */
    std::string socket_path;

    /** Enable SO_REUSEADDR */
    bool reuse_addr = true;

    /** Server mode (listen for connections) */
    bool is_server = false;

    /** Accept backlog for server (STREAM only) */
    int backlog = 128;

    /** Enable TCP_NODELAY equivalent (immediate send) */
    bool no_delay = true;

    /** Send buffer size (SO_SNDBUF) */
    int send_buffer_size = 2097152; // 2 MB

    /** Receive buffer size (SO_RCVBUF) */
    int recv_buffer_size = 2097152; // 2 MB

    /** Use epoll for I/O multiplexing (server mode) */
    bool use_epoll = true;

    /** Maximum number of concurrent connections (server mode) */
    int max_connections = 100;

    /** Credentials passing (SCM_CREDENTIALS) */
    bool enable_credentials = false;

    UnixSocketConfig() = default;
};

/**
 * @brief Unix domain socket transport implementation
 */
class UnixSocketTransport : public ITransport {
public:
    /**
     * @brief Constructor
     */
    UnixSocketTransport();

    /**
     * @brief Destructor
     */
    ~UnixSocketTransport() override;

    // ITransport interface implementation

    TransportResult<bool> init(const TransportConfig& config) override;
    TransportResult<bool> start() override;
    TransportResult<bool> stop() override;
    TransportResult<bool> cleanup() override;

    TransportResult<bool> connect() override;
    TransportResult<bool> disconnect() override;
    bool isConnected() const override;

    TransportResult<bool> send(const Message& message) override;
    TransportResult<bool> send(Message&& message) override;
    TransportResult<MessagePtr> receive(int32_t timeout_ms = 0) override;
    TransportResult<MessagePtr> tryReceive() override;

    void setMessageCallback(MessageCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;
    void setStateChangeCallback(StateChangeCallback callback) override;

    TransportState getState() const override;
    TransportType getType() const override;
    const TransportConfig& getConfig() const override;
    TransportStats getStats() const override;
    void resetStats() override;
    std::pair<TransportError, std::string> getLastError() const override;
    std::string getInfo() const override;

    // Unix socket specific methods

    /**
     * @brief Get Unix socket configuration
     * @return Unix socket config
     */
    const UnixSocketConfig& getUnixConfig() const { return unix_config_; }

    /**
     * @brief Get socket file descriptor
     * @return Socket fd (-1 if not connected)
     */
    int getSocketFd() const { return socket_fd_; }

private:
    // Configuration
    TransportConfig config_;
    UnixSocketConfig unix_config_;
    SerializerPtr serializer_;

    // State
    std::atomic<TransportState> state_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;

    // Socket file descriptors
    int socket_fd_;           // Main socket (server listen socket or client socket)
    int client_fd_;           // Connected client socket (client mode)
    int epoll_fd_;            // Epoll fd (server mode)

    // Server mode: client connections
    struct ClientConnection {
        int fd;
        std::string address;
        std::chrono::system_clock::time_point connected_at;
    };
    std::vector<ClientConnection> clients_;
    mutable std::mutex clients_mutex_;

    // Request-to-client routing map (message_id -> client_fd)
    std::unordered_map<std::string, int> request_routing_;
    mutable std::mutex routing_mutex_;

    // I/O thread (async mode)
    std::unique_ptr<std::thread> io_thread_;

    // Callbacks
    MessageCallback message_callback_;
    ErrorCallback error_callback_;
    StateChangeCallback state_callback_;

    // Statistics
    mutable std::mutex stats_mutex_;
    TransportStats stats_;

    // Last error
    mutable std::mutex error_mutex_;
    TransportError last_error_;
    std::string last_error_msg_;

    // Send mutex to serialize concurrent sends
    mutable std::mutex send_mutex_;

    // Helper methods
    TransportResult<bool> createSocket();
    TransportResult<bool> bindSocket();
    TransportResult<bool> listenSocket();
    TransportResult<bool> connectSocket();
    TransportResult<bool> configureSocket(int fd);
    TransportResult<bool> setupEpoll();

    TransportResult<bool> sendToSocket(int fd, const Message& message);
    TransportResult<MessagePtr> receiveFromSocket(int fd, int32_t timeout_ms);

    void ioThreadFunc();
    void handleEpollEvents();
    void acceptNewConnection();
    void handleClientData(int fd);
    void removeClient(int fd);

    void setState(TransportState new_state);
    void setError(TransportError error, const std::string& message);
    void updateStats(bool is_send, uint32_t bytes, bool success);

    static bool setNonBlocking(int fd);
    static bool setSocketOptions(int fd, const UnixSocketConfig& config);
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_UNIX_SOCKET_TRANSPORT_H
