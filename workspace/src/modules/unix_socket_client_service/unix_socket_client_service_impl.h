#pragma once
#include "unix_socket_client_service.h"
#include "ipc/service_proxy.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>

namespace cdmf::services {

class UnixSocketClientServiceImpl : public IUnixSocketClientService {
public:
    UnixSocketClientServiceImpl();
    ~UnixSocketClientServiceImpl() override;

    void start();
    void stop();

    // IUnixSocketClientService interface
    bool sendData(const void* data, size_t size) override;
    bool echoRequest(const void* data, size_t size, void* response,
                    size_t response_size, size_t& bytes_received) override;
    std::string getRemoteStatus() override;
    std::string getRemoteStatistics() override;
    std::string getStatus() const override;
    std::string getStatistics() const override;

private:
    void testThreadFunc();

    mutable std::mutex mutex_;
    bool running_ = false;

    // IPC proxy for calling server methods
    cdmf::ipc::ServiceProxyPtr server_proxy_;

    // Test thread
    std::thread test_thread_;

    // Statistics
    std::atomic<uint64_t> request_count_{0};
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> echo_count_{0};
    std::atomic<uint64_t> request_failures_{0};
};

}
