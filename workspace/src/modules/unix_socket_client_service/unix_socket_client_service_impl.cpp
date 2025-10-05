#include "unix_socket_client_service_impl.h"
#include "utils/log.h"
#include <sstream>
#include <cstring>
#include <chrono>

namespace cdmf::services {

UnixSocketClientServiceImpl::UnixSocketClientServiceImpl() {
    LOGI("UnixSocketClientServiceImpl constructed");
}

UnixSocketClientServiceImpl::~UnixSocketClientServiceImpl() {
    if (running_) {
        stop();
    }
    LOGI("UnixSocketClientServiceImpl destroyed");
}

void UnixSocketClientServiceImpl::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        LOGW("UnixSocketClientService already running");
        return;
    }

    // Configure IPC proxy to connect to server
    using namespace cdmf::ipc;
    ProxyConfig config;
    config.service_name = "UnixSocketClientService";
    config.default_timeout_ms = 5000;  // 5 second timeout
    config.auto_reconnect = true;

    // Configure Unix Socket transport (must match server!)
    config.transport_config.type = TransportType::UNIX_SOCKET;
    config.transport_config.endpoint = "/tmp/cdmf_unix_socket_server.sock";
    config.serialization_format = SerializationFormat::BINARY;

    // Create proxy (connection will be attempted later or via auto-reconnect)
    server_proxy_ = std::make_shared<ServiceProxy>(config);

    // Try to connect, but don't fail if server isn't ready yet
    if (!server_proxy_->connect()) {
        LOGW("Failed to connect to UnixSocketServerService initially - will retry via auto-reconnect");
        // Don't return here - continue starting the service
    } else {
        LOGI("Successfully connected to UnixSocketServerService via IPC");
    }

    running_ = true;

    // Start test thread
    test_thread_ = std::thread(&UnixSocketClientServiceImpl::testThreadFunc, this);

    LOGI("UnixSocketClientService started (auto-reconnect enabled)");
}

void UnixSocketClientServiceImpl::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }

    // Wait for test thread to finish
    if (test_thread_.joinable()) {
        test_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Disconnect from server
        if (server_proxy_) {
            server_proxy_->disconnect();
        }
    }

    LOGI("UnixSocketClientService stopped");
}

bool UnixSocketClientServiceImpl::sendData(const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        LOGE("UnixSocketClientService not running");
        request_failures_++;
        return false;
    }

    if (!data || size == 0) {
        LOGE("Invalid sendData parameters");
        request_failures_++;
        return false;
    }

    if (!server_proxy_) {
        LOGE("Server proxy not initialized");
        request_failures_++;
        return false;
    }

    // Prepare request
    std::vector<uint8_t> request(size);
    std::memcpy(request.data(), data, size);

    // Call server's processData method
    auto result = server_proxy_->call("processData", request, 1000);

    if (!result.success) {
        LOGE_FMT("Failed to send data: " << result.error_message);
        request_failures_++;
        return false;
    }

    // Check response (1 byte: 0=failure, 1=success)
    bool success = (result.data.size() > 0 && result.data[0] == 1);

    if (success) {
        request_count_++;
        bytes_sent_ += size;
        LOGD_FMT("Successfully sent " << size << " bytes to server");
    } else {
        request_failures_++;
        LOGE("Server failed to process data");
    }

    return success;
}

bool UnixSocketClientServiceImpl::echoRequest(const void* data, size_t size,
                                             void* response, size_t response_size,
                                             size_t& bytes_received) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        LOGE("UnixSocketClientService not running");
        request_failures_++;
        return false;
    }

    if (!data || size == 0 || !response || response_size == 0) {
        LOGE("Invalid echoRequest parameters");
        request_failures_++;
        return false;
    }

    if (!server_proxy_) {
        LOGE("Server proxy not initialized");
        request_failures_++;
        return false;
    }

    // Prepare request
    std::vector<uint8_t> request(size);
    std::memcpy(request.data(), data, size);

    // Call server's echo method
    auto result = server_proxy_->call("echo", request, 1000);

    if (!result.success) {
        LOGE_FMT("Echo request failed: " << result.error_message);
        request_failures_++;
        return false;
    }

    // Copy response
    bytes_received = std::min(response_size, result.data.size());
    if (bytes_received > 0) {
        std::memcpy(response, result.data.data(), bytes_received);
        echo_count_++;
        bytes_sent_ += size;
        LOGD_FMT("Echo successful: sent " << size << " bytes, received " << bytes_received << " bytes");
    }

    return true;
}

std::string UnixSocketClientServiceImpl::getRemoteStatus() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_ || !server_proxy_) {
        return "Client not running";
    }

    // Call server's getStatus method
    std::vector<uint8_t> empty;
    auto result = server_proxy_->call("getStatus", empty, 1000);

    if (!result.success) {
        LOGE_FMT("Failed to get remote status: " << result.error_message);
        return "Error: " + result.error_message;
    }

    return std::string(result.data.begin(), result.data.end());
}

std::string UnixSocketClientServiceImpl::getRemoteStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_ || !server_proxy_) {
        return "Client not running";
    }

    // Call server's getStatistics method
    std::vector<uint8_t> empty;
    auto result = server_proxy_->call("getStatistics", empty, 1000);

    if (!result.success) {
        LOGE_FMT("Failed to get remote statistics: " << result.error_message);
        return "Error: " + result.error_message;
    }

    return std::string(result.data.begin(), result.data.end());
}

std::string UnixSocketClientServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string local_status = running_ ? "Running" : "Stopped";

    // If running, check server connection
    if (running_ && server_proxy_) {
        bool connected = server_proxy_->isConnected();
        return local_status + (connected ? " (Connected)" : " (Disconnected)");
    }

    return local_status;
}

std::string UnixSocketClientServiceImpl::getStatistics() const {
    std::ostringstream oss;
    oss << "Unix Socket Client Statistics:\n"
        << "  Status: " << getStatus() << "\n"
        << "  Total Requests: " << request_count_.load() << "\n"
        << "  Echo Requests: " << echo_count_.load() << "\n"
        << "  Bytes Sent: " << bytes_sent_.load() << "\n"
        << "  Request Failures: " << request_failures_.load();
    return oss.str();
}

void UnixSocketClientServiceImpl::testThreadFunc() {
    LOGI("Test thread started - will periodically call server APIs");

    int test_cycle = 0;
    const size_t BUFFER_SIZE = 256;
    std::vector<uint8_t> response_buffer(BUFFER_SIZE);
    int reconnect_attempts = 0;

    while (running_) {
        // Try to reconnect if not connected
        if (!server_proxy_ || !server_proxy_->isConnected()) {
            if (reconnect_attempts < 10) {
                LOGI_FMT("Attempting to connect to server (attempt " << (reconnect_attempts + 1) << "/10)");
                if (server_proxy_ && server_proxy_->connect()) {
                    LOGI("Successfully connected to UnixSocketServerService");
                    reconnect_attempts = 0;  // Reset counter on success
                } else {
                    reconnect_attempts++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
            } else {
                LOGI("Waiting for server connection...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                reconnect_attempts = 0;  // Reset and try again
                continue;
            }
        }

        test_cycle++;
        LOGI_FMT("\n========== Test Cycle " << test_cycle << " ==========");

        // Test 1: Send processData request
        {
            std::string test_data = "Test data from client cycle " + std::to_string(test_cycle);
            LOGI_FMT("1. Calling processData with: \"" << test_data << "\"");

            bool result = sendData(test_data.c_str(), test_data.size());
            if (result) {
                LOGI("   ✓ processData succeeded");
            } else {
                LOGE("   ✗ processData failed");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Test 2: Send echo request
        {
            std::string echo_data = "Echo test " + std::to_string(test_cycle);
            size_t bytes_received = 0;
            LOGI_FMT("2. Calling echo with: \"" << echo_data << "\"");

            bool result = echoRequest(echo_data.c_str(), echo_data.size(),
                                    response_buffer.data(), BUFFER_SIZE, bytes_received);
            if (result && bytes_received > 0) {
                std::string echoed(response_buffer.begin(), response_buffer.begin() + bytes_received);
                LOGI_FMT("   ✓ echo succeeded, received: \"" << echoed << "\"");
            } else {
                LOGE("   ✗ echo failed");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Test 3: Get server status
        {
            LOGI("3. Calling getStatus");
            std::string status = getRemoteStatus();
            LOGI_FMT("   ✓ Server status: " << status);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Test 4: Get server statistics
        {
            LOGI("4. Calling getStatistics");
            std::string stats = getRemoteStatistics();
            LOGI_FMT("   ✓ Server statistics:\n" << stats);
        }

        LOGI("========== Test Cycle Complete ==========\n");

        // Wait 3 seconds before next test cycle
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    LOGI("Test thread stopped");
}

}
