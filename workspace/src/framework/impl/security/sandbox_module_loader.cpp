/**
 * @file sandbox_module_loader.cpp
 * @brief Sandboxed module loader implementation
 *
 * @version 1.0.0
 * @date 2025-10-06
 */

#include "security/sandbox_module_loader.h"
#include "security/sandbox_ipc.h"
#include "utils/log.h"
#include <nlohmann/json.hpp>

#ifdef __linux__
#include <dlfcn.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

using json = nlohmann::json;

namespace cdmf {
namespace security {

int SandboxModuleLoader::runSandboxedProcess(const std::string& sandboxId,
                                             std::unique_ptr<SandboxIPC> ipc) {
#ifdef __linux__
    pid_t pid = getpid();
#elif defined(_WIN32)
    DWORD pid = GetCurrentProcessId();
#else
    int pid = 0;
#endif

    LOGI_FMT("Sandboxed process started, PID=" << pid << ", sandbox=" << sandboxId);

    if (!ipc || !ipc->isConnected()) {
        LOGE("IPC channel not connected");
        return 1;
    }

    LOGI_FMT("IPC already connected: endpoint=" << ipc->getEndpoint());

    Context ctx;
    ctx.module_handle = nullptr;
    ctx.activator = nullptr;
    ctx.running = true;
    ctx.ipc = std::move(ipc);  // Take ownership of pre-connected IPC

    // Note: Initial heartbeat already sent by parent process before starting this loader
    LOGI_FMT("Module loader starting event loop...");

    // Main event loop
    int heartbeat_counter = 0;
    while (ctx.running) {
        LOGD_FMT("Child event loop iteration, heartbeat_counter=" << heartbeat_counter);
        SandboxMessage msg;

        // Try to receive message with timeout
        LOGD("Child calling receiveMessage...");
        if (!ctx.ipc->receiveMessage(msg, 1000)) {
            LOGD("Child receiveMessage returned false (timeout or error)");
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
                LOGW_FMT("Unknown message type: " << static_cast<int>(msg.type));
                break;
        }

        if (!handled) {
            LOGE_FMT("Failed to handle message type: " << static_cast<int>(msg.type));
        }
    }

    // Cleanup
    if (ctx.activator) {
        // ctx.activator->stop(contextStub);
        delete ctx.activator;
    }
    if (ctx.module_handle) {
#ifdef __linux__
        dlclose(ctx.module_handle);
#elif defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(ctx.module_handle));
#endif
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
#ifdef __linux__
    ctx.module_handle = dlopen(libPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!ctx.module_handle) {
        std::string error = dlerror();
        LOGE_FMT("Failed to load module: " << error);
        sendResponse(ctx, SandboxMessageType::ERROR, error, 1, msg.requestId);
        return false;
    }

    // Get activator factory function
    using CreateFunc = IModuleActivator*(*)();
    auto createFunc = reinterpret_cast<CreateFunc>(dlsym(ctx.module_handle, "createModuleActivator"));
    if (!createFunc) {
        std::string error = "Module factory function 'createModuleActivator' not found";
        LOGE(error);
        sendResponse(ctx, SandboxMessageType::ERROR, error, 2, msg.requestId);
        dlclose(ctx.module_handle);
        ctx.module_handle = nullptr;
        return false;
    }
#elif defined(_WIN32)
    ctx.module_handle = LoadLibraryA(libPath.c_str());
    if (!ctx.module_handle) {
        DWORD errorCode = GetLastError();
        std::string error = "Failed to load module: error code " + std::to_string(errorCode);
        LOGE_FMT(error);
        sendResponse(ctx, SandboxMessageType::ERROR, error, 1, msg.requestId);
        return false;
    }

    using CreateFunc = IModuleActivator*(*)();
    auto createFunc = reinterpret_cast<CreateFunc>(GetProcAddress(static_cast<HMODULE>(ctx.module_handle), "createModuleActivator"));
    if (!createFunc) {
        std::string error = "Module factory function 'createModuleActivator' not found";
        LOGE(error);
        sendResponse(ctx, SandboxMessageType::ERROR, error, 2, msg.requestId);
        FreeLibrary(static_cast<HMODULE>(ctx.module_handle));
        ctx.module_handle = nullptr;
        return false;
    }
#else
    std::string error = "Dynamic loading not supported on this platform";
    LOGE(error);
    sendResponse(ctx, SandboxMessageType::ERROR, error, 3, msg.requestId);
    return false;
#endif

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
