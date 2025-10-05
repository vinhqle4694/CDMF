#pragma once
#include "shm_consumer_service.h"
#include "ipc/service_proxy.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>

namespace cdmf::services {

class ShmConsumerServiceImpl : public IShmConsumerService {
public:
    ShmConsumerServiceImpl();
    ~ShmConsumerServiceImpl() override;

    void start();
    void stop();

    // IShmConsumerService interface
    bool read(void* buffer, size_t buffer_size, size_t& bytes_read) override;
    void setDataCallback(DataCallback callback) override;
    size_t getAvailableData() const override;
    std::string getStatus() const override;
    std::string getStatistics() const override;

private:
    void consumerThreadFunc();

    mutable std::mutex mutex_;
    bool running_ = false;

    // IPC proxy for calling producer methods
    cdmf::ipc::ServiceProxyPtr producer_proxy_;

    // Callback
    DataCallback data_callback_;

    // Consumer thread
    std::thread consumer_thread_;

    // Statistics
    std::atomic<uint64_t> read_count_{0};
    std::atomic<uint64_t> bytes_read_{0};
    std::atomic<uint64_t> read_failures_{0};

    // Shared memory handle (placeholder - will be implemented based on actual SHM implementation)
    void* shm_handle_ = nullptr;
};

}
