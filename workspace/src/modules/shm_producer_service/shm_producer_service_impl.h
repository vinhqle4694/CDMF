#pragma once
#include "shm_producer_service.h"
#include "ipc/service_stub.h"
#include <mutex>
#include <atomic>
#include <memory>

namespace cdmf::services {

class ShmProducerServiceImpl : public IShmProducerService {
public:
    ShmProducerServiceImpl();
    ~ShmProducerServiceImpl() override;

    void start();
    void stop();

    // IShmProducerService interface
    bool write(const void* data, size_t size) override;
    size_t getAvailableSpace() const override;
    std::string getStatus() const override;
    std::string getStatistics() const override;

private:
    void registerIPCMethods();

    mutable std::mutex mutex_;
    bool running_ = false;

    // IPC stub for exposing methods
    cdmf::ipc::ServiceStubPtr ipc_stub_;

    // Statistics
    std::atomic<uint64_t> write_count_{0};
    std::atomic<uint64_t> bytes_written_{0};
    std::atomic<uint64_t> write_failures_{0};

    // Shared memory handle (placeholder - will be implemented based on actual SHM implementation)
    void* shm_handle_ = nullptr;
};

}
