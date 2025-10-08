/**
 * @file sandbox_ipc.cpp
 * @brief Sandbox IPC implementation
 *
 * @version 1.0.0
 * @date 2025-10-06
 */

#include "security/sandbox_ipc.h"
#include "ipc/shared_memory_transport.h"
#include "ipc/unix_socket_transport.h"
#include "utils/log.h"
#include <nlohmann/json.hpp>
#include <cstring>

using json = nlohmann::json;

namespace cdmf {
namespace security {

// ========== SandboxMessage ==========

ipc::MessagePtr SandboxMessage::toIPCMessage() const {
    auto msg = std::make_shared<ipc::Message>(ipc::MessageType::REQUEST);

    // Pack sandbox message into JSON payload
    json j;
    j["type"] = static_cast<uint32_t>(type);
    j["moduleId"] = moduleId;
    j["payload"] = payload;
    j["requestId"] = requestId;
    j["errorCode"] = errorCode;

    std::string jsonStr = j.dump();
    msg->setPayload(jsonStr.data(), jsonStr.size());
    msg->updateChecksum();

    return msg;
}

SandboxMessage SandboxMessage::fromIPCMessage(const ipc::Message& msg) {
    SandboxMessage result;

    // Extract JSON payload
    const uint8_t* data = msg.getPayload();
    uint32_t size = msg.getPayloadSize();

    if (data && size > 0) {
        try {
            std::string jsonStr(reinterpret_cast<const char*>(data), size);
            json j = json::parse(jsonStr);

            result.type = static_cast<SandboxMessageType>(j.value("type", 0u));
            result.moduleId = j.value("moduleId", "");
            result.payload = j.value("payload", "");
            result.requestId = j.value("requestId", 0ull);
            result.errorCode = j.value("errorCode", 0);
        } catch (const json::exception& e) {
            LOGE_FMT("Failed to parse sandbox message JSON: " << e.what());
            result.errorCode = -1;
        }
    }

    return result;
}

// ========== SandboxIPC ==========

SandboxIPC::SandboxIPC(Role role, const std::string& sandboxId,
                       std::unique_ptr<ipc::ITransport> transport)
    : role_(role)
    , sandbox_id_(sandboxId)
    , transport_(std::move(transport))
    , stats_{}
    , next_request_id_(1)
    , receiver_running_(false) {
    LOGD_FMT("SandboxIPC created: role=" << (role == Role::PARENT ? "PARENT" : "CHILD")
             << ", sandboxId=" << sandboxId);
}

SandboxIPC::~SandboxIPC() {
    close();
    LOGD_FMT("SandboxIPC destroyed for sandbox: " << sandbox_id_);
}

bool SandboxIPC::initialize(const ipc::TransportConfig& config) {
    LOGI_FMT("Initializing SandboxIPC: sandbox=" << sandbox_id_
             << ", transport=" << ipc::transportTypeToString(config.type));

    if (!transport_) {
        LOGE("Transport is null");
        return false;
    }

    // Initialize transport
    auto result = transport_->init(config);
    if (!result.success()) {
        LOGE_FMT("Failed to initialize transport: " << result.error_message);
        return false;
    }

    if (role_ == Role::PARENT) {
        // Parent: Start listening/binding
        result = transport_->start();
        if (!result.success()) {
            LOGE_FMT("Failed to start transport: " << result.error_message);
            return false;
        }
        LOGI_FMT("Parent transport started: " << getEndpoint());

        // Don't start receiver thread yet - wait for initial connection handshake
        // Call startReceiverThread() after initial message exchange completes
    } else {
        // Child: Start transport first (sets up ring buffers for shared memory, creates socket for unix socket)
        result = transport_->start();
        if (!result.success()) {
            LOGE_FMT("Failed to start child transport: " << result.error_message);
            return false;
        }
        LOGI_FMT("Child transport started: " << getEndpoint());

        // Then connect to parent
        result = transport_->connect();
        if (!result.success()) {
            LOGE_FMT("Failed to connect transport: " << result.error_message);
            return false;
        }
        LOGI_FMT("Child transport connected: " << getEndpoint());
    }

    return true;
}

bool SandboxIPC::sendMessage(const SandboxMessage& msg, int32_t timeoutMs) {
    if (!transport_ || !transport_->isConnected()) {
        LOGE("Transport not connected");
        updateStats(true, 0, false);
        return false;
    }

    // Convert to IPC message
    auto ipcMsg = msg.toIPCMessage();
    if (!ipcMsg) {
        LOGE("Failed to convert sandbox message to IPC message");
        updateStats(true, 0, false);
        return false;
    }

    // Send via transport
    auto result = transport_->send(*ipcMsg);
    size_t bytes = ipcMsg->getTotalSize();

    if (!result.success()) {
        LOGE_FMT("Failed to send message: " << result.error_message);
        updateStats(true, bytes, false);
        return false;
    }

    updateStats(true, bytes, true);
    LOGV_FMT("Sent message: type=" << static_cast<uint32_t>(msg.type)
             << ", requestId=" << msg.requestId << ", bytes=" << bytes);
    return true;
}

bool SandboxIPC::receiveMessage(SandboxMessage& msg, int32_t timeoutMs) {
    if (!transport_ || !transport_->isConnected()) {
        LOGE("Transport not connected");
        updateStats(false, 0, false);
        return false;
    }

    // Receive via transport
    auto result = transport_->receive(timeoutMs);
    if (!result.success()) {
        if (result.error != ipc::TransportError::TIMEOUT) {
            LOGE_FMT("Failed to receive message: " << result.error_message);
        }
        updateStats(false, 0, false);
        return false;
    }

    if (!result.value) {
        LOGE("Received null message");
        updateStats(false, 0, false);
        return false;
    }

    // Convert from IPC message
    size_t bytes = result.value->getTotalSize();
    msg = SandboxMessage::fromIPCMessage(*result.value);

    updateStats(false, bytes, true);
    LOGV_FMT("Received message: type=" << static_cast<uint32_t>(msg.type)
             << ", requestId=" << msg.requestId << ", bytes=" << bytes);
    return true;
}

bool SandboxIPC::tryReceiveMessage(SandboxMessage& msg) {
    if (!transport_ || !transport_->isConnected()) {
        return false;
    }

    auto result = transport_->tryReceive();
    if (!result.success() || !result.value) {
        return false;
    }

    msg = SandboxMessage::fromIPCMessage(*result.value);
    size_t bytes = result.value->getTotalSize();
    updateStats(false, bytes, true);
    return true;
}

bool SandboxIPC::sendRequest(const SandboxMessage& request, SandboxMessage& response,
                             int32_t timeoutMs) {
    // Assign request ID if not set
    SandboxMessage req = request;
    if (req.requestId == 0) {
        req.requestId = next_request_id_++;
    }

    // Send request
    if (!sendMessage(req, timeoutMs)) {
        return false;
    }

    // Wait for response with matching request ID
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= timeoutMs) {
            LOGE_FMT("Request timeout: requestId=" << req.requestId);
            return false;
        }

        int32_t remainingTimeout = timeoutMs - static_cast<int32_t>(elapsed);
        if (!receiveMessage(response, remainingTimeout)) {
            continue;
        }

        // Check if response matches request
        if (response.requestId == req.requestId) {
            return true;
        }

        LOGW_FMT("Received message with mismatched requestId: expected="
                 << req.requestId << ", got=" << response.requestId);
    }

    return false;
}

void SandboxIPC::startReceiverThread() {
    if (role_ != Role::PARENT) {
        LOGW("startReceiverThread() called on non-parent role, ignoring");
        return;
    }

    if (receiver_running_) {
        LOGW_FMT("Receiver thread already running for sandbox: " << sandbox_id_);
        return;
    }

    receiver_running_ = true;
    receiver_thread_ = std::make_unique<std::thread>(&SandboxIPC::receiverThreadFunc, this);
    LOGI_FMT("Parent receiver thread started for sandbox: " << sandbox_id_);
}

void SandboxIPC::stopReceiverThread() {
    if (role_ != Role::PARENT) {
        return;
    }

    if (!receiver_running_) {
        return;
    }

    LOGI_FMT("Stopping receiver thread for sandbox: " << sandbox_id_);
    receiver_running_ = false;

    if (receiver_thread_ && receiver_thread_->joinable()) {
        receiver_thread_->join();
    }
    LOGI_FMT("Receiver thread stopped for sandbox: " << sandbox_id_);
}

void SandboxIPC::close() {
    // Stop receiver thread if running
    stopReceiverThread();

    if (transport_) {
        LOGI_FMT("Closing SandboxIPC: sandbox=" << sandbox_id_);
        transport_->stop();
        transport_->cleanup();
    }
}

bool SandboxIPC::isConnected() const {
    return transport_ && transport_->isConnected();
}

std::string SandboxIPC::getEndpoint() const {
    if (transport_) {
        return transport_->getConfig().endpoint;
    }
    return "";
}

SandboxIPC::Stats SandboxIPC::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void SandboxIPC::updateStats(bool is_send, size_t bytes, bool success) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (is_send) {
        if (success) {
            stats_.messages_sent++;
            stats_.bytes_sent += bytes;
        } else {
            stats_.send_failures++;
        }
    } else {
        if (success) {
            stats_.messages_received++;
            stats_.bytes_received += bytes;
        } else {
            stats_.receive_failures++;
        }
    }
}

// ========== Helper Functions ==========

std::unique_ptr<ipc::ITransport> createSandboxTransport(
    ipc::TransportType type,
    const std::string& sandboxId) {

    switch (type) {
        case ipc::TransportType::SHARED_MEMORY:
            return std::make_unique<ipc::SharedMemoryTransport>();

        case ipc::TransportType::UNIX_SOCKET:
            return std::make_unique<ipc::UnixSocketTransport>();

        // TCP transport would be added here
        // case ipc::TransportType::TCP_SOCKET:
        //     return std::make_unique<ipc::TCPSocketTransport>();

        default:
            LOGW_FMT("Unknown transport type, using SharedMemory");
            return std::make_unique<ipc::SharedMemoryTransport>();
    }
}

ipc::TransportConfig createSandboxTransportConfig(
    ipc::TransportType type,
    const std::string& sandboxId,
    SandboxIPC::Role role,
    const std::map<std::string, std::string>& properties) {

    ipc::TransportConfig config;
    config.type = type;
    config.mode = ipc::TransportMode::SYNC; // Sandbox IPC uses sync mode

    // Helper lambda to get property with default
    auto getProperty = [&properties](const std::string& key, const std::string& defaultValue) {
        auto it = properties.find(key);
        return (it != properties.end()) ? it->second : defaultValue;
    };

    auto getPropertyInt = [&getProperty](const std::string& key, uint32_t defaultValue) {
        std::string value = getProperty(key, "");
        return value.empty() ? defaultValue : std::stoul(value);
    };

    config.send_timeout_ms = getPropertyInt("ipc_timeout_ms", 5000);
    config.recv_timeout_ms = getPropertyInt("ipc_timeout_ms", 5000);
    config.buffer_size = getPropertyInt("ipc_buffer_size", 4096);

    // Transport-specific configuration
    switch (type) {
        case ipc::TransportType::SHARED_MEMORY: {
            // Endpoint is shared memory name
            // POSIX shm_open() expects name starting with "/" (NOT /dev/shm/)
#ifdef _WIN32
            config.endpoint = "Global\\cdmf_sandbox_" + sandboxId;
#else
            config.endpoint = "/cdmf_sandbox_" + sandboxId;
#endif

            // Shared memory specific properties
            config.properties["shm_size"] = getProperty("ipc_shm_size", "4194304"); // 4 MB default
            config.properties["ring_capacity"] = getProperty("ipc_buffer_size", "4096");
            config.properties["create_shm"] = (role == SandboxIPC::Role::PARENT) ? "true" : "false";
            break;
        }

        case ipc::TransportType::UNIX_SOCKET: {
            // Endpoint is socket path
#ifdef _WIN32
            config.endpoint = "\\\\.\\pipe\\cdmf_sandbox_" + sandboxId;
#else
            config.endpoint = "/tmp/cdmf_sandbox_" + sandboxId + ".sock";
#endif
            // Use file-based socket for parent-child communication
            // Parent will bind/listen, child will connect
            config.properties["is_server"] = (role == SandboxIPC::Role::PARENT) ? "true" : "false";
            break;
        }

        case ipc::TransportType::TCP_SOCKET: {
            // Endpoint from properties or default to localhost with dynamic port
            config.endpoint = getProperty("ipc_endpoint", "localhost:0");
            config.properties["reuse_addr"] = "true";
            break;
        }

        default:
            break;
    }

    // Copy remaining properties
    for (const auto& [key, value] : properties) {
        if (config.properties.find(key) == config.properties.end()) {
            config.properties[key] = value;
        }
    }

    return config;
}

void SandboxIPC::receiverThreadFunc() {
    LOGI_FMT("Receiver thread started for sandbox: " << sandbox_id_);

    while (receiver_running_) {
        SandboxMessage msg;

        // Try to receive message with 100ms timeout
        if (!receiveMessage(msg, 100)) {
            continue; // Timeout or error, try again
        }

        // Process received message
        switch (msg.type) {
            case SandboxMessageType::HEARTBEAT:
                // LOGD_FMT("Received HEARTBEAT from child: sandbox=" << sandbox_id_);
                break;

            case SandboxMessageType::STATUS_REPORT:
                // LOGD_FMT("Received STATUS_REPORT from child: sandbox=" << sandbox_id_
                //          << ", payload=" << msg.payload);
                break;

            case SandboxMessageType::ERROR:
                // LOGD_FMT("Received ERROR from child: sandbox=" << sandbox_id_
                //          << ", error=" << msg.payload);
                break;

            default:
                // LOGV_FMT("Received message type=" << static_cast<uint32_t>(msg.type)
                //          << " from child: sandbox=" << sandbox_id_);
                break;
        }
    }

    LOGI_FMT("Receiver thread stopped for sandbox: " << sandbox_id_);
}

const char* transportTypeToString(ipc::TransportType type) {
    switch (type) {
        case ipc::TransportType::SHARED_MEMORY: return "SHARED_MEMORY";
        case ipc::TransportType::UNIX_SOCKET: return "UNIX_SOCKET";
        case ipc::TransportType::TCP_SOCKET: return "TCP_SOCKET";
        case ipc::TransportType::UDP_SOCKET: return "UDP_SOCKET";
        case ipc::TransportType::GRPC: return "GRPC";
        default: return "UNKNOWN";
    }
}

} // namespace security
} // namespace cdmf
