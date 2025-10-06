# Process Sandbox Implementation Guide (Pluggable IPC)

## Overview

This document explains how to implement actual process sandboxing where modules run in isolated processes with resource limits and pluggable IPC communication.

## Key Architecture Decisions

### 1. Strategy Pattern for IPC Transport
Use **Strategy pattern** for IPC transport selection, allowing runtime selection via configuration:
- **Shared Memory**: High bandwidth (>1 GB/s), low latency (1-5 μs) - **recommended for production/same-device**
- **Unix Sockets**: Moderate bandwidth (200-400 MB/s), low setup overhead - **good for development/debugging**
- **TCP Sockets**: For distributed sandboxing across machines

### 2. Properties Map Configuration (Option A)
IPC transport is configured via a **properties map** inside `sandbox.config`:

```json
"sandbox": {
  "config": {
    "maxMemoryMB": 128,
    "properties": {
      "ipc_transport": "shared_memory",
      "ipc_buffer_size": "4096"
    }
  }
}
```

**Rationale:**
- ✅ Follows existing `ipc::TransportConfig` pattern (already uses properties map)
- ✅ Flexible - add transport-specific properties without changing C++ struct
- ✅ Minimal code changes - just add `std::map<std::string, std::string> properties;` to `SandboxConfig`
- ✅ Easy parsing - framework iterates over JSON properties and copies to map
- ✅ Type-agnostic - all values are strings, parsed as needed

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│               Framework Process (Parent)                │
│                                                         │
│  ┌──────────────┐          ┌─────────────────┐          │
│  │  Framework   │          │  SandboxManager │          │
│  │    Core      │◄────────►│                 │          │
│  └──────────────┘          └────────┬────────┘          │
│                                     │                   │
│                                     │ fork()            │
│                                     ▼                   │
│                          ┌──────────────────┐           │
│                          │   SandboxIPC     │           │
│                          │   (Strategy)     │           │
│                          └────────┬─────────┘           │
│                                   │                     │
│                          ┌────────▼─────────┐           │
│                          │   ITransport*    │           │
│                          │   (Pluggable)    │           │
│                          └────────┬─────────┘           │
└───────────────────────────────────┼─────────────────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
     ┌────────▼─────────┐  ┌────────▼────────┐  ┌─────────▼─────┐
     │ SharedMemory     │  │  UnixSocket     │  │  TCPSocket    │
     │   Transport      │  │   Transport     │  │   Transport   │
     └────────┬─────────┘  └────────┬────────┘  └─────────┬─────┘
              │                     │                     │
     Shared Memory Region    Unix Socket Pair     TCP Connection
     (mmap + RingBuffer)     (socketpair)         (localhost:port)
              │                     │                     │
┌─────────────┼─────────────────────┼─────────────────────┼────┐
│             ▼                     ▼                     ▼    │
│         Sandboxed Module Process (Child)                     │
│                                                              │
│  ┌─────────────┐    ┌──────────────┐   ┌──────────┐          │
│  │ SandboxIPC  │◄──►│ Module Loader│◄─►│  Module  │          │
│  │  (Consumer) │    │              │   │(Isolated)│          │
│  └─────────────┘    └──────────────┘   └──────────┘          │
│                                                              │
│  Resource Limits:                                            │
│  - Memory: 128 MB (RLIMIT_AS)                                │
│  - CPU: 25% (cgroups or RLIMIT_CPU)                          │
│  - File Descriptors: 64 (RLIMIT_NOFILE)                      │
│  - Dropped Privileges (setuid/setgid)                        │
└──────────────────────────────────────────────────────────────┘
```

## Existing Framework Components

The CDMF framework already provides:

### 1. Transport Abstraction (`ipc/transport.h`)
- **ITransport** interface - base class for all transports
- **TransportType** enum: UNIX_SOCKET, SHARED_MEMORY, TCP_SOCKET, UDP_SOCKET, GRPC
- **TransportConfig** - unified configuration for all transport types
- Message serialization/deserialization
- Callbacks for async message handling
- Statistics and error handling

### 2. Transport Implementations
- **SharedMemoryTransport** (`ipc/shared_memory_transport.h`)
  - POSIX shared memory (shm_open/mmap)
  - Lock-free SPSCRingBuffer for bidirectional communication
  - Semaphore-based blocking operations
  - Best for: High bandwidth (1-2 GB/s), low latency (1-5 μs)

- **UnixSocketTransport** (`ipc/unix_socket_transport.h`)
  - Unix domain sockets (socketpair for parent-child)
  - Stream or datagram modes
  - Best for: Development, debugging, low setup overhead

- **TCPSocketTransport** (`ipc/tcp_socket_transport.h`)
  - TCP/IP sockets
  - Best for: Distributed sandboxing across network

### 3. Lock-free Ring Buffers (`ipc/ring_buffer.h`)
- **SPSCRingBuffer**: Single Producer Single Consumer (used by SharedMemoryTransport)
- **MPMCRingBuffer**: Multi Producer Multi Consumer
- Cache-line aligned to prevent false sharing
- Atomic operations with proper memory ordering
- Zero-copy operations where possible

## Implementation Steps

### Step 1: Sandbox IPC Message Protocol

**File: `workspace/src/framework/include/security/sandbox_ipc.h`**

```cpp
#ifndef CDMF_SANDBOX_IPC_H
#define CDMF_SANDBOX_IPC_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include "ipc/transport.h"
#include "ipc/message.h"

namespace cdmf {
namespace security {

/**
 * @brief Message types for sandbox IPC
 */
enum class SandboxMessageType : uint32_t {
    // Lifecycle commands
    LOAD_MODULE = 1,        // Parent → Child: Load module shared library
    MODULE_LOADED = 2,      // Child → Parent: Module loaded successfully
    START_MODULE = 3,       // Parent → Child: Start module
    MODULE_STARTED = 4,     // Child → Parent: Module started
    STOP_MODULE = 5,        // Parent → Child: Stop module
    MODULE_STOPPED = 6,     // Child → Parent: Module stopped

    // Service invocation
    CALL_SERVICE = 10,      // Parent → Child: Call service method
    SERVICE_RESPONSE = 11,  // Child → Parent: Service method response

    // Monitoring
    HEARTBEAT = 20,         // Bidirectional: Keep-alive
    STATUS_QUERY = 21,      // Parent → Child: Request status
    STATUS_REPORT = 22,     // Child → Parent: Status report

    // Control
    SHUTDOWN = 30,          // Parent → Child: Shutdown process
    ERROR = 31,             // Child → Parent: Error occurred
};

/**
 * @brief Sandbox IPC message payload
 */
struct SandboxMessage {
    SandboxMessageType type;
    std::string moduleId;
    std::string payload;      // JSON-encoded data (module path, service call args, etc.)
    uint64_t requestId;       // For request-response correlation
    int32_t errorCode;        // 0 = success, non-zero = error

    // Serialize to IPC Message
    ipc::MessagePtr toIPCMessage() const;

    // Deserialize from IPC Message
    static SandboxMessage fromIPCMessage(const ipc::Message& msg);
};

/**
 * @brief High-level wrapper around ITransport for sandbox communication
 *
 * Uses Strategy pattern to support multiple transport types:
 * - SharedMemoryTransport: High bandwidth for same-device IPC
 * - UnixSocketTransport: Low overhead for development/debugging
 * - TCPSocketTransport: For distributed sandboxing
 *
 * Provides synchronous send/receive semantics on top of pluggable transport.
 * Optimized for parent-child process communication.
 */
class SandboxIPC {
public:
    /**
     * @brief Role in IPC communication
     */
    enum class Role {
        PARENT,  // Framework (creates/binds transport)
        CHILD    // Sandboxed module (connects to transport)
    };

    /**
     * @brief Constructor with transport injection (Strategy pattern)
     *
     * @param role PARENT or CHILD
     * @param sandboxId Unique sandbox identifier
     * @param transport Transport strategy to use (ownership transferred)
     */
    SandboxIPC(Role role, const std::string& sandboxId,
               std::unique_ptr<ipc::ITransport> transport);

    ~SandboxIPC();

    /**
     * @brief Initialize IPC channel with transport-specific config
     *
     * Parent creates/binds transport, child connects to existing.
     *
     * @param config Transport configuration (type-specific)
     * @return true if initialization successful
     */
    bool initialize(const ipc::TransportConfig& config);

    /**
     * @brief Send message (blocking with timeout)
     */
    bool sendMessage(const SandboxMessage& msg, int32_t timeoutMs = 5000);

    /**
     * @brief Receive message (blocking with timeout)
     */
    bool receiveMessage(SandboxMessage& msg, int32_t timeoutMs = 5000);

    /**
     * @brief Try receive message (non-blocking)
     */
    bool tryReceiveMessage(SandboxMessage& msg);

    /**
     * @brief Send request and wait for response
     */
    bool sendRequest(const SandboxMessage& request, SandboxMessage& response,
                    int32_t timeoutMs = 5000);

    /**
     * @brief Close IPC channel
     */
    void close();

    /**
     * @brief Check if IPC is connected
     */
    bool isConnected() const;

    /**
     * @brief Get transport endpoint information
     *
     * Returns transport-specific endpoint:
     * - Shared memory: "/dev/shm/cdmf_sandbox_<id>"
     * - Unix socket: "/tmp/cdmf_sandbox_<id>.sock"
     * - TCP: "localhost:9000"
     */
    std::string getEndpoint() const;

    /**
     * @brief Get statistics
     */
    struct Stats {
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t send_failures;
        uint64_t receive_failures;
        uint64_t bytes_sent;
        uint64_t bytes_received;
    };
    Stats getStats() const;

private:
    Role role_;
    std::string sandbox_id_;

    // Transport strategy (pluggable)
    std::unique_ptr<ipc::ITransport> transport_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;

    // Request-response tracking
    std::atomic<uint64_t> next_request_id_;

    void updateStats(bool is_send, size_t bytes, bool success);
};

/**
 * @brief Factory function to create transport for sandbox IPC
 *
 * @param type Transport type to create
 * @param sandboxId Sandbox identifier (used for endpoint naming)
 * @return Unique pointer to transport instance
 */
std::unique_ptr<ipc::ITransport> createSandboxTransport(
    ipc::TransportType type,
    const std::string& sandboxId);

/**
 * @brief Helper to create transport configuration for sandbox
 *
 * Creates transport configuration based on type and properties from manifest.
 * Reads properties like ipc_buffer_size, ipc_shm_size, ipc_endpoint, etc.
 *
 * @param type Transport type
 * @param sandboxId Sandbox identifier
 * @param role PARENT or CHILD
 * @param properties Properties map from SandboxConfig (from manifest.json)
 * @return Transport configuration
 */
ipc::TransportConfig createSandboxTransportConfig(
    ipc::TransportType type,
    const std::string& sandboxId,
    SandboxIPC::Role role,
    const std::map<std::string, std::string>& properties);

} // namespace security
} // namespace cdmf

#endif // CDMF_SANDBOX_IPC_H
```

### Step 2: Sandboxed Module Loader

**File: `workspace/src/framework/include/security/sandbox_module_loader.h`**

```cpp
#ifndef CDMF_SANDBOX_MODULE_LOADER_H
#define CDMF_SANDBOX_MODULE_LOADER_H

#include <string>
#include <memory>
#include "security/sandbox_ipc.h"
#include "module/module_activator.h"

namespace cdmf {
namespace security {

/**
 * @brief Runs in sandboxed child process - loads and manages module
 *
 * Main event loop that:
 * 1. Receives commands from parent via shared memory IPC
 * 2. Loads module shared library (dlopen)
 * 3. Manages module lifecycle (start/stop)
 * 4. Proxies service calls from parent
 */
class SandboxModuleLoader {
public:
    /**
     * @brief Main entry point for sandboxed process
     *
     * Called immediately after fork() in child process.
     * Blocks until SHUTDOWN message received.
     *
     * @param sandboxId Unique sandbox identifier (for shared memory name)
     * @return Exit code (0 = success)
     */
    static int runSandboxedProcess(const std::string& sandboxId);

private:
    struct Context {
        std::unique_ptr<SandboxIPC> ipc;
        void* module_handle;
        IModuleActivator* activator;
        std::string module_id;
        bool running;
    };

    static bool handleLoadModule(Context& ctx, const SandboxMessage& msg);
    static bool handleStartModule(Context& ctx, const SandboxMessage& msg);
    static bool handleStopModule(Context& ctx, const SandboxMessage& msg);
    static bool handleCallService(Context& ctx, const SandboxMessage& msg);
    static bool handleShutdown(Context& ctx, const SandboxMessage& msg);

    static void sendResponse(Context& ctx, SandboxMessageType type,
                            const std::string& payload, int32_t errorCode,
                            uint64_t requestId);
    static void sendHeartbeat(Context& ctx);
};

} // namespace security
} // namespace cdmf

#endif // CDMF_SANDBOX_MODULE_LOADER_H
```

### Step 3: Updated Process Sandbox Implementation

**File: `workspace/src/framework/impl/security/sandbox_manager.cpp`**

Update the `setupProcessSandbox` function to use pluggable transport strategy:

```cpp
bool SandboxManager::setupProcessSandbox(const std::string& sandboxId,
                                        const SandboxConfig& config) {
    LOGD_FMT("Setting up process sandbox: " << sandboxId
             << " with memory=" << config.maxMemoryMB << "MB");

#ifndef __linux__
    LOGW("Process sandboxing only supported on Linux");
    return false;
#else

    // 1. Determine transport type from config properties (default: shared_memory)
    ipc::TransportType transportType = ipc::TransportType::SHARED_MEMORY;

    auto it = config.properties.find("ipc_transport");
    if (it != config.properties.end()) {
        const std::string& transportStr = it->second;
        if (transportStr == "unix_socket") {
            transportType = ipc::TransportType::UNIX_SOCKET;
        } else if (transportStr == "tcp") {
            transportType = ipc::TransportType::TCP_SOCKET;
        } else if (transportStr == "shared_memory") {
            transportType = ipc::TransportType::SHARED_MEMORY;
        } else {
            LOGW_FMT("Unknown transport type: " << transportStr << ", using shared_memory");
        }
    }

    LOGI_FMT("Using IPC transport: " << transportTypeToString(transportType));

    // 2. Create transport configuration from properties
    ipc::TransportConfig transportConfig = createSandboxTransportConfig(
        transportType, sandboxId, SandboxIPC::Role::PARENT, config.properties);

    // 3. Create transport strategy
    auto transport = createSandboxTransport(transportType, sandboxId);

    // 4. Create SandboxIPC with injected transport strategy
    auto ipc = std::make_unique<SandboxIPC>(SandboxIPC::Role::PARENT, sandboxId,
                                            std::move(transport));

    // Initialize transport before fork (so it exists for child to connect)
    if (!ipc->initialize(transportConfig)) {
        LOGE("Failed to initialize IPC channel");
        return false;
    }

    // 4. Fork process
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        LOGE_FMT("Fork failed: " << strerror(errno));
        return false;
    }

    if (pid == 0) {
        // ===== CHILD PROCESS =====

        // Set resource limits BEFORE loading module
        if (!setResourceLimits(config)) {
            LOGE("Failed to set resource limits in child process");
            exit(1);
        }

        // Drop privileges (run as unprivileged user)
        if (!dropPrivileges()) {
            LOGE("Failed to drop privileges");
            exit(1);
        }

        // Run sandboxed module loader
        // This blocks until shutdown
        int exitCode = SandboxModuleLoader::runSandboxedProcess(sandboxId);

        exit(exitCode);
    }

    // ===== PARENT PROCESS =====

    // Store sandbox info with IPC channel and transport type
    auto info = pImpl_->sandboxes[sandboxId];
    info->processId = pid;
    info->ipc = std::move(ipc);  // Transfer ownership to sandbox info
    info->transportType = transportType;  // Store for reference

    LOGI_FMT("Process sandbox created: PID=" << pid << ", sandbox=" << sandboxId
             << ", transport=" << transportTypeToString(transportType)
             << ", endpoint=" << info->ipc->getEndpoint());
    return true;

#endif // __linux__
}

bool SandboxManager::setResourceLimits(const SandboxConfig& config) {
#ifdef __linux__
    struct rlimit limit;

    // Memory limit (virtual address space)
    limit.rlim_cur = config.maxMemoryMB * 1024ULL * 1024ULL;
    limit.rlim_max = config.maxMemoryMB * 1024ULL * 1024ULL;
    if (setrlimit(RLIMIT_AS, &limit) != 0) {
        LOGE_FMT("Failed to set RLIMIT_AS: " << strerror(errno));
        return false;
    }

    // File descriptor limit
    limit.rlim_cur = config.maxFileDescriptors;
    limit.rlim_max = config.maxFileDescriptors;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
        LOGE_FMT("Failed to set RLIMIT_NOFILE: " << strerror(errno));
        return false;
    }

    // CPU time limit (soft limit in seconds)
    // Note: This is per-second CPU time, not real-time
    limit.rlim_cur = config.maxCPUPercent;
    limit.rlim_max = config.maxCPUPercent;
    if (setrlimit(RLIMIT_CPU, &limit) != 0) {
        LOGE_FMT("Failed to set RLIMIT_CPU: " << strerror(errno));
        return false;
    }

    LOGI("Resource limits set successfully");
    return true;
#else
    return false;
#endif
}

bool SandboxManager::dropPrivileges() {
#ifdef __linux__
    // Drop to nobody user (UID 65534) if running as root
    if (getuid() == 0) {
        if (setgid(65534) != 0 || setuid(65534) != 0) {
            LOGE_FMT("Failed to drop privileges: " << strerror(errno));
            return false;
        }
        LOGI("Dropped privileges to nobody user");
    }
    return true;
#else
    return false;
#endif
}
```

### Step 4: Module Loading via Shared Memory IPC

**File: `workspace/src/framework/impl/security/sandbox_module_loader.cpp`**

```cpp
#include "security/sandbox_module_loader.h"
#include "security/sandbox_ipc.h"
#include "log/logger.h"
#include <dlfcn.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace cdmf {
namespace security {

int SandboxModuleLoader::runSandboxedProcess(const std::string& sandboxId) {
    LOGI_FMT("Sandboxed process started, PID=" << getpid()
             << ", sandbox=" << sandboxId);

    Context ctx;
    ctx.module_handle = nullptr;
    ctx.activator = nullptr;
    ctx.running = true;

    // Initialize IPC as child (connects to existing transport)
    // Read manifest.json to determine transport type (same as parent)
    // Note: sandboxId is passed as parameter to this function

    // TODO: Read module manifest.json to get sandbox.config.properties
    // For now, assume default shared_memory transport
    ipc::TransportType transportType = ipc::TransportType::SHARED_MEMORY;
    std::map<std::string, std::string> properties;

    // Create transport configuration (child role)
    ipc::TransportConfig transportConfig = createSandboxTransportConfig(
        transportType, sandboxId, SandboxIPC::Role::CHILD, properties);

    // Create transport strategy
    auto transport = createSandboxTransport(transportType, sandboxId);

    // Create SandboxIPC
    ctx.ipc = std::make_unique<SandboxIPC>(SandboxIPC::Role::CHILD, sandboxId,
                                           std::move(transport));

    if (!ctx.ipc->initialize(transportConfig)) {
        LOGE("Failed to connect to IPC channel");
        return 1;
    }

    LOGI_FMT("IPC connected: transport=" << transportTypeToString(transportType)
             << ", endpoint=" << ctx.ipc->getEndpoint());

    // Main event loop
    int heartbeat_counter = 0;
    while (ctx.running) {
        SandboxMessage msg;

        // Try to receive message with timeout
        if (!ctx.ipc->receiveMessage(msg, 1000)) {
            // Timeout - send heartbeat every 5 seconds
            if (++heartbeat_counter >= 5) {
                sendHeartbeat(ctx);
                heartbeat_counter = 0;
            }
            continue;
        }

        // Reset heartbeat counter on message
        heartbeat_counter = 0;

        // Dispatch message
        bool handled = false;
        switch (msg.type) {
            case SandboxMessageType::LOAD_MODULE:
                handled = handleLoadModule(ctx, msg);
                break;

            case SandboxMessageType::START_MODULE:
                handled = handleStartModule(ctx, msg);
                break;

            case SandboxMessageType::STOP_MODULE:
                handled = handleStopModule(ctx, msg);
                break;

            case SandboxMessageType::CALL_SERVICE:
                handled = handleCallService(ctx, msg);
                break;

            case SandboxMessageType::SHUTDOWN:
                handled = handleShutdown(ctx, msg);
                ctx.running = false;
                break;

            default:
                LOGW_FMT("Unknown message type: " << (int)msg.type);
                break;
        }

        if (!handled) {
            LOGE_FMT("Failed to handle message type: " << (int)msg.type);
        }
    }

    // Cleanup
    if (ctx.activator) {
        // ctx.activator->stop(contextStub);
        delete ctx.activator;
    }
    if (ctx.module_handle) {
        dlclose(ctx.module_handle);
    }
    ctx.ipc->close();

    LOGI("Sandboxed process exiting");
    return 0;
}

bool SandboxModuleLoader::handleLoadModule(Context& ctx, const SandboxMessage& msg) {
    LOGI_FMT("Loading module: " << msg.payload);

    // Payload is module library path
    std::string libPath = msg.payload;

    // Load module shared library
    ctx.module_handle = dlopen(libPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!ctx.module_handle) {
        std::string error = dlerror();
        LOGE_FMT("Failed to load module: " << error);
        sendResponse(ctx, SandboxMessageType::ERROR, error, 1, msg.requestId);
        return false;
    }

    // Get activator factory function
    using CreateFunc = IModuleActivator*(*)();
    auto createFunc = (CreateFunc)dlsym(ctx.module_handle, "createModuleActivator");
    if (!createFunc) {
        std::string error = "Module factory function 'createModuleActivator' not found";
        LOGE(error);
        sendResponse(ctx, SandboxMessageType::ERROR, error, 2, msg.requestId);
        dlclose(ctx.module_handle);
        ctx.module_handle = nullptr;
        return false;
    }

    // Create activator instance
    ctx.activator = createFunc();
    ctx.module_id = msg.moduleId;

    LOGI_FMT("Module loaded successfully: " << ctx.module_id);
    sendResponse(ctx, SandboxMessageType::MODULE_LOADED, "", 0, msg.requestId);
    return true;
}

bool SandboxModuleLoader::handleStartModule(Context& ctx, const SandboxMessage& msg) {
    if (!ctx.activator) {
        sendResponse(ctx, SandboxMessageType::ERROR, "Module not loaded", 3, msg.requestId);
        return false;
    }

    LOGI_FMT("Starting module: " << ctx.module_id);

    // TODO: Create module context stub (IPC proxy to parent)
    // ctx.activator->start(contextStub);

    LOGI_FMT("Module started: " << ctx.module_id);
    sendResponse(ctx, SandboxMessageType::MODULE_STARTED, "", 0, msg.requestId);
    return true;
}

bool SandboxModuleLoader::handleStopModule(Context& ctx, const SandboxMessage& msg) {
    if (!ctx.activator) {
        sendResponse(ctx, SandboxMessageType::ERROR, "Module not loaded", 4, msg.requestId);
        return false;
    }

    LOGI_FMT("Stopping module: " << ctx.module_id);

    // TODO: Stop module
    // ctx.activator->stop(contextStub);

    LOGI_FMT("Module stopped: " << ctx.module_id);
    sendResponse(ctx, SandboxMessageType::MODULE_STOPPED, "", 0, msg.requestId);
    return true;
}

bool SandboxModuleLoader::handleCallService(Context& ctx, const SandboxMessage& msg) {
    // TODO: Implement service call proxying
    LOGW("SERVICE_CALL not yet implemented");
    sendResponse(ctx, SandboxMessageType::ERROR, "Not implemented", 99, msg.requestId);
    return false;
}

bool SandboxModuleLoader::handleShutdown(Context& ctx, const SandboxMessage& msg) {
    LOGI("Received SHUTDOWN command");
    sendResponse(ctx, SandboxMessageType::MODULE_STOPPED, "", 0, msg.requestId);
    return true;
}

void SandboxModuleLoader::sendResponse(Context& ctx, SandboxMessageType type,
                                       const std::string& payload, int32_t errorCode,
                                       uint64_t requestId) {
    SandboxMessage response;
    response.type = type;
    response.moduleId = ctx.module_id;
    response.payload = payload;
    response.requestId = requestId;
    response.errorCode = errorCode;

    if (!ctx.ipc->sendMessage(response, 5000)) {
        LOGE("Failed to send response to parent");
    }
}

void SandboxModuleLoader::sendHeartbeat(Context& ctx) {
    SandboxMessage heartbeat;
    heartbeat.type = SandboxMessageType::HEARTBEAT;
    heartbeat.moduleId = ctx.module_id;
    heartbeat.requestId = 0;
    heartbeat.errorCode = 0;

    ctx.ipc->sendMessage(heartbeat, 1000);
}

} // namespace security
} // namespace cdmf
```

### Step 5: Helper Functions Implementation

**File: `workspace/src/framework/impl/security/sandbox_ipc.cpp`**

```cpp
std::unique_ptr<ipc::ITransport> createSandboxTransport(
    ipc::TransportType type,
    const std::string& sandboxId) {

    switch (type) {
        case ipc::TransportType::SHARED_MEMORY:
            return std::make_unique<ipc::SharedMemoryTransport>();

        case ipc::TransportType::UNIX_SOCKET:
            return std::make_unique<ipc::UnixSocketTransport>();

        case ipc::TransportType::TCP_SOCKET:
            return std::make_unique<ipc::TCPSocketTransport>();

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

    // Parse common properties
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
            config.endpoint = "/dev/shm/cdmf_sandbox_" + sandboxId;

            // Shared memory specific properties
            config.properties["shm_size"] = getProperty("ipc_shm_size", "4194304"); // 4 MB default
            config.properties["ring_capacity"] = getProperty("ipc_buffer_size", "4096");
            config.properties["create_shm"] = (role == SandboxIPC::Role::PARENT) ? "true" : "false";
            break;
        }

        case ipc::TransportType::UNIX_SOCKET: {
            // Endpoint is socket path
            config.endpoint = "/tmp/cdmf_sandbox_" + sandboxId + ".sock";
            // For parent-child, use socketpair instead of path
            config.properties["use_socketpair"] = "true";
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

std::string transportTypeToString(ipc::TransportType type) {
    switch (type) {
        case ipc::TransportType::SHARED_MEMORY: return "SHARED_MEMORY";
        case ipc::TransportType::UNIX_SOCKET: return "UNIX_SOCKET";
        case ipc::TransportType::TCP_SOCKET: return "TCP_SOCKET";
        default: return "UNKNOWN";
    }
}
```

### Step 6: Framework Integration

Update framework to send load/start commands via IPC:

```cpp
// In framework.cpp, after sandbox creation:
void Framework::installModuleInSandbox(Module* module, const std::string& sandboxId) {
    auto& sandboxMgr = SandboxManager::getInstance();
    auto sandboxInfo = sandboxMgr.getSandboxInfo(sandboxId);

    if (!sandboxInfo || !sandboxInfo->ipc) {
        throw std::runtime_error("Sandbox not found or not initialized");
    }

    // Send LOAD_MODULE command to sandboxed process
    SandboxMessage loadMsg;
    loadMsg.type = SandboxMessageType::LOAD_MODULE;
    loadMsg.moduleId = module->getSymbolicName();
    loadMsg.payload = module->getLocation(); // Library path
    loadMsg.requestId = 1;

    SandboxMessage response;
    if (!sandboxInfo->ipc->sendRequest(loadMsg, response, 10000)) {
        throw std::runtime_error("Failed to send LOAD_MODULE command");
    }

    if (response.type == SandboxMessageType::MODULE_LOADED) {
        LOGI_FMT("Module loaded in sandbox: " << module->getSymbolicName());
    } else {
        throw std::runtime_error("Module load failed: " + response.payload);
    }

    // Send START_MODULE command
    SandboxMessage startMsg;
    startMsg.type = SandboxMessageType::START_MODULE;
    startMsg.moduleId = module->getSymbolicName();
    startMsg.requestId = 2;

    if (!sandboxInfo->ipc->sendRequest(startMsg, response, 10000)) {
        throw std::runtime_error("Failed to send START_MODULE command");
    }

    if (response.type == SandboxMessageType::MODULE_STARTED) {
        LOGI_FMT("Module started in sandbox: " << module->getSymbolicName());
    } else {
        throw std::runtime_error("Module start failed: " + response.payload);
    }
}
```

## Performance Characteristics

### Transport Comparison

| Metric | Shared Memory | Unix Sockets | TCP Sockets |
|--------|---------------|--------------|-------------|
| **Bandwidth** | 1-2 GB/s | 200-400 MB/s | 100-200 MB/s (localhost) |
| **Latency** | 1-5 μs | 50-100 μs | 100-500 μs |
| **Zero-copy** | Yes (mmap) | No (kernel buffers) | No (kernel + TCP stack) |
| **Setup overhead** | Moderate (shm_open) | Low (socketpair) | Moderate (bind/connect) |
| **Process isolation** | Same machine | Same machine | Cross-machine capable |
| **Best for** | High throughput | Development, debugging | Distributed sandboxing |

**Recommendation by use case**:
- **Production (same-machine)**: SharedMemoryTransport - **5-10x better bandwidth**, **10-50x lower latency**
- **Development/Debugging**: UnixSocketTransport - Easy to monitor with tools like `socat`, `strace`
- **Distributed sandboxing**: TCPSocketTransport - Sandbox on remote machine for security isolation

### Transport Selection Example

**In manifest.json, specify transport type via properties map inside config:**

```json
"sandbox": {
  "enabled": true,
  "type": "PROCESS",
  "config": {
    "allowNetworkAccess": false,
    "allowFileSystemAccess": true,
    "allowedPaths": ["/tmp/hello_service_greetings.log"],
    "maxMemoryMB": 128,
    "maxCPUPercent": 25,
    "maxFileDescriptors": 64,
    "maxThreads": 5,
    "properties": {
      "ipc_transport": "shared_memory",
      "ipc_buffer_size": "4096",
      "ipc_shm_size": "4194304"
    }
  }
}
```

**Supported property keys:**
- `"ipc_transport"`: Transport type - `"shared_memory"` (default), `"unix_socket"`, `"tcp"`
- `"ipc_buffer_size"`: Ring buffer capacity (number of messages) - default `"4096"`
- `"ipc_shm_size"`: Shared memory size in bytes (for shared_memory transport) - default `"4194304"` (4 MB)
- `"ipc_endpoint"`: Custom endpoint (for TCP transport) - default auto-generated
- `"ipc_timeout_ms"`: Send/receive timeout in milliseconds - default `"5000"`

**Examples for different transports:**

```json
// Shared Memory (high bandwidth)
"properties": {
  "ipc_transport": "shared_memory",
  "ipc_shm_size": "8388608"  // 8 MB for high-throughput modules
}

// Unix Socket (development/debugging)
"properties": {
  "ipc_transport": "unix_socket",
  "ipc_buffer_size": "1024"
}

// TCP Socket (distributed sandboxing)
"properties": {
  "ipc_transport": "tcp",
  "ipc_endpoint": "localhost:9000"
}
```

### Ring Buffer Performance

- **SPSCRingBuffer**: Optimized for single producer/consumer (parent↔child)
  - Lock-free with relaxed memory ordering
  - ~10-20 ns per operation
  - Zero contention with proper usage

- **Cache-line alignment**: Prevents false sharing between producer/consumer
- **Power-of-2 capacity**: Enables fast modulo via bitmask `(pos & (Capacity - 1))`

## Complete Example: Hello Service with Shared Memory IPC

**manifest.json:**
```json
{
  "module": {
    "symbolic-name": "cdmf.hello_service",
    "name": "Hello Service Module",
    "version": "1.0.0"
  },
  "security": {
    "permissions": [...],
    "sandbox": {
      "enabled": true,
      "type": "PROCESS",
      "config": {
        "allowNetworkAccess": false,
        "allowFileSystemAccess": true,
        "allowedPaths": ["/tmp/hello_service_greetings.log"],
        "maxMemoryMB": 128,
        "maxCPUPercent": 25,
        "maxFileDescriptors": 64,
        "maxThreads": 5,
        "properties": {
          "ipc_transport": "shared_memory",
          "ipc_shm_size": "4194304",
          "ipc_buffer_size": "4096"
        }
      }
    }
  }
}
```

**Execution flow:**
1. Framework reads manifest, parses `sandbox.config.properties`
2. Creates `SandboxConfig` with `properties["ipc_transport"] = "shared_memory"`
3. Calls `SandboxManager::createSandbox(moduleId, config)`
4. `setupProcessSandbox()` reads `config.properties["ipc_transport"]`
5. Creates `SharedMemoryTransport` instance
6. Calls `fork()` - parent and child both have access to shared memory
7. Parent waits for child to connect
8. Child connects to existing shared memory
9. Parent sends `LOAD_MODULE` message with library path
10. Child loads module via `dlopen()`, sends `MODULE_LOADED` response
11. Communication continues via high-bandwidth shared memory

## Testing

### Test 1: Verify Process Isolation

```bash
# Run framework
./bin/cdmf

# In another terminal, check processes
ps aux | grep cdmf

# You should see:
# - Parent: cdmf framework process
# - Child: cdmf module process (for each sandboxed module)

# Check shared memory
ls -lh /dev/shm/cdmf_sandbox_*
```

### Test 2: Verify Resource Limits

```bash
# Get child PID from log or ps
CHILD_PID=$(pgrep -f "cdmf.*sandbox")

# Check memory limit
cat /proc/$CHILD_PID/limits | grep "Max address space"
# Should show: 134217728 (128 MB in bytes)

# Check file descriptor limit
cat /proc/$CHILD_PID/limits | grep "Max open files"
# Should show: 64

# Monitor shared memory usage
cat /proc/$CHILD_PID/maps | grep "/dev/shm"
```

### Test 3: Verify Shared Memory IPC

Add debug logs to track IPC messages:

**Parent process**:
```
[INFO] Creating shared memory: /dev/shm/cdmf_sandbox_hello_service
[INFO] Sending LOAD_MODULE: /path/to/hello_service_module.so
[INFO] Received MODULE_LOADED from child (PID: 12345)
[INFO] Sending START_MODULE
[INFO] Received MODULE_STARTED from child
```

**Child process**:
```
[INFO] Sandboxed process started, PID=12345
[INFO] Shared memory IPC connected: /dev/shm/cdmf_sandbox_hello_service
[INFO] Loading module: /path/to/hello_service_module.so
[INFO] Module loaded successfully: cdmf.hello_service
[INFO] Starting module: cdmf.hello_service
[INFO] Module started: cdmf.hello_service
```

### Test 4: Measure IPC Bandwidth

```cpp
// Benchmark in parent process
auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 100000; i++) {
    SandboxMessage msg;
    msg.type = SandboxMessageType::HEARTBEAT;
    msg.payload = std::string(1024, 'X'); // 1 KB payload
    ipc->sendMessage(msg);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

double bandwidth_mb_s = (100000.0 * 1024.0) / (duration.count() / 1e6) / (1024.0 * 1024.0);
std::cout << "Bandwidth: " << bandwidth_mb_s << " MB/s" << std::endl;
```

Expected: **800-1500 MB/s** for shared memory with SPSCRingBuffer.

## Security Considerations

1. **Privilege Separation**: Child runs as `nobody` user (UID 65534)
2. **Resource Isolation**: Memory/CPU/FD limits enforced via setrlimit()
3. **Shared Memory Permissions**: Only parent and child can access (proper shm permissions)
4. **Network Isolation**: Can be enhanced with network namespaces
5. **File Access**: Restrict using seccomp or AppArmor (future enhancement)

## Memory Layout

```
Shared Memory Segment (/dev/shm/cdmf_sandbox_<id>)
┌────────────────────────────────────────────────┐
│  ShmControlBlock (64 bytes, cache-line aligned)│
│  - magic: 0xCDAF5000                           │
│  - version, size, flags                        │
│  - reader_count, writer_count                  │
│  - server_pid, client_pid                      │
├────────────────────────────────────────────────┤
│  TX Ring Buffer (Parent → Child)               │
│  - SPSCRingBuffer<Message, 4096>               │
│  - write_pos (atomic, cache-aligned)           │
│  - read_pos (atomic, cache-aligned)            │
│  - buffer[4096] (message slots)                │
├────────────────────────────────────────────────┤
│  RX Ring Buffer (Child → Parent)               │
│  - SPSCRingBuffer<Message, 4096>               │
│  - write_pos (atomic, cache-aligned)           │
│  - read_pos (atomic, cache-aligned)            │
│  - buffer[4096] (message slots)                │
└────────────────────────────────────────────────┘
Total: ~4 MB (configurable)
```

## Future Enhancements

1. **Dynamic Ring Buffer Sizing**: Adjust capacity based on load
2. **Multiple Ring Buffers**: Separate channels for control vs data messages
3. **cgroups v2**: Better CPU/memory control than setrlimit
4. **Network Namespaces**: Complete network isolation
5. **Seccomp-BPF**: Syscall filtering for enhanced security
6. **Process Pooling**: Reuse processes for multiple modules
7. **Service Call Proxying**: Full RPC support over shared memory
8. **Crash Recovery**: Parent detects child crashes and restarts sandboxes

## References

- Linux `fork()` man page
- `setrlimit()` resource limits
- POSIX shared memory (`shm_open`, `mmap`)
- Lock-free algorithms (Lamport's SPSC queue)
- Memory barriers and atomic operations
- Framework IPC integration quick start: `workspace/docs/quick_start/ipc_integration_quick_start.md`
