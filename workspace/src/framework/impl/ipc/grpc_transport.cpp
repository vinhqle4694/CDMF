/**
 * @file grpc_transport.cpp
 * @brief gRPC transport implementation
 *
 * @version 1.0.0
 * @date 2025-10-03
 */

#include "ipc/grpc_transport.h"
#include "ipc/serializer.h"
#include "utils/log.h"

// Note: This is a simplified implementation showing the structure.
// Full gRPC integration would require:
// - message_service.proto file
// - Generated gRPC stubs
// - Linking with gRPC C++ library

#include <sstream>
#include <chrono>

// Stub definitions for grpc types (replace with actual grpc++ includes when available)
#ifdef GRPC_AVAILABLE
#include <grpcpp/grpcpp.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/channel.h>
#include <grpcpp/completion_queue.h>
#else
// Stub declarations for compilation without gRPC - must be after chrono/system includes
namespace grpc {
    class Server {
    public:
        void Shutdown() {}
    };
    class ServerCompletionQueue {
    public:
        void Shutdown() {}
    };
    class CompletionQueue {
    public:
        void Shutdown() {}
    };
}
#endif

namespace cdmf {
namespace ipc {

// Implementation details hidden from header
class GrpcTransport::GrpcImpl {
public:
    // Would contain gRPC-specific implementation details
    // such as service stubs, completion queue handlers, etc.
};

GrpcTransport::GrpcTransport()
    : state_(TransportState::UNINITIALIZED)
    , running_(false)
    , connected_(false)
    , stream_state_(GrpcStreamState::IDLE)
    , impl_(std::make_unique<GrpcImpl>())
    , last_error_(TransportError::SUCCESS) {
    LOGD_FMT("GrpcTransport constructed");
}

GrpcTransport::~GrpcTransport() {
    LOGD_FMT("GrpcTransport destructor called");
    cleanup();
}

TransportResult<bool> GrpcTransport::init(const TransportConfig& config) {
    if (state_ != TransportState::UNINITIALIZED) {
        LOGW_FMT("GrpcTransport already initialized");
        return {TransportError::ALREADY_INITIALIZED, false, "Transport already initialized"};
    }

    LOGI_FMT("Initializing GrpcTransport, endpoint=" << config.endpoint);
    config_ = config;

    // Parse gRPC specific config from properties
    grpc_config_.server_address = config.endpoint;

    auto it = config.properties.find("is_server");
    if (it != config.properties.end()) {
        grpc_config_.is_server = (it->second == "true" || it->second == "1");
    }

    it = config.properties.find("enable_tls");
    if (it != config.properties.end()) {
        grpc_config_.enable_tls = (it->second == "true" || it->second == "1");
    }

    it = config.properties.find("max_concurrent_streams");
    if (it != config.properties.end()) {
        grpc_config_.max_concurrent_streams = std::stoi(it->second);
    }

    it = config.properties.find("cq_thread_count");
    if (it != config.properties.end()) {
        grpc_config_.cq_thread_count = std::stoi(it->second);
    }

    setState(TransportState::INITIALIZED);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::start() {
    if (state_ != TransportState::INITIALIZED && state_ != TransportState::DISCONNECTED) {
        return {TransportError::NOT_INITIALIZED, false, "Transport not initialized"};
    }

    TransportResult<bool> result;

    if (grpc_config_.is_server) {
        result = initServer();
    } else {
        result = initClient();
    }

    if (!result) {
        return result;
    }

    setState(TransportState::CONNECTED);
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::stop() {
    LOGI_FMT("GrpcTransport::stop called");
    if (!running_ && !connected_) {
        LOGD_FMT("GrpcTransport already stopped");
        return {TransportError::SUCCESS, true, ""};
    }

    running_ = false;

    // Stop server threads
    for (size_t i = 0; i < server_threads_.size(); ++i) {
        auto& thread = server_threads_[i];
        if (thread && thread->joinable()) {
            LOGD_FMT("Joining server thread " << i);
            thread->join();
        }
    }
    server_threads_.clear();

    // Stop client thread
    if (client_thread_ && client_thread_->joinable()) {
        LOGD_FMT("Joining client thread");
        client_thread_->join();
    }

    // Shutdown server
    if (server_) {
        LOGD_FMT("Shutting down gRPC server");
        server_->Shutdown();
        server_.reset();
    }

    // Shutdown completion queues
    for (size_t i = 0; i < server_cqs_.size(); ++i) {
        auto& cq = server_cqs_[i];
        if (cq) {
            LOGD_FMT("Shutting down server completion queue " << i);
            cq->Shutdown();
        }
    }
    server_cqs_.clear();

    if (client_cq_) {
        LOGD_FMT("Shutting down client completion queue");
        client_cq_->Shutdown();
        client_cq_.reset();
    }

    disconnect();

    setState(TransportState::DISCONNECTED);
    LOGI_FMT("GrpcTransport::stop completed");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::cleanup() {
    LOGI_FMT("GrpcTransport::cleanup called");
    stop();

    channel_.reset();
    setState(TransportState::UNINITIALIZED);

    LOGI_FMT("GrpcTransport::cleanup completed");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::connect() {
    LOGI_FMT("GrpcTransport::connect called");
    if (connected_) {
        LOGW_FMT("GrpcTransport already connected");
        return {TransportError::ALREADY_CONNECTED, false, "Already connected"};
    }

    if (grpc_config_.is_server) {
        LOGW_FMT("GrpcTransport::connect failed - server mode does not connect");
        return {TransportError::INVALID_CONFIG, false, "Server mode does not connect"};
    }

    setState(TransportState::CONNECTING);

    auto result = createChannel();
    if (!result) {
        LOGE_FMT("GrpcTransport::connect - createChannel failed");
        setState(TransportState::DISCONNECTED);
        return result;
    }

    connected_ = true;
    setStreamState(GrpcStreamState::ACTIVE);
    setState(TransportState::CONNECTED);

    LOGI_FMT("GrpcTransport::connect completed successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::disconnect() {
    LOGI_FMT("GrpcTransport::disconnect called");
    if (!connected_) {
        LOGD_FMT("GrpcTransport already disconnected");
        return {TransportError::SUCCESS, true, ""};
    }

    setState(TransportState::DISCONNECTING);

    setStreamState(GrpcStreamState::FINISHING);

    // Close stream and channel
    channel_.reset();

    connected_ = false;
    setStreamState(GrpcStreamState::FINISHED);
    setState(TransportState::DISCONNECTED);

    LOGI_FMT("GrpcTransport::disconnect completed");
    return {TransportError::SUCCESS, true, ""};
}

bool GrpcTransport::isConnected() const {
    return connected_;
}

TransportResult<bool> GrpcTransport::send(const Message& message) {
    LOGD_FMT("GrpcTransport::send - message type: " << static_cast<int>(message.getType())
             << ", size: " << message.getTotalSize());
    if (!connected_) {
        LOGE_FMT("GrpcTransport::send failed - not connected");
        return {TransportError::NOT_CONNECTED, false, "Not connected"};
    }

    if (stream_state_ != GrpcStreamState::ACTIVE) {
        LOGE_FMT("GrpcTransport::send failed - stream not active");
        return {TransportError::PROTOCOL_ERROR, false, "Stream not active"};
    }

    // Enqueue message for async sending
    auto result = enqueueMessage(message);
    if (!result) {
        LOGE_FMT("GrpcTransport::send - enqueue failed");
        return result;
    }

    // Signal send thread
    send_cv_.notify_one();

    updateStats(true, message.getTotalSize(), true);
    LOGD_FMT("GrpcTransport::send completed successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::send(Message&& message) {
    return send(message);
}

TransportResult<MessagePtr> GrpcTransport::receive(int32_t timeout_ms) {
    LOGD_FMT("GrpcTransport::receive - timeout: " << timeout_ms << "ms");
    if (!connected_) {
        LOGE_FMT("GrpcTransport::receive failed - not connected");
        return {TransportError::NOT_CONNECTED, nullptr, "Not connected"};
    }

    auto result = dequeueMessage(timeout_ms);
    if (result.success()) {
        LOGD_FMT("GrpcTransport::receive completed successfully");
    }
    return result;
}

TransportResult<MessagePtr> GrpcTransport::tryReceive() {
    LOGD_FMT("GrpcTransport::tryReceive (non-blocking)");
    return receive(0);
}

void GrpcTransport::setMessageCallback(MessageCallback callback) {
    LOGD_FMT("Setting message callback");
    message_callback_ = callback;
}

void GrpcTransport::setErrorCallback(ErrorCallback callback) {
    LOGD_FMT("Setting error callback");
    error_callback_ = callback;
}

void GrpcTransport::setStateChangeCallback(StateChangeCallback callback) {
    LOGD_FMT("Setting state change callback");
    state_callback_ = callback;
}

TransportState GrpcTransport::getState() const {
    return state_;
}

TransportType GrpcTransport::getType() const {
    return TransportType::GRPC;
}

const TransportConfig& GrpcTransport::getConfig() const {
    return config_;
}

TransportStats GrpcTransport::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void GrpcTransport::resetStats() {
    LOGD_FMT("Resetting GrpcTransport stats");
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = TransportStats{};
}

std::pair<TransportError, std::string> GrpcTransport::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return {last_error_, last_error_msg_};
}

std::string GrpcTransport::getInfo() const {
    std::ostringstream oss;
    oss << "GrpcTransport[" << grpc_config_.server_address
        << ", " << (grpc_config_.is_server ? "SERVER" : "CLIENT")
        << ", TLS=" << (grpc_config_.enable_tls ? "ON" : "OFF") << "]";
    return oss.str();
}

// Private helper methods

TransportResult<bool> GrpcTransport::initServer() {
    LOGD_FMT("Initializing gRPC server");
    // Note: Simplified - actual implementation would:
    // 1. Create ServerBuilder
    // 2. Add listening port (with or without TLS)
    // 3. Register service
    // 4. Build server
    // 5. Create completion queues
    // 6. Start server threads

    auto result = createServerCQs();
    if (!result) return result;

    result = startServerThreads();
    if (!result) return result;

    running_ = true;
    connected_ = true;

    LOGD_FMT("gRPC server initialized successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::initClient() {
    LOGD_FMT("Initializing gRPC client");
    auto result = createChannel();
    if (!result) return result;

    // Create client completion queue
    // client_cq_ = std::make_unique<grpc::CompletionQueue>();

    result = startClientThread();
    if (!result) return result;

    running_ = true;

    LOGD_FMT("gRPC client initialized successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::createChannel() {
    LOGD_FMT("Creating gRPC channel to: " << grpc_config_.server_address);
    // Note: Simplified - actual implementation would:
    // 1. Build channel arguments (keepalive, message size, etc.)
    // 2. Create credentials (TLS or insecure)
    // 3. Create channel using grpc::CreateChannel or grpc::CreateCustomChannel

    std::string channel_args = buildChannelArgs();
    LOGD_FMT("Channel args: " << channel_args);

    // Placeholder - would create actual gRPC channel here
    // if (grpc_config_.enable_tls) {
    //     auto creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
    //     channel_ = grpc::CreateChannel(grpc_config_.server_address, creds);
    // } else {
    //     auto creds = grpc::InsecureChannelCredentials();
    //     channel_ = grpc::CreateChannel(grpc_config_.server_address, creds);
    // }

    LOGD_FMT("gRPC channel created successfully");
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::createServerCQs() {
    LOGD_FMT("Creating " << grpc_config_.cq_thread_count << " server completion queues");
    for (int i = 0; i < grpc_config_.cq_thread_count; i++) {
        // server_cqs_.push_back(std::make_unique<grpc::ServerCompletionQueue>());
    }
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::startServerThreads() {
    LOGD_FMT("Starting " << grpc_config_.cq_thread_count << " server threads");
    for (int i = 0; i < grpc_config_.cq_thread_count; i++) {
        server_threads_.push_back(
            std::make_unique<std::thread>(&GrpcTransport::serverThreadFunc, this, i)
        );
    }
    return {TransportError::SUCCESS, true, ""};
}

TransportResult<bool> GrpcTransport::startClientThread() {
    LOGD_FMT("Starting client thread");
    client_thread_ = std::make_unique<std::thread>(&GrpcTransport::clientThreadFunc, this);
    return {TransportError::SUCCESS, true, ""};
}

void GrpcTransport::serverThreadFunc(int cq_index) {
    LOGD_FMT("Server thread " << cq_index << " started");
    // Note: Simplified - actual implementation would:
    // 1. Handle async RPC calls from completion queue
    // 2. Process incoming streams
    // 3. Send responses
    // 4. Handle stream lifecycle (start, data, finish)

    while (running_) {
        handleServerEvent();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOGD_FMT("Server thread " << cq_index << " terminated");
}

void GrpcTransport::clientThreadFunc() {
    LOGD_FMT("Client thread started");
    // Note: Simplified - actual implementation would:
    // 1. Handle async responses from completion queue
    // 2. Process incoming messages from stream
    // 3. Manage stream lifecycle

    while (running_) {
        handleClientEvent();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    LOGD_FMT("Client thread terminated");
}

void GrpcTransport::handleServerEvent() {
    // Placeholder for server event handling
    // Would use completion queue Next() to get events
}

void GrpcTransport::handleClientEvent() {
    // Placeholder for client event handling
    // Would use completion queue Next() to get events
}

TransportResult<bool> GrpcTransport::enqueueMessage(const Message& message) {
    LOGD_FMT("Enqueueing message for send");
    std::lock_guard<std::mutex> lock(send_queue_mutex_);

    auto msg_ptr = std::make_shared<Message>(message);
    send_queue_.push(msg_ptr);

    return {TransportError::SUCCESS, true, ""};
}

TransportResult<MessagePtr> GrpcTransport::dequeueMessage(int32_t timeout_ms) {
    LOGD_FMT("Dequeueing message, timeout: " << timeout_ms << "ms");
    std::unique_lock<std::mutex> lock(recv_queue_mutex_);

    if (recv_queue_.empty()) {
        if (timeout_ms == 0) {
            return {TransportError::TIMEOUT, nullptr, "No message available"};
        } else if (timeout_ms < 0) {
            recv_cv_.wait(lock, [this] { return !recv_queue_.empty() || !running_; });
        } else {
            auto result = recv_cv_.wait_for(
                lock,
                std::chrono::milliseconds(timeout_ms),
                [this] { return !recv_queue_.empty() || !running_; }
            );
            if (!result) {
                LOGW_FMT("Dequeue timeout after " << timeout_ms << "ms");
                return {TransportError::TIMEOUT, nullptr, "Timeout waiting for message"};
            }
        }
    }

    if (recv_queue_.empty()) {
        return {TransportError::TIMEOUT, nullptr, "No message available"};
    }

    auto message = recv_queue_.front();
    recv_queue_.pop();

    updateStats(false, message->getTotalSize(), true);
    LOGD_FMT("Message dequeued successfully");
    return {TransportError::SUCCESS, message, ""};
}

void GrpcTransport::setState(TransportState new_state) {
    TransportState old_state = state_.exchange(new_state);
    if (old_state != new_state && state_callback_) {
        state_callback_(old_state, new_state);
    }
}

void GrpcTransport::setStreamState(GrpcStreamState new_state) {
    stream_state_.store(new_state);
}

void GrpcTransport::setError(TransportError error, const std::string& message) {
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

void GrpcTransport::updateStats(bool is_send, uint32_t bytes, bool success) {
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

std::string GrpcTransport::buildChannelArgs() {
    std::ostringstream oss;

    oss << "keepalive_time_ms=" << (grpc_config_.keepalive_time_sec * 1000)
        << ",keepalive_timeout_ms=" << (grpc_config_.keepalive_timeout_sec * 1000)
        << ",max_concurrent_streams=" << grpc_config_.max_concurrent_streams
        << ",max_message_size=" << grpc_config_.max_message_size;

    return oss.str();
}

std::string GrpcTransport::grpcStatusToString(int status_code) {
    // Map gRPC status codes to strings
    const char* status_names[] = {
        "OK", "CANCELLED", "UNKNOWN", "INVALID_ARGUMENT", "DEADLINE_EXCEEDED",
        "NOT_FOUND", "ALREADY_EXISTS", "PERMISSION_DENIED", "RESOURCE_EXHAUSTED",
        "FAILED_PRECONDITION", "ABORTED", "OUT_OF_RANGE", "UNIMPLEMENTED",
        "INTERNAL", "UNAVAILABLE", "DATA_LOSS", "UNAUTHENTICATED"
    };

    if (status_code >= 0 && status_code < 17) {
        return status_names[status_code];
    }
    return "UNKNOWN_STATUS";
}

} // namespace ipc
} // namespace cdmf
