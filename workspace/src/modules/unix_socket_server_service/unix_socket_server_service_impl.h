#pragma once
#include "unix_socket_server_service.h"
#include "ipc/service_stub.h"
#include <mutex>
#include <atomic>
#include <memory>

namespace cdmf::services {

class UnixSocketServerServiceImpl : public IUnixSocketServerService {
public:
    UnixSocketServerServiceImpl();
    ~UnixSocketServerServiceImpl() override;

    void start();
    void stop();

    // IUnixSocketServerService interface
    bool processData(const void* data, size_t size) override;
    size_t echo(const void* data, size_t size, void* response, size_t response_size) override;
    std::string getStatus() const override;
    std::string getStatistics() const override;

private:
    void registerIPCMethods();

    mutable std::mutex mutex_;
    bool running_ = false;

    // IPC stub for exposing methods via Unix Socket
    cdmf::ipc::ServiceStubPtr ipc_stub_;

    // Statistics
    std::atomic<uint64_t> request_count_{0};
    std::atomic<uint64_t> bytes_processed_{0};
    std::atomic<uint64_t> echo_count_{0};
    std::atomic<uint64_t> process_failures_{0};
};

}
