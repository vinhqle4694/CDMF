/**
 * @file grpc_transport.h
 * @brief gRPC transport implementation
 *
 * Provides IPC/RPC transport using gRPC C++ library with bidirectional
 * streaming, connection pooling, and async completion queue pattern.
 *
 * @version 1.0.0
 * @date 2025-10-03
 * @author Transport Agent
 */

#ifndef CDMF_IPC_GRPC_TRANSPORT_H
#define CDMF_IPC_GRPC_TRANSPORT_H

#include "transport.h"
#include <memory>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

// Forward declarations for gRPC types to avoid heavy includes in header
namespace grpc {
    class Channel;
    class Server;
    class ServerBuilder;
    class CompletionQueue;
    class ServerCompletionQueue;
    class ClientContext;
    class ServerContext;
    template<class W, class R> class ServerAsyncReaderWriter;
    template<class W, class R> class ClientAsyncReaderWriter;
}

namespace cdmf {
namespace ipc {

/**
 * @brief gRPC specific configuration
 */
struct GrpcConfig {
    /** Server address (e.g., "localhost:50051", "unix:///tmp/grpc.sock") */
    std::string server_address;

    /** Enable server mode (listen) */
    bool is_server = false;

    /** Enable TLS/SSL */
    bool enable_tls = false;

    /** Server certificate path (PEM format) */
    std::string server_cert_path;

    /** Server private key path (PEM format) */
    std::string server_key_path;

    /** Client CA certificate path for verification */
    std::string ca_cert_path;

    /** Maximum number of concurrent streams */
    int max_concurrent_streams = 100;

    /** Keepalive time (seconds) */
    int keepalive_time_sec = 30;

    /** Keepalive timeout (seconds) */
    int keepalive_timeout_sec = 10;

    /** Enable keepalive without calls */
    bool keepalive_permit_without_calls = true;

    /** Maximum message size (bytes) */
    int max_message_size = 16 * 1024 * 1024; // 16 MB

    /** Number of completion queue threads */
    int cq_thread_count = 4;

    /** Connection pool size (client mode) */
    int connection_pool_size = 1;

    /** Enable health checking */
    bool enable_health_check = true;

    /** Health check interval (seconds) */
    int health_check_interval_sec = 30;

    GrpcConfig() = default;
};

/**
 * @brief gRPC stream state
 */
enum class GrpcStreamState {
    IDLE,
    ACTIVE,
    FINISHING,
    FINISHED,
    ERROR
};

/**
 * @brief gRPC transport implementation
 */
class GrpcTransport : public ITransport {
public:
    /**
     * @brief Constructor
     */
    GrpcTransport();

    /**
     * @brief Destructor
     */
    ~GrpcTransport() override;

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

    // gRPC specific methods

    /**
     * @brief Get gRPC configuration
     */
    const GrpcConfig& getGrpcConfig() const { return grpc_config_; }

    /**
     * @brief Get stream state
     */
    GrpcStreamState getStreamState() const { return stream_state_; }

private:
    // Configuration
    TransportConfig config_;
    GrpcConfig grpc_config_;

    // State
    std::atomic<TransportState> state_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::atomic<GrpcStreamState> stream_state_;

    // gRPC components (using forward declarations)
    class GrpcImpl;  // Implementation details hidden
    std::unique_ptr<GrpcImpl> impl_;

    // Server components
    std::unique_ptr<grpc::Server> server_;
    std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> server_cqs_;
    std::vector<std::unique_ptr<std::thread>> server_threads_;

    // Client components
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<grpc::CompletionQueue> client_cq_;
    std::unique_ptr<std::thread> client_thread_;

    // Message queues
    std::queue<MessagePtr> send_queue_;
    std::queue<MessagePtr> recv_queue_;
    mutable std::mutex send_queue_mutex_;
    mutable std::mutex recv_queue_mutex_;
    std::condition_variable recv_cv_;
    std::condition_variable send_cv_;

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

    // Helper methods
    TransportResult<bool> initServer();
    TransportResult<bool> initClient();
    TransportResult<bool> createChannel();
    TransportResult<bool> createServerCQs();
    TransportResult<bool> startServerThreads();
    TransportResult<bool> startClientThread();

    void serverThreadFunc(int cq_index);
    void clientThreadFunc();
    void handleServerEvent();
    void handleClientEvent();

    TransportResult<bool> enqueueMessage(const Message& message);
    TransportResult<MessagePtr> dequeueMessage(int32_t timeout_ms);

    void setState(TransportState new_state);
    void setStreamState(GrpcStreamState new_state);
    void setError(TransportError error, const std::string& message);
    void updateStats(bool is_send, uint32_t bytes, bool success);

    std::string buildChannelArgs();
    static std::string grpcStatusToString(int status_code);
};

} // namespace ipc
} // namespace cdmf

#endif // CDMF_IPC_GRPC_TRANSPORT_H
