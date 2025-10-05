#include "shm_producer_service_impl.h"
#include "utils/log.h"
#include <sstream>
#include <cstring>

namespace cdmf::services {

ShmProducerServiceImpl::ShmProducerServiceImpl() {
    LOGI("ShmProducerServiceImpl constructed");
}

ShmProducerServiceImpl::~ShmProducerServiceImpl() {
    if (running_) {
        stop();
    }
    LOGI("ShmProducerServiceImpl destroyed");
}

void ShmProducerServiceImpl::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        LOGW("ShmProducerService already running");
        return;
    }

    // TODO: Initialize shared memory ring buffer here
    // shm_handle_ = create_or_open_shm(...);

    // Configure IPC stub
    using namespace cdmf::ipc;
    StubConfig config;
    config.service_name = "ShmProducerService";
    config.max_concurrent_requests = 100;

    // Configure shared memory transport (high frequency, local)
    config.transport_config.type = TransportType::SHARED_MEMORY;
    config.transport_config.endpoint = "/cdmf_shm_producer_service";
    config.transport_config.properties["create_shm"] = "true";  // Producer creates shared memory
    config.transport_config.properties["bidirectional"] = "true";  // Enable bidirectional communication
    config.serialization_format = SerializationFormat::BINARY;

    // Create and configure stub
    ipc_stub_ = std::make_shared<ServiceStub>(config);
    registerIPCMethods();
    ipc_stub_->start();

    running_ = true;
    LOGI("ShmProducerService started with IPC stub");
}

void ShmProducerServiceImpl::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }

    // Stop IPC stub
    if (ipc_stub_) {
        ipc_stub_->stop();
    }

    // TODO: Cleanup shared memory resources
    // if (shm_handle_) { close_shm(shm_handle_); }

    running_ = false;
    LOGI("ShmProducerService stopped");
}

bool ShmProducerServiceImpl::write(const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        LOGE("ShmProducerService not running");
        write_failures_++;
        return false;
    }

    if (!data || size == 0) {
        LOGE("Invalid write parameters");
        write_failures_++;
        return false;
    }

    // TODO: Implement actual shared memory write
    // For now, just simulate success
    // bool success = shm_ring_buffer_write(shm_handle_, data, size);
    bool success = true;

    if (success) {
        write_count_++;
        bytes_written_ += size;
        LOGD_FMT("Written " << size << " bytes to shared memory");
    } else {
        write_failures_++;
        LOGE_FMT("Failed to write " << size << " bytes to shared memory");
    }

    return success;
}

size_t ShmProducerServiceImpl::getAvailableSpace() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
        return 0;
    }

    // TODO: Query actual ring buffer available space
    // return shm_ring_buffer_available_space(shm_handle_);
    return 1024 * 1024; // Placeholder: 1MB
}

std::string ShmProducerServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ ? "Running" : "Stopped";
}

std::string ShmProducerServiceImpl::getStatistics() const {
    std::ostringstream oss;
    oss << "Shared Memory Producer Statistics:\n"
        << "  Status: " << getStatus() << "\n"
        << "  Total Writes: " << write_count_.load() << "\n"
        << "  Bytes Written: " << bytes_written_.load() << "\n"
        << "  Write Failures: " << write_failures_.load() << "\n"
        << "  Available Space: " << getAvailableSpace() << " bytes";
    return oss.str();
}

void ShmProducerServiceImpl::registerIPCMethods() {
    // Register write method
    ipc_stub_->registerMethod("write",
        [this](const std::vector<uint8_t>& request) {
            // Request format: data bytes
            bool success = this->write(request.data(), request.size());

            // Response format: 1 byte (0=failure, 1=success)
            std::vector<uint8_t> response(1);
            response[0] = success ? 1 : 0;
            return response;
        });

    // Register getAvailableSpace method
    ipc_stub_->registerMethod("getAvailableSpace",
        [this](const std::vector<uint8_t>&) {
            size_t space = this->getAvailableSpace();

            // Serialize size_t to bytes
            std::vector<uint8_t> response(sizeof(size_t));
            std::memcpy(response.data(), &space, sizeof(size_t));
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

    LOGI("IPC methods registered for ShmProducerService");
}

}
