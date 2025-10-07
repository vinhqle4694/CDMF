/**
 * @file unix_socket_transport.cpp
 * @brief Unix domain socket transport implementation
 *
 * @version 1.0.0
 * @date 2025-10-03
 */

#include "ipc/unix_socket_transport.h"
#include "ipc/serializer.h"
#include "utils/log.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

namespace cdmf {
namespace ipc {

UnixSocketTransport::UnixSocketTransport()
    : state_(TransportState::UNINITIALIZED)
    , running_(false)
    , connected_(false)
    , socket_fd_(-1)
    , client_fd_(-1)
    , epoll_fd_(-1)
    , last_error_(TransportError::SUCCESS) {
}

UnixSocketTransport::~UnixSocketTransport() {
    cleanup();
}

TransportResult<bool> UnixSocketTransport::init(const TransportConfig& config) {
    if (state_ != TransportState::UNINITIALIZED) {
        LOGW_FMT("Transport already initialized");
        return {TransportError::ALREADY_INITIALIZED, false, "Transport already initialized"};
    }

    LOGI_FMT("Initializing UnixSocketTransport with endpoint: " << config.endpoint);
    config_ = config;

    // Parse Unix socket specific config from properties
    auto it = config.properties.find("socket_type");
    if (it != config.properties.end()) {
        unix_config_.socket_type = (it->second == "DGRAM") ?
            UnixSocketType::DGRAM : UnixSocketType::STREAM;
    }

    unix_config_.socket_path = config.endpoint;

    it = config.properties.find("is_server");
    if (it != config.properties.end()) {
        unix_config_.is_server = (it->second == "true" || it->second == "1");
    }

    it = config.properties.find("max_connections");
    if (it != config.properties.end()) {
        unix_config_.max_connections = std::stoi(it->second);
    }

    it = config.properties.find("use_epoll");
    if (it != config.properties.end()) {
        unix_config_.use_epoll = (it->second == "true" || it->second == "1");
    }

    // Create serializer
    serializer_ = SerializerFactory::createSerializer(SerializationFormat::BINARY);
    if (!serializer_) {
        LOGE_FMT("Failed to create serializer");
        return {TransportError::INVALID_CONFIG, false, "Failed to create serializer"};
    }

    LOGI_FMT("UnixSocketTransport initialized successfully - Type: "
             << (unix_config_.socket_type == UnixSocketType::STREAM ? "STREAM" : "DGRAM")
             << ", Server: " << (unix_config_.is_server ? "true" : "false"));
    setState(TransportState::INITIALIZED);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::start() {
    if (state_ != TransportState::INITIALIZED && state_ != TransportState::DISCONNECTED) {
        LOGW_FMT("Transport not initialized");
        return {TransportError::NOT_INITIALIZED, false, "Transport not initialized"};
    }

    LOGI_FMT("Starting UnixSocketTransport - Mode: " << (unix_config_.is_server ? "SERVER" : "CLIENT"));
    auto result = createSocket();
    if (!result) {
        return result;
    }

    if (unix_config_.is_server) {
        result = bindSocket();
        if (!result) return result;

        if (unix_config_.socket_type == UnixSocketType::STREAM) {
            result = listenSocket();
            if (!result) return result;

            if (unix_config_.use_epoll) {
                result = setupEpoll();
                if (!result) return result;
            }
        }

        // Start I/O thread for server
        if (config_.mode == TransportMode::ASYNC) {
            running_ = true;
            io_thread_ = std::make_unique<std::thread>(&UnixSocketTransport::ioThreadFunc, this);
        }

        setState(TransportState::CONNECTED);
        connected_ = true;
        LOGI_FMT("UnixSocketTransport started successfully in SERVER mode");
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::stop() {
    if (!running_ && !connected_) {
        return {TransportError::SUCCESS, true, ""};
    }

    LOGI_FMT("Stopping UnixSocketTransport");
    running_ = false;

    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }

    disconnect();

    setState(TransportState::DISCONNECTED);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::cleanup() {
    LOGI_FMT("Cleaning up UnixSocketTransport");
    stop();

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_) {
            close(client.fd);
        }
        clients_.clear();
    }

    // Close sockets
    if (client_fd_ >= 0) {
        close(client_fd_);
        client_fd_ = -1;
    }

    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }

    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }

    // Remove socket file
    if (unix_config_.is_server && !unix_config_.socket_path.empty()) {
        unlink(unix_config_.socket_path.c_str());
    }

    setState(TransportState::UNINITIALIZED);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::connect() {
    if (connected_) {
        LOGW_FMT("Already connected");
        return {TransportError::ALREADY_CONNECTED, false, "Already connected"};
    }

    if (unix_config_.is_server) {
        LOGE_FMT("Server mode does not connect");
        return {TransportError::INVALID_CONFIG, false, "Server mode does not connect"};
    }

    LOGI_FMT("Connecting to socket: " << unix_config_.socket_path);
    setState(TransportState::CONNECTING);

    if (socket_fd_ < 0) {
        auto result = createSocket();
        if (!result) return result;
    }

    auto result = connectSocket();
    if (!result) {
        setState(TransportState::DISCONNECTED);
        return result;
    }

    client_fd_ = socket_fd_;
    connected_ = true;
    setState(TransportState::CONNECTED);
    LOGI_FMT("Connected successfully to " << unix_config_.socket_path);

    // Start I/O thread for async mode
    if (config_.mode == TransportMode::ASYNC) {
        running_ = true;
        io_thread_ = std::make_unique<std::thread>(&UnixSocketTransport::ioThreadFunc, this);
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::disconnect() {
    if (!connected_) {
        return {TransportError::SUCCESS, true, ""};
    }

    LOGI_FMT("Disconnecting from socket");
    setState(TransportState::DISCONNECTING);

    if (client_fd_ >= 0) {
        shutdown(client_fd_, SHUT_RDWR);
        close(client_fd_);
        client_fd_ = -1;
    }

    connected_ = false;
    setState(TransportState::DISCONNECTED);

    return {TransportError::SUCCESS, true, ""};
}

bool UnixSocketTransport::isConnected() const {
    return connected_;
}

TransportResult<bool> UnixSocketTransport::send(const Message& message) {
    if (!connected_ && !unix_config_.is_server) {
        LOGW_FMT("Cannot send: not connected");
        return {TransportError::NOT_CONNECTED, false, "Not connected"};
    }

    int target_fd = unix_config_.is_server ? socket_fd_ : client_fd_;

    // For server STREAM mode with RESPONSE/ERROR messages, send to specific client
    if (unix_config_.is_server && unix_config_.socket_type == UnixSocketType::STREAM) {
        if (message.getType() == MessageType::RESPONSE || message.getType() == MessageType::ERROR) {
            // Find the client FD using correlation_id (which matches the request message_id)
            uint8_t corr_id[16];
            message.getCorrelationId(corr_id);

            std::ostringstream oss;
            for (int i = 0; i < 16; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(corr_id[i]);
            }
            std::string corr_id_str = oss.str();

            int client_fd = -1;
            {
                std::lock_guard<std::mutex> lock(routing_mutex_);
                auto it = request_routing_.find(corr_id_str);
                if (it != request_routing_.end()) {
                    client_fd = it->second;
                    // Remove routing entry after use
                    request_routing_.erase(it);
                }
            }

            if (client_fd >= 0) {
                return sendToSocket(client_fd, message);
            } else {
                return {TransportError::SEND_FAILED, false, "No routing info for response"};
            }
        }

        // For other message types (REQUEST, EVENT, etc.), broadcast to all clients
        std::lock_guard<std::mutex> lock(clients_mutex_);
        bool all_success = true;
        for (const auto& client : clients_) {
            auto result = sendToSocket(client.fd, message);
            if (!result) {
                all_success = false;
            }
        }
        return {all_success ? TransportError::SUCCESS : TransportError::SEND_FAILED,
                all_success, ""};
    }

    return sendToSocket(target_fd, message);
}

TransportResult<bool> UnixSocketTransport::send(Message&& message) {
    return send(message);
}

TransportResult<MessagePtr> UnixSocketTransport::receive(int32_t timeout_ms) {
    if (!connected_ && !unix_config_.is_server) {
        LOGW_FMT("Cannot receive: not connected");
        return {TransportError::NOT_CONNECTED, nullptr, "Not connected"};
    }

    // For SERVER mode, we need to wait for client connection and receive from client FD
    if (unix_config_.is_server) {
        // Wait for at least one client to connect
        auto start_time = std::chrono::steady_clock::now();
        while (true) {
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                if (!clients_.empty()) {
                    // Receive from first client (parent-child 1-to-1 communication)
                    return receiveFromSocket(clients_[0].fd, timeout_ms);
                }
            }

            // Check timeout
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= timeout_ms) {
                return {TransportError::TIMEOUT, nullptr, "No client connected within timeout"};
            }

            // Manually trigger accept by polling the listening socket
            struct pollfd pfd;
            pfd.fd = socket_fd_;
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, 10); // 10ms poll timeout
            if (ret > 0 && (pfd.revents & POLLIN)) {
                // Connection is ready, accept it
                acceptNewConnection();
            }

            // Brief sleep to avoid busy-wait
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // CLIENT mode: receive from server
    int target_fd = client_fd_;
    return receiveFromSocket(target_fd, timeout_ms);
}

TransportResult<MessagePtr> UnixSocketTransport::tryReceive() {
    return receive(0);
}

void UnixSocketTransport::setMessageCallback(MessageCallback callback) {
    message_callback_ = callback;
}

void UnixSocketTransport::setErrorCallback(ErrorCallback callback) {
    error_callback_ = callback;
}

void UnixSocketTransport::setStateChangeCallback(StateChangeCallback callback) {
    state_callback_ = callback;
}

TransportState UnixSocketTransport::getState() const {
    return state_;
}

TransportType UnixSocketTransport::getType() const {
    return TransportType::UNIX_SOCKET;
}

const TransportConfig& UnixSocketTransport::getConfig() const {
    return config_;
}

TransportStats UnixSocketTransport::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void UnixSocketTransport::resetStats() {
    LOGI_FMT("Resetting transport statistics");
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = TransportStats{};
}

std::pair<TransportError, std::string> UnixSocketTransport::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return {last_error_, last_error_msg_};
}

std::string UnixSocketTransport::getInfo() const {
    return "UnixSocketTransport[" + unix_config_.socket_path + "]";
}

// Private helper methods

TransportResult<bool> UnixSocketTransport::createSocket() {
    int sock_type = (unix_config_.socket_type == UnixSocketType::STREAM) ?
        SOCK_STREAM : SOCK_DGRAM;

    LOGD_FMT("Creating Unix socket, type=" << (sock_type == SOCK_STREAM ? "STREAM" : "DGRAM"));
    socket_fd_ = socket(AF_UNIX, sock_type, 0);
    if (socket_fd_ < 0) {
        LOGE_FMT("Failed to create socket: " << strerror(errno));
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to create socket: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    auto result = configureSocket(socket_fd_);
    if (!result) {
        close(socket_fd_);
        socket_fd_ = -1;
        return result;
    }

    LOGD_FMT("Socket created successfully, fd=" << socket_fd_);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::bindSocket() {
    LOGD_FMT("Binding socket to path: " << unix_config_.socket_path);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, unix_config_.socket_path.c_str(),
            sizeof(addr.sun_path) - 1);

    // Remove existing socket file
    unlink(unix_config_.socket_path.c_str());

    if (bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE_FMT("Failed to bind socket: " << strerror(errno));
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to bind socket: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    LOGD_FMT("Socket bound successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::listenSocket() {
    LOGD_FMT("Starting to listen on socket, backlog=" << unix_config_.backlog);
    if (listen(socket_fd_, unix_config_.backlog) < 0) {
        LOGE_FMT("Failed to listen: " << strerror(errno));
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to listen: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    LOGD_FMT("Socket listening successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::connectSocket() {
    LOGD_FMT("Connecting to Unix socket: " << unix_config_.socket_path);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, unix_config_.socket_path.c_str(),
            sizeof(addr.sun_path) - 1);

    if (::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno == EINPROGRESS) {
            LOGD_FMT("Non-blocking connect in progress");
            // Non-blocking connect in progress
            // Would need to use select/poll to wait for completion
        }
        LOGE_FMT("Failed to connect: " << strerror(errno));
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to connect: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    LOGD_FMT("Socket connected successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::configureSocket(int fd) {
    LOGD_FMT("Configuring socket fd=" << fd);
    // Set SO_REUSEADDR
    if (unix_config_.reuse_addr) {
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    // Set send/recv buffer sizes
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
               &unix_config_.send_buffer_size, sizeof(unix_config_.send_buffer_size));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
               &unix_config_.recv_buffer_size, sizeof(unix_config_.recv_buffer_size));

    // Set receive timeout for blocking recv() calls
    // This allows receiver threads to timeout and check exit conditions
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms timeout
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        LOGW_FMT("Failed to set SO_RCVTIMEO: " << strerror(errno));
    }

    // Set non-blocking for async mode
    if (config_.mode == TransportMode::ASYNC || config_.mode == TransportMode::HYBRID) {
        if (!setNonBlocking(fd)) {
            LOGE_FMT("Failed to set non-blocking mode for fd=" << fd);
            return {TransportError::CONNECTION_FAILED, false,
                    "Failed to set non-blocking mode"};
        }
        LOGD_FMT("Socket set to non-blocking mode");
    }

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::setupEpoll() {
    LOGD_FMT("Setting up epoll");
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        LOGE_FMT("Failed to create epoll: " << strerror(errno));
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to create epoll: ") + strerror(errno));
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // Edge-triggered
    ev.data.fd = socket_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_fd_, &ev) < 0) {
        LOGE_FMT("Failed to add socket to epoll: " << strerror(errno));
        setError(TransportError::CONNECTION_FAILED,
                 std::string("Failed to add socket to epoll: ") + strerror(errno));
        close(epoll_fd_);
        epoll_fd_ = -1;
        return {TransportError::CONNECTION_FAILED, false, last_error_msg_};
    }

    LOGD_FMT("Epoll setup successfully, epoll_fd=" << epoll_fd_);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> UnixSocketTransport::sendToSocket(int fd, const Message& message) {
    // Serialize message (outside mutex to reduce lock contention)
    auto result = serializer_->serialize(message);
    if (!result.success) {
        LOGE_FMT("Serialization failed: " << result.error_message);
        setError(TransportError::SERIALIZATION_ERROR, result.error_message);
        updateStats(true, 0, false);
        return {TransportError::SERIALIZATION_ERROR, false, result.error_message};
    }

    // Lock send mutex to serialize concurrent sends to the same socket
    std::lock_guard<std::mutex> lock(send_mutex_);

    // Send size first (4 bytes) with exponential backoff on EAGAIN
    uint32_t msg_size = result.data.size();
    ssize_t sent;
    int retry_count = 0;
    const int MAX_RETRIES = 100;  // Maximum retry attempts
    int backoff_us = 10;  // Start with 10 microseconds

    while (retry_count < MAX_RETRIES) {
        sent = ::send(fd, &msg_size, sizeof(msg_size), MSG_NOSIGNAL);
        if (sent == sizeof(msg_size)) {
            break;  // Success
        }
        if (sent < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted, retry immediately
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full - exponential backoff
                retry_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(backoff_us));
                backoff_us = std::min(backoff_us * 2, 5000);  // Cap at 5ms
                continue;
            }
        }
        // Any other error
        setError(TransportError::SEND_FAILED,
                 std::string("Failed to send size: ") + strerror(errno));
        updateStats(true, 0, false);
        return {TransportError::SEND_FAILED, false, last_error_msg_};
    }

    if (retry_count >= MAX_RETRIES) {
        setError(TransportError::TIMEOUT, "Send size timed out after retries");
        updateStats(true, 0, false);
        return {TransportError::TIMEOUT, false, "Send size timed out"};
    }

    // Send message data with same retry logic
    size_t total_sent = 0;
    retry_count = 0;
    backoff_us = 10;

    while (total_sent < msg_size) {
        sent = ::send(fd, result.data.data() + total_sent,
                     msg_size - total_sent, MSG_NOSIGNAL);
        if (sent > 0) {
            total_sent += sent;
            retry_count = 0;  // Reset retry count on progress
            backoff_us = 10;  // Reset backoff
            continue;
        }
        if (sent < 0) {
            if (errno == EINTR) {
                // Interrupted system call - retry immediately
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket buffer full - exponential backoff
                if (retry_count >= MAX_RETRIES) {
                    setError(TransportError::TIMEOUT, "Send data timed out after retries");
                    updateStats(true, total_sent, false);
                    return {TransportError::TIMEOUT, false, "Send data timed out"};
                }
                retry_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(backoff_us));
                backoff_us = std::min(backoff_us * 2, 5000);  // Cap at 5ms
                continue;
            }
            if (errno == EPIPE) {
                setError(TransportError::CONNECTION_CLOSED, "Broken pipe");
                updateStats(true, total_sent, false);
                return {TransportError::CONNECTION_CLOSED, false, "Broken pipe"};
            }
            setError(TransportError::SEND_FAILED,
                     std::string("Failed to send data: ") + strerror(errno));
            updateStats(true, total_sent, false);
            return {TransportError::SEND_FAILED, false, last_error_msg_};
        }
    }

    updateStats(true, msg_size, true);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<MessagePtr> UnixSocketTransport::receiveFromSocket(int fd, int32_t /* timeout_ms */) {
    // Receive size first (timeout_ms parameter reserved for future use)
    uint32_t msg_size = 0;
    ssize_t received = recv(fd, &msg_size, sizeof(msg_size), 0);

    if (received == 0) {
        return {TransportError::CONNECTION_CLOSED, nullptr, "Connection closed"};
    }

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return {TransportError::TIMEOUT, nullptr, "No data available"};
        }
        if (errno == EINTR) {
            return {TransportError::TIMEOUT, nullptr, "Interrupted"};
        }
        setError(TransportError::RECV_FAILED,
                 std::string("Failed to receive size: ") + strerror(errno));
        updateStats(false, 0, false);
        return {TransportError::RECV_FAILED, nullptr, last_error_msg_};
    }

    if (received != sizeof(msg_size)) {
        setError(TransportError::PROTOCOL_ERROR, "Incomplete size header");
        updateStats(false, 0, false);
        return {TransportError::PROTOCOL_ERROR, nullptr, "Incomplete size header"};
    }

    if (msg_size > config_.max_message_size) {
        setError(TransportError::BUFFER_OVERFLOW, "Message too large");
        updateStats(false, 0, false);
        return {TransportError::BUFFER_OVERFLOW, nullptr, "Message too large"};
    }

    // Receive message data
    std::vector<uint8_t> buffer(msg_size);
    size_t total_received = 0;
    while (total_received < msg_size) {
        received = recv(fd, buffer.data() + total_received,
                       msg_size - total_received, 0);
        if (received <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            setError(TransportError::RECV_FAILED,
                     std::string("Failed to receive data: ") + strerror(errno));
            updateStats(false, total_received, false);
            return {TransportError::RECV_FAILED, nullptr, last_error_msg_};
        }
        total_received += received;
    }

    // Deserialize message
    auto deser_result = serializer_->deserialize(buffer.data(), buffer.size());
    if (!deser_result.success) {
        LOGE_FMT("Deserialization failed: " << deser_result.error_message);
        setError(TransportError::DESERIALIZATION_ERROR, deser_result.error_message);
        updateStats(false, buffer.size(), false);
        return {TransportError::DESERIALIZATION_ERROR, nullptr, deser_result.error_message};
    }

    updateStats(false, buffer.size(), true);
    return {TransportError::SUCCESS, deser_result.message, ""};
}

void UnixSocketTransport::ioThreadFunc() {
    LOGI_FMT("I/O thread started");
    if (unix_config_.use_epoll && unix_config_.is_server) {
        handleEpollEvents();
    }
    LOGI_FMT("I/O thread stopped");
}

void UnixSocketTransport::handleEpollEvents() {
    LOGI_FMT("Starting epoll event loop");
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100); // 100ms timeout

        if (nfds < 0) {
            if (errno == EINTR) continue;
            LOGE_FMT("epoll_wait failed: " << strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == socket_fd_) {
                // New connection
                acceptNewConnection();
            } else {
                // Data from client
                handleClientData(events[i].data.fd);
            }
        }
    }
    LOGI_FMT("Epoll event loop terminated");
}

void UnixSocketTransport::acceptNewConnection() {
    struct sockaddr_un client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_fd = accept(socket_fd_, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            setError(TransportError::CONNECTION_FAILED,
                     std::string("Accept failed: ") + strerror(errno));
        }
        return;
    }

    // Check max connections
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (clients_.size() >= static_cast<size_t>(unix_config_.max_connections)) {
            LOGW_FMT("Max connections reached (" << unix_config_.max_connections << "), rejecting connection");
            close(client_fd);
            return;
        }
    }

    // Configure client socket
    configureSocket(client_fd);

    // Add to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);

    // Add to client list
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        ClientConnection conn;
        conn.fd = client_fd;
        conn.connected_at = std::chrono::system_clock::now();
        clients_.push_back(conn);
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.active_connections++;
    }

    LOGI_FMT("New client connected, fd=" << client_fd << ", total clients=" << clients_.size());
}

void UnixSocketTransport::handleClientData(int fd) {
    // Edge-triggered epoll requires reading until EAGAIN
    // Loop to drain all available messages from the socket buffer
    while (true) {
        auto result = receiveFromSocket(fd, 0);

        // EAGAIN/EWOULDBLOCK means no more data - normal for edge-triggered epoll
        if (result.error == TransportError::TIMEOUT) {
            return;
        }

        if (result.error == TransportError::CONNECTION_CLOSED) {
            LOGI_FMT("Client disconnected, fd=" << fd);
            removeClient(fd);
            return;
        }

        if (!result.success() || !result.value) {
            // Error occurred or no message - stop processing
            return;
        }

        // Store routing information: map message_id to client_fd for response routing
        if (result.value->getType() == MessageType::REQUEST) {
            uint8_t msg_id[16];
            result.value->getMessageId(msg_id);

            std::ostringstream oss;
            for (int i = 0; i < 16; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(msg_id[i]);
            }
            std::string msg_id_str = oss.str();

            std::lock_guard<std::mutex> lock(routing_mutex_);
            request_routing_[msg_id_str] = fd;
        }

        if (message_callback_) {
            message_callback_(result.value);
        }
        // Continue loop to read next message
    }
}

void UnixSocketTransport::removeClient(int fd) {
    LOGD_FMT("Removing client fd=" << fd);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                          [fd](const ClientConnection& c) { return c.fd == fd; }),
            clients_.end()
        );
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (stats_.active_connections > 0) {
            stats_.active_connections--;
        }
    }
}

void UnixSocketTransport::setState(TransportState new_state) {
    TransportState old_state = state_.exchange(new_state);
    if (old_state != new_state) {
        LOGV_FMT("State changed: " << static_cast<int>(old_state) << " -> " << static_cast<int>(new_state));
        if (state_callback_) {
            state_callback_(old_state, new_state);
        }
    }
}

void UnixSocketTransport::setError(TransportError error, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = error;
        last_error_msg_ = message;
        stats_.last_error = message;
        stats_.last_error_time = std::chrono::system_clock::now();
    }

    if (error_callback_) {
        error_callback_(error, message);
    }
}

void UnixSocketTransport::updateStats(bool is_send, uint32_t bytes, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (is_send) {
        if (success) {
            stats_.messages_sent++;
            stats_.bytes_sent += bytes;
        } else {
            stats_.send_errors++;
        }
    } else {
        if (success) {
            stats_.messages_received++;
            stats_.bytes_received += bytes;
        } else {
            stats_.recv_errors++;
        }
    }
}

bool UnixSocketTransport::setNonBlocking(int fd) {
    LOGD_FMT("Setting non-blocking mode for fd=" << fd);
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        LOGE_FMT("Failed to get socket flags for fd=" << fd << ": " << strerror(errno));
        return false;
    }
    bool result = fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
    if (!result) {
        LOGE_FMT("Failed to set non-blocking for fd=" << fd << ": " << strerror(errno));
    }
    return result;
}

bool UnixSocketTransport::setSocketOptions(int fd, const UnixSocketConfig& config) {
    LOGD_FMT("Setting socket options for fd=" << fd << ", reuse_addr=" << config.reuse_addr
             << ", send_buf=" << config.send_buffer_size << ", recv_buf=" << config.recv_buffer_size);
    if (config.reuse_addr) {
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
               &config.send_buffer_size, sizeof(config.send_buffer_size));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
               &config.recv_buffer_size, sizeof(config.recv_buffer_size));

    LOGD_FMT("Socket options set successfully for fd=" << fd);
    return true;
}

} // namespace ipc
} // namespace cdmf
