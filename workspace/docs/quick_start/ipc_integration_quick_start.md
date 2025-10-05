# IPC Integration Quick Start Guide

**Version:** 1.0.0
**Date:** 2025-10-05
**Purpose:** Simple step-by-step guide to integrate IPC in your service

---

## Overview

This guide shows you how to add IPC communication to your CDMF service in 5 simple steps. Choose whether your service is a **Server** (provides functionality) or **Client** (calls other services).

---

## Quick Decision: Which Transport?

```
┌─────────────────────────────────────────────────────────┐
│  Same Machine?                                          │
│  ├─ Yes → High Frequency (>1000 msg/s)?                 │
│  │         ├─ Yes → SHARED_MEMORY                       │
│  │         └─ No  → UNIX_SOCKET                         │
│  └─ No  → GRPC                                          │
└─────────────────────────────────────────────────────────┘
```

---

## Server Side: Expose Your Service via IPC

### Step 1: Add IPC Headers

```cpp
// your_service_impl.h
#include "ipc/service_stub.h"
#include "ipc/message.h"
#include <memory>

using namespace cdmf::ipc;
```

### Step 2: Add Stub Member Variable

```cpp
class YourServiceImpl : public IYourService {
public:
    void start();
    void stop();

private:
    ServiceStubPtr ipc_stub_;  // Add this
};
```

### Step 3: Create and Configure Stub

```cpp
// your_service_impl.cpp
void YourServiceImpl::start() {
    // 1. Configure stub
    StubConfig config;
    config.service_name = "YourService";
    config.max_concurrent_requests = 100;

    // 2. Configure transport (choose one)

    // Option A: Unix Socket (moderate frequency, local)
    config.transport_config.type = TransportType::UNIX_SOCKET;
    config.transport_config.endpoint = "/tmp/your_service.sock";
    config.serialization_format = SerializationFormat::BINARY;

    // Option B: Shared Memory (high frequency, local)
    // config.transport_config.type = TransportType::SHARED_MEMORY;
    // config.transport_config.endpoint = "/cdmf_your_service_shm";
    // config.serialization_format = SerializationFormat::FLATBUFFERS;

    // Option C: gRPC (cross-network)
    // config.transport_config.type = TransportType::GRPC;
    // config.transport_config.endpoint = "0.0.0.0:50051";
    // config.serialization_format = SerializationFormat::PROTOBUF;

    // 3. Create stub
    ipc_stub_ = std::make_shared<ServiceStub>(config);

    // 4. Register methods (see Step 4)
    registerIPCMethods();

    // 5. Start stub
    ipc_stub_->start();

    LOGI("YourService IPC stub started");
}
```

### Step 4: Register Your Methods

```cpp
void YourServiceImpl::registerIPCMethods() {
    // Simple echo method
    ipc_stub_->registerMethod("echo",
        [](const std::vector<uint8_t>& request) {
            return request; // Echo back
        });

    // Method with processing
    ipc_stub_->registerMethod("processData",
        [this](const std::vector<uint8_t>& request) {
            // Deserialize
            auto data = deserializeData(request);

            // Process
            auto result = this->processYourData(data);

            // Serialize and return
            return serializeResult(result);
        });

    // Method returning status
    ipc_stub_->registerMethod("getStatus",
        [this](const std::vector<uint8_t>&) {
            std::string status = this->getStatus();
            return std::vector<uint8_t>(status.begin(), status.end());
        });
}
```

### Step 5: Stop Stub on Shutdown

```cpp
void YourServiceImpl::stop() {
    if (ipc_stub_) {
        ipc_stub_->stop();
    }
    LOGI("YourService IPC stub stopped");
}
```

### Complete Server Example

```cpp
// your_service_impl.h
#pragma once
#include "your_service.h"
#include "ipc/service_stub.h"

class YourServiceImpl : public IYourService {
public:
    YourServiceImpl();
    ~YourServiceImpl();

    void start();
    void stop();

    // Your business methods
    std::string getStatus();
    void processYourData(const Data& data);

private:
    void registerIPCMethods();

    cdmf::ipc::ServiceStubPtr ipc_stub_;
};

// your_service_impl.cpp
#include "your_service_impl.h"
#include "utils/log.h"

YourServiceImpl::YourServiceImpl() {
    LOGI("YourServiceImpl constructed");
}

YourServiceImpl::~YourServiceImpl() {
    if (ipc_stub_) {
        stop();
    }
}

void YourServiceImpl::start() {
    using namespace cdmf::ipc;

    // Configure
    StubConfig config;
    config.service_name = "YourService";
    config.transport_config.type = TransportType::UNIX_SOCKET;
    config.transport_config.endpoint = "/tmp/your_service.sock";
    config.serialization_format = SerializationFormat::BINARY;
    config.max_concurrent_requests = 100;

    // Create and start
    ipc_stub_ = std::make_shared<ServiceStub>(config);
    registerIPCMethods();
    ipc_stub_->start();

    LOGI("YourService started");
}

void YourServiceImpl::registerIPCMethods() {
    ipc_stub_->registerMethod("getStatus",
        [this](const std::vector<uint8_t>&) {
            std::string status = this->getStatus();
            return std::vector<uint8_t>(status.begin(), status.end());
        });
}

void YourServiceImpl::stop() {
    if (ipc_stub_) {
        ipc_stub_->stop();
    }
}
```

---

## Client Side: Call Other Services via IPC

### Step 1: Add IPC Headers

```cpp
// your_client_service.h
#include "ipc/service_proxy.h"
#include <memory>

using namespace cdmf::ipc;
```

### Step 2: Add Proxy Member Variable

```cpp
class YourClientService {
public:
    void start();
    void stop();
    void callRemoteService();

private:
    ServiceProxyPtr remote_service_proxy_;  // Add this
};
```

### Step 3: Create and Connect Proxy

```cpp
// your_client_service.cpp
void YourClientService::start() {
    // 1. Configure proxy
    ProxyConfig config;
    config.service_name = "YourClient";
    config.default_timeout_ms = 5000;  // 5 second timeout
    config.auto_reconnect = true;

    // 2. Configure transport (must match server!)

    // Option A: Unix Socket
    config.transport_config.type = TransportType::UNIX_SOCKET;
    config.transport_config.endpoint = "/tmp/remote_service.sock";
    config.serialization_format = SerializationFormat::BINARY;

    // Option B: Shared Memory
    // config.transport_config.type = TransportType::SHARED_MEMORY;
    // config.transport_config.endpoint = "/cdmf_remote_service_shm";
    // config.serialization_format = SerializationFormat::FLATBUFFERS;

    // Option C: gRPC
    // config.transport_config.type = TransportType::GRPC;
    // config.transport_config.endpoint = "localhost:50051";
    // config.serialization_format = SerializationFormat::PROTOBUF;

    // 3. Create proxy
    remote_service_proxy_ = std::make_shared<ServiceProxy>(config);

    // 4. Connect
    if (!remote_service_proxy_->connect()) {
        LOGE("Failed to connect to remote service");
        return;
    }

    LOGI("Connected to remote service");
}
```

### Step 4: Call Remote Methods

```cpp
// Synchronous call (blocks until response)
void YourClientService::callRemoteService() {
    // Prepare request
    std::string request = "Hello, Service!";
    std::vector<uint8_t> requestData(request.begin(), request.end());

    // Make call
    auto result = remote_service_proxy_->call("echo", requestData, 1000);

    // Check result
    if (result.success) {
        std::string response(result.data.begin(), result.data.end());
        LOGI("Received: " << response);
    } else {
        LOGE("Call failed: " << result.error_message);
    }
}

// Asynchronous call (non-blocking)
void YourClientService::callRemoteServiceAsync() {
    std::vector<uint8_t> requestData = prepareRequest();

    // Start async call
    auto future = remote_service_proxy_->callAsync("processData", requestData);

    // Do other work here...
    doOtherWork();

    // Get result when ready (blocks if not ready)
    auto result = future.get();

    if (result.success) {
        processResponse(result.data);
    }
}

// One-way call (fire-and-forget)
void YourClientService::sendEvent() {
    std::vector<uint8_t> eventData = prepareEvent();

    // Send without waiting for response
    remote_service_proxy_->callOneWay("logEvent", eventData);
}
```

### Step 5: Disconnect on Shutdown

```cpp
void YourClientService::stop() {
    if (remote_service_proxy_) {
        remote_service_proxy_->disconnect();
    }
    LOGI("Disconnected from remote service");
}
```

### Complete Client Example

```cpp
// your_client_service.h
#pragma once
#include "ipc/service_proxy.h"

class YourClientService {
public:
    YourClientService();
    ~YourClientService();

    void start();
    void stop();
    std::string getRemoteStatus();

private:
    cdmf::ipc::ServiceProxyPtr remote_service_proxy_;
};

// your_client_service.cpp
#include "your_client_service.h"
#include "utils/log.h"

YourClientService::YourClientService() {
    LOGI("YourClientService constructed");
}

YourClientService::~YourClientService() {
    if (remote_service_proxy_) {
        stop();
    }
}

void YourClientService::start() {
    using namespace cdmf::ipc;

    // Configure
    ProxyConfig config;
    config.service_name = "YourClient";
    config.transport_config.type = TransportType::UNIX_SOCKET;
    config.transport_config.endpoint = "/tmp/remote_service.sock";
    config.serialization_format = SerializationFormat::BINARY;
    config.default_timeout_ms = 5000;
    config.auto_reconnect = true;

    // Create and connect
    remote_service_proxy_ = std::make_shared<ServiceProxy>(config);

    if (!remote_service_proxy_->connect()) {
        LOGE("Failed to connect to remote service");
        return;
    }

    LOGI("YourClientService started");
}

std::string YourClientService::getRemoteStatus() {
    std::vector<uint8_t> empty;
    auto result = remote_service_proxy_->call("getStatus", empty);

    if (result.success) {
        return std::string(result.data.begin(), result.data.end());
    } else {
        LOGE("Failed to get status: " << result.error_message);
        return "ERROR";
    }
}

void YourClientService::stop() {
    if (remote_service_proxy_) {
        remote_service_proxy_->disconnect();
    }
}
```

---

## Common Patterns

### Pattern 1: Simple Data Exchange (Strings)

**Server:**
```cpp
ipc_stub_->registerMethod("echo",
    [](const std::vector<uint8_t>& request) {
        // Request is already bytes, just echo
        return request;
    });
```

**Client:**
```cpp
std::string message = "Hello";
std::vector<uint8_t> request(message.begin(), message.end());

auto result = proxy_->call("echo", request);

if (result.success) {
    std::string response(result.data.begin(), result.data.end());
    std::cout << response << std::endl;
}
```

### Pattern 2: Structured Data (Integers, Structs)

**Server:**
```cpp
ipc_stub_->registerMethod("add",
    [](const std::vector<uint8_t>& request) {
        // Deserialize two integers
        int32_t a = *reinterpret_cast<const int32_t*>(request.data());
        int32_t b = *reinterpret_cast<const int32_t*>(request.data() + 4);

        // Calculate
        int32_t sum = a + b;

        // Serialize result
        std::vector<uint8_t> response(sizeof(int32_t));
        std::memcpy(response.data(), &sum, sizeof(int32_t));
        return response;
    });
```

**Client:**
```cpp
int32_t a = 10;
int32_t b = 20;

// Serialize request
std::vector<uint8_t> request(sizeof(int32_t) * 2);
std::memcpy(request.data(), &a, sizeof(int32_t));
std::memcpy(request.data() + 4, &b, sizeof(int32_t));

// Call
auto result = proxy_->call("add", request);

if (result.success) {
    int32_t sum = *reinterpret_cast<const int32_t*>(result.data.data());
    std::cout << "Sum: " << sum << std::endl;
}
```

### Pattern 3: Using Protobuf (Recommended for Complex Data)

**Define proto file (my_service.proto):**
```protobuf
syntax = "proto3";

message DataRequest {
    string name = 1;
    int32 value = 2;
}

message DataResponse {
    string result = 1;
    bool success = 2;
}
```

**Server:**
```cpp
ipc_stub_->registerMethod("processData",
    [this](const std::vector<uint8_t>& request) {
        // Deserialize protobuf request
        DataRequest req;
        req.ParseFromArray(request.data(), request.size());

        // Process
        auto result = this->process(req.name(), req.value());

        // Serialize protobuf response
        DataResponse resp;
        resp.set_result(result);
        resp.set_success(true);

        std::vector<uint8_t> response(resp.ByteSizeLong());
        resp.SerializeToArray(response.data(), response.size());
        return response;
    });
```

**Client:**
```cpp
// Create protobuf request
DataRequest req;
req.set_name("test");
req.set_value(42);

// Serialize
std::vector<uint8_t> request(req.ByteSizeLong());
req.SerializeToArray(request.data(), request.size());

// Call
auto result = proxy_->call("processData", request);

if (result.success) {
    // Deserialize protobuf response
    DataResponse resp;
    resp.ParseFromArray(result.data.data(), result.data.size());

    std::cout << "Result: " << resp.result() << std::endl;
}
```

### Pattern 4: Error Handling

```cpp
// Client side error handling
auto result = proxy_->call("someMethod", request, 5000);

if (!result.success) {
    switch (result.error_code) {
        case 1001:  // METHOD_NOT_FOUND
            LOGE("Method not found on server");
            break;
        case 1004:  // HANDLER_EXCEPTION
            LOGE("Server handler threw exception: " << result.error_message);
            break;
        case 3:     // TIMEOUT
            LOGE("Call timed out");
            break;
        default:
            LOGE("Unknown error: " << result.error_code << " - " << result.error_message);
    }
}
```

### Pattern 5: Retry on Failure

```cpp
// Configure retry policy
ProxyConfig config;
config.retry_policy.enabled = true;
config.retry_policy.max_attempts = 3;
config.retry_policy.initial_delay_ms = 100;
config.retry_policy.exponential_backoff = true;

auto proxy = std::make_shared<ServiceProxy>(config);
proxy->connect();

// Calls will automatically retry on failure
auto result = proxy->call("unreliableMethod", request);
```

---

## CMakeLists.txt Integration

Add IPC framework to your service's CMakeLists.txt:

```cmake
# your_service/CMakeLists.txt

# Link IPC libraries
target_link_libraries(your_service
    PRIVATE
        cdmf::ipc          # Core IPC framework
        cdmf::transport    # Transport layer
        cdmf::serializer   # Serialization
)

# If using protobuf
find_package(Protobuf REQUIRED)
target_link_libraries(your_service
    PRIVATE
        ${Protobuf_LIBRARIES}
)

# If using FlatBuffers
find_package(flatbuffers REQUIRED)
target_link_libraries(your_service
    PRIVATE
        flatbuffers::flatbuffers
)
```

---

## Checklist

### Server (Service Provider)

- [ ] Add `#include "ipc/service_stub.h"`
- [ ] Add `ServiceStubPtr` member variable
- [ ] Create `StubConfig` in `start()`
- [ ] Choose transport type (UNIX_SOCKET, SHARED_MEMORY, or GRPC)
- [ ] Set endpoint path/address
- [ ] Create `ServiceStub` instance
- [ ] Register all methods with handlers
- [ ] Call `stub_->start()`
- [ ] Call `stub_->stop()` in shutdown

### Client (Service Consumer)

- [ ] Add `#include "ipc/service_proxy.h"`
- [ ] Add `ServiceProxyPtr` member variable
- [ ] Create `ProxyConfig` in `start()`
- [ ] Choose transport type (must match server!)
- [ ] Set endpoint path/address (must match server!)
- [ ] Create `ServiceProxy` instance
- [ ] Call `proxy_->connect()`
- [ ] Make calls with `proxy_->call()`, `callAsync()`, or `callOneWay()`
- [ ] Check `result.success` before using data
- [ ] Call `proxy_->disconnect()` in shutdown

---

## Troubleshooting

### Connection Failed

```
Error: Failed to connect to remote service
```

**Solutions:**
1. Ensure server is started before client
2. Check endpoint matches exactly (server and client)
3. Check transport type matches (server and client)
4. For Unix socket: Check file permissions
5. For shared memory: Check `/dev/shm/` permissions
6. Add delay between server start and client connect:
   ```cpp
   stub_->start();
   std::this_thread::sleep_for(std::chrono::milliseconds(100));
   ```

### Method Not Found (Error 1001)

```
Error code: 1001 - Method not found
```

**Solutions:**
1. Check method name spelling (case-sensitive!)
2. Ensure method is registered on server before client calls
3. Verify `registerMethod()` was called

### Timeout (Error 3)

```
Error code: 3 - Call timeout
```

**Solutions:**
1. Increase timeout: `proxy_->call("method", data, 10000);` (10 seconds)
2. Check server is processing requests
3. Check network latency (for gRPC)
4. Optimize handler performance

### Serialization Error

```
Error: Failed to deserialize response
```

**Solutions:**
1. Ensure serialization format matches (server and client)
2. Check data size matches expected structure
3. Use protobuf for complex data instead of raw bytes
4. Validate data before serializing

---

## Next Steps

1. **Test your integration:**
   - Start server service
   - Start client service
   - Make test calls
   - Check logs for errors

2. **Add error handling:**
   - Check `result.success` on all calls
   - Log errors with context
   - Implement retry logic

3. **Optimize performance:**
   - Choose appropriate transport
   - Use async calls for non-blocking
   - Monitor statistics: `proxy->getStats()`

4. **Add reliability:**
   - Enable retry policy
   - Use circuit breakers for failing services
   - Implement health checks

For detailed reference, see:
- **Full Documentation:** `workspace/docs/architecture/ipc_framework_integration_guide.md`
- **Test Examples:** `workspace/tests/unit/test_proxy_stub.cpp`
- **Service Examples:** `workspace/src/modules/shm_*_service/`
