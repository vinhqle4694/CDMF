#include "unix_socket_server_service_impl.h"
#include "utils/log.h"
#include <sstream>
#include <cstring>

namespace cdmf::services {

UnixSocketServerServiceImpl::UnixSocketServerServiceImpl() {
    LOGI("UnixSocketServerServiceImpl constructed");
}

UnixSocketServerServiceImpl::~UnixSocketServerServiceImpl() {
    if (running_) {
        stop();
    }
    LOGI("UnixSocketServerServiceImpl destroyed");
}

void UnixSocketServerServiceImpl::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        LOGW("UnixSocketServerService already running");
        return;
    }

    // Configure IPC stub
    using namespace cdmf::ipc;
    StubConfig config;
    config.service_name = "UnixSocketServerService";
    config.max_concurrent_requests = 100;

    // Configure Unix Socket transport (moderate frequency, local)
    config.transport_config.type = TransportType::UNIX_SOCKET;
    config.transport_config.mode = TransportMode::ASYNC;  // ASYNC mode for I/O thread
    config.transport_config.endpoint = "/tmp/cdmf_unix_socket_server.sock";
    config.transport_config.properties["is_server"] = "true";  // Server mode: listen for connections
    config.serialization_format = SerializationFormat::BINARY;

    // Create and configure stub
    ipc_stub_ = std::make_shared<ServiceStub>(config);
    registerIPCMethods();
    ipc_stub_->start();

    running_ = true;
    LOGI("UnixSocketServerService started with IPC stub on /tmp/cdmf_unix_socket_server.sock");
}

void UnixSocketServerServiceImpl::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }

    // Stop IPC stub
    if (ipc_stub_) {
        ipc_stub_->stop();
    }

    running_ = false;
    LOGI("UnixSocketServerService stopped");
}

bool UnixSocketServerServiceImpl::processData(const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        LOGE("UnixSocketServerService not running");
        process_failures_++;
        return false;
    }

    if (!data || size == 0) {
        LOGE("Invalid processData parameters");
        process_failures_++;
        return false;
    }

    // Simulate data processing
    request_count_++;
    bytes_processed_ += size;
    LOGD_FMT("Processed " << size << " bytes of data");

    return true;
}

size_t UnixSocketServerServiceImpl::echo(const void* data, size_t size, void* response, size_t response_size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        LOGE("UnixSocketServerService not running");
        return 0;
    }

    if (!data || size == 0 || !response || response_size == 0) {
        LOGE("Invalid echo parameters");
        return 0;
    }

    // Echo back the data
    size_t copy_size = std::min(size, response_size);
    std::memcpy(response, data, copy_size);

    echo_count_++;
    bytes_processed_ += copy_size;
    LOGD_FMT("Echoed " << copy_size << " bytes");

    return copy_size;
}

std::string UnixSocketServerServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ ? "Running" : "Stopped";
}

std::string UnixSocketServerServiceImpl::getStatistics() const {
    std::ostringstream oss;
    oss << "Unix Socket Server Statistics:\n"
        << "  Status: " << getStatus() << "\n"
        << "  Total Requests: " << request_count_.load() << "\n"
        << "  Echo Requests: " << echo_count_.load() << "\n"
        << "  Bytes Processed: " << bytes_processed_.load() << "\n"
        << "  Process Failures: " << process_failures_.load();
    return oss.str();
}

void UnixSocketServerServiceImpl::registerIPCMethods() {
    // Register processData method
    ipc_stub_->registerMethod("processData",
        [this](const std::vector<uint8_t>& request) {
            // Request format: data bytes
            bool success = this->processData(request.data(), request.size());

            // Response format: 1 byte (0=failure, 1=success)
            std::vector<uint8_t> response(1);
            response[0] = success ? 1 : 0;
            return response;
        });

    // Register echo method
    ipc_stub_->registerMethod("echo",
        [this](const std::vector<uint8_t>& request) {
            // Request format: data bytes to echo
            std::vector<uint8_t> response(request.size());
            size_t echoed = this->echo(request.data(), request.size(),
                                      response.data(), response.size());

            // Response format: echoed data
            response.resize(echoed);
            return response;
        });

    // Register getStatus method
    ipc_stub_->registerMethod("getStatus",
        [this](const std::vector<uint8_t>&) {
            std::string status = this->getStatus();
            return std::vector<uint8_t>(status.begin(), status.end());
        });

    // Register getStatistics method
    ipc_stub_->registerMethod("getStatistics",
        [this](const std::vector<uint8_t>&) {
            std::string stats = this->getStatistics();
            return std::vector<uint8_t>(stats.begin(), stats.end());
        });

    LOGI("IPC methods registered for UnixSocketServerService");
}

}
