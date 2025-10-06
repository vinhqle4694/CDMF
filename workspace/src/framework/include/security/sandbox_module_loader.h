/**
 * @file sandbox_module_loader.h
 * @brief Sandboxed module loader interface
 *
 * Runs in sandboxed child process to load and manage module lifecycle
 * via IPC commands from parent framework process.
 *
 * @version 1.0.0
 * @date 2025-10-06
 */

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
 * 1. Receives commands from parent via IPC (shared memory/Unix socket/TCP)
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
     * @param sandboxId Unique sandbox identifier
     * @param ipc Already-connected IPC channel (connected as root before privilege drop)
     * @return Exit code (0 = success)
     */
    static int runSandboxedProcess(const std::string& sandboxId,
                                   std::unique_ptr<SandboxIPC> ipc);

private:
    /**
     * @brief Context for sandboxed module loader
     */
    struct Context {
        std::unique_ptr<SandboxIPC> ipc;     ///< IPC channel to parent
        void* module_handle;                  ///< Module shared library handle
        IModuleActivator* activator;          ///< Module activator instance
        std::string module_id;                ///< Module symbolic name
        bool running;                         ///< Main event loop flag
    };

    /**
     * @brief Handle LOAD_MODULE command
     * @param ctx Loader context
     * @param msg IPC message
     * @return true if handled successfully
     */
    static bool handleLoadModule(Context& ctx, const SandboxMessage& msg);

    /**
     * @brief Handle START_MODULE command
     * @param ctx Loader context
     * @param msg IPC message
     * @return true if handled successfully
     */
    static bool handleStartModule(Context& ctx, const SandboxMessage& msg);

    /**
     * @brief Handle STOP_MODULE command
     * @param ctx Loader context
     * @param msg IPC message
     * @return true if handled successfully
     */
    static bool handleStopModule(Context& ctx, const SandboxMessage& msg);

    /**
     * @brief Handle CALL_SERVICE command
     * @param ctx Loader context
     * @param msg IPC message
     * @return true if handled successfully
     */
    static bool handleCallService(Context& ctx, const SandboxMessage& msg);

    /**
     * @brief Handle SHUTDOWN command
     * @param ctx Loader context
     * @param msg IPC message
     * @return true if handled successfully
     */
    static bool handleShutdown(Context& ctx, const SandboxMessage& msg);

    /**
     * @brief Send response to parent
     * @param ctx Loader context
     * @param type Message type
     * @param payload Payload data
     * @param errorCode Error code (0 = success)
     * @param requestId Request ID for correlation
     */
    static void sendResponse(Context& ctx, SandboxMessageType type,
                            const std::string& payload, int32_t errorCode,
                            uint64_t requestId);

    /**
     * @brief Send heartbeat to parent
     * @param ctx Loader context
     */
    static void sendHeartbeat(Context& ctx);
};

} // namespace security
} // namespace cdmf

#endif // CDMF_SANDBOX_MODULE_LOADER_H
