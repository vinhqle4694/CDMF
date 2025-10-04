/**
 * @file transport.cpp
 * @brief Transport base implementation and factory
 *
 * @version 1.0.0
 * @date 2025-10-03
 */

#include "ipc/transport.h"
#include "ipc/unix_socket_transport.h"
#include "ipc/shared_memory_transport.h"
#include "ipc/grpc_transport.h"
#include "utils/log.h"
#include <map>
#include <mutex>

namespace cdmf {
namespace ipc {

// Static factory registry
static std::map<TransportType, TransportFactory::TransportCreator> g_transport_registry;
static std::mutex g_registry_mutex;

TransportPtr TransportFactory::create(TransportType type) {
    LOGD_FMT("TransportFactory::create - type: " << static_cast<int>(type));

    // Check custom registered transports first
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        auto it = g_transport_registry.find(type);
        if (it != g_transport_registry.end()) {
            LOGD_FMT("Creating custom transport from registry");
            return it->second();
        }
    }

    // Built-in transports
    switch (type) {
        case TransportType::UNIX_SOCKET:
            LOGD_FMT("Creating UnixSocketTransport");
            return std::make_shared<UnixSocketTransport>();

        case TransportType::SHARED_MEMORY:
            LOGD_FMT("Creating SharedMemoryTransport");
            return std::make_shared<SharedMemoryTransport>();

        case TransportType::GRPC:
            LOGD_FMT("Creating GrpcTransport");
            return std::make_shared<GrpcTransport>();

        default:
            LOGE_FMT("Unknown transport type: " << static_cast<int>(type));
            return nullptr;
    }
}

TransportPtr TransportFactory::create(const TransportConfig& config) {
    LOGI_FMT("TransportFactory::create - type: " << static_cast<int>(config.type)
             << ", endpoint: " << config.endpoint);

    auto transport = create(config.type);
    if (transport) {
        auto result = transport->init(config);
        if (!result.success()) {
            LOGE_FMT("Transport initialization failed: " << result.error_message);
            return nullptr;
        }
        LOGI_FMT("Transport created and initialized successfully");
    }
    return transport;
}

void TransportFactory::registerTransport(TransportType type, TransportCreator creator) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    g_transport_registry[type] = creator;
}

const char* transportErrorToString(TransportError error) {
    switch (error) {
        case TransportError::SUCCESS: return "SUCCESS";
        case TransportError::NOT_INITIALIZED: return "NOT_INITIALIZED";
        case TransportError::ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case TransportError::NOT_CONNECTED: return "NOT_CONNECTED";
        case TransportError::ALREADY_CONNECTED: return "ALREADY_CONNECTED";
        case TransportError::CONNECTION_FAILED: return "CONNECTION_FAILED";
        case TransportError::CONNECTION_CLOSED: return "CONNECTION_CLOSED";
        case TransportError::CONNECTION_TIMEOUT: return "CONNECTION_TIMEOUT";
        case TransportError::SEND_FAILED: return "SEND_FAILED";
        case TransportError::RECV_FAILED: return "RECV_FAILED";
        case TransportError::TIMEOUT: return "TIMEOUT";
        case TransportError::INVALID_CONFIG: return "INVALID_CONFIG";
        case TransportError::INVALID_MESSAGE: return "INVALID_MESSAGE";
        case TransportError::BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
        case TransportError::SERIALIZATION_ERROR: return "SERIALIZATION_ERROR";
        case TransportError::DESERIALIZATION_ERROR: return "DESERIALIZATION_ERROR";
        case TransportError::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case TransportError::PERMISSION_DENIED: return "PERMISSION_DENIED";
        case TransportError::ENDPOINT_NOT_FOUND: return "ENDPOINT_NOT_FOUND";
        case TransportError::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
        case TransportError::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        default: return "UNKNOWN";
    }
}

const char* transportTypeToString(TransportType type) {
    switch (type) {
        case TransportType::UNIX_SOCKET: return "UNIX_SOCKET";
        case TransportType::SHARED_MEMORY: return "SHARED_MEMORY";
        case TransportType::GRPC: return "GRPC";
        case TransportType::TCP_SOCKET: return "TCP_SOCKET";
        case TransportType::UDP_SOCKET: return "UDP_SOCKET";
        case TransportType::UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

const char* transportStateToString(TransportState state) {
    switch (state) {
        case TransportState::UNINITIALIZED: return "UNINITIALIZED";
        case TransportState::INITIALIZED: return "INITIALIZED";
        case TransportState::CONNECTING: return "CONNECTING";
        case TransportState::CONNECTED: return "CONNECTED";
        case TransportState::DISCONNECTING: return "DISCONNECTING";
        case TransportState::DISCONNECTED: return "DISCONNECTED";
        case TransportState::ERROR: return "ERROR";
        default: return "INVALID";
    }
}

} // namespace ipc
} // namespace cdmf
