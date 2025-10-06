#ifndef CDMF_SANDBOX_MANAGER_H
#define CDMF_SANDBOX_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include "ipc/transport.h"

// Platform-specific includes
#ifdef __linux__
#include <sys/types.h>
#include <unistd.h>
#endif

namespace cdmf {

namespace module {
    class Module;
}

namespace security {

/**
 * @enum SandboxType
 * @brief Types of sandboxing mechanisms
 */
enum class SandboxType {
    NONE,                ///< No sandboxing
    PROCESS,             ///< Process-level isolation (separate process)
    NAMESPACE,           ///< Linux namespace isolation
    CONTAINER,           ///< Container-based isolation
    SECCOMP,             ///< Seccomp-BPF filtering
    APPARMOR,            ///< AppArmor profile
    SELINUX              ///< SELinux context
};

/**
 * @enum SandboxStatus
 * @brief Status of a sandboxed module
 */
enum class SandboxStatus {
    CREATED,             ///< Sandbox created but not started
    ACTIVE,              ///< Sandbox is active and running
    SUSPENDED,           ///< Sandbox is temporarily suspended
    TERMINATED,          ///< Sandbox has been terminated
    ERROR                ///< Sandbox is in error state
};

/**
 * @struct SandboxConfig
 * @brief Configuration for a sandbox
 */
struct SandboxConfig {
    SandboxType type = SandboxType::PROCESS;
    bool allowNetworkAccess = false;
    bool allowFileSystemAccess = false;
    std::vector<std::string> allowedPaths;
    std::vector<std::string> allowedSyscalls;
    std::vector<std::string> allowedIPs;
    uint32_t maxMemoryMB = 256;
    uint32_t maxCPUPercent = 50;
    uint32_t maxFileDescriptors = 256;
    uint32_t maxThreads = 10;
    std::string apparmorProfile;
    std::string selinuxContext;

    /**
     * @brief Additional properties for configuring sandbox behavior
     *
     * Common properties:
     * - ipc_transport: Transport type ("shared_memory", "unix_socket", "tcp")
     * - ipc_buffer_size: Ring buffer capacity (default: "4096")
     * - ipc_shm_size: Shared memory size in bytes (default: "4194304" = 4MB)
     * - ipc_endpoint: Custom endpoint (for TCP transport)
     * - ipc_timeout_ms: Send/receive timeout in milliseconds (default: "5000")
     */
    std::map<std::string, std::string> properties;
};

// Forward declaration
class SandboxIPC;

/**
 * @struct SandboxInfo
 * @brief Information about a sandbox
 */
struct SandboxInfo {
    std::string sandboxId;
    std::string moduleId;
    SandboxType type;
    SandboxStatus status;
    SandboxConfig config;
    uint64_t createdTime;
    uint64_t terminatedTime;
    std::string errorMessage;

    // Process sandbox specific fields
    pid_t processId = -1;                              ///< Child process ID (for PROCESS sandbox)
    std::unique_ptr<SandboxIPC> ipc;                   ///< IPC channel to sandboxed process
    ipc::TransportType transportType;                   ///< IPC transport type used
};

/**
 * @class SandboxManager
 * @brief Manages sandboxed execution of modules
 *
 * The SandboxManager provides isolation mechanisms to run untrusted modules
 * in restricted environments, limiting their access to system resources.
 * Supports multiple sandboxing technologies:
 * - Process isolation
 * - Linux namespaces (PID, NET, MNT, IPC, UTS)
 * - Seccomp-BPF syscall filtering
 * - AppArmor/SELinux mandatory access control
 */
class SandboxManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the SandboxManager instance
     */
    static SandboxManager& getInstance();

    /**
     * @brief Delete copy constructor
     */
    SandboxManager(const SandboxManager&) = delete;

    /**
     * @brief Delete assignment operator
     */
    SandboxManager& operator=(const SandboxManager&) = delete;

    /**
     * @brief Create a sandbox for a module
     * @param moduleId The module ID
     * @param config Sandbox configuration
     * @return Sandbox ID if successful, empty string otherwise
     */
    std::string createSandbox(const std::string& moduleId, const SandboxConfig& config);

    /**
     * @brief Destroy a sandbox
     * @param sandboxId The sandbox ID
     * @return true if destroyed successfully
     */
    bool destroySandbox(const std::string& sandboxId);

    /**
     * @brief Start a sandbox
     * @param sandboxId The sandbox ID
     * @return true if started successfully
     */
    bool startSandbox(const std::string& sandboxId);

    /**
     * @brief Stop a sandbox
     * @param sandboxId The sandbox ID
     * @return true if stopped successfully
     */
    bool stopSandbox(const std::string& sandboxId);

    /**
     * @brief Suspend a sandbox
     * @param sandboxId The sandbox ID
     * @return true if suspended successfully
     */
    bool suspendSandbox(const std::string& sandboxId);

    /**
     * @brief Resume a suspended sandbox
     * @param sandboxId The sandbox ID
     * @return true if resumed successfully
     */
    bool resumeSandbox(const std::string& sandboxId);

    /**
     * @brief Get sandbox information
     * @param sandboxId The sandbox ID
     * @return SandboxInfo or null if not found
     */
    std::shared_ptr<SandboxInfo> getSandboxInfo(const std::string& sandboxId) const;

    /**
     * @brief Get sandbox for a module
     * @param moduleId The module ID
     * @return Sandbox ID or empty string if not found
     */
    std::string getSandboxForModule(const std::string& moduleId) const;

    /**
     * @brief Get all active sandboxes
     * @return Vector of sandbox IDs
     */
    std::vector<std::string> getActiveSandboxes() const;

    /**
     * @brief Check if a module is sandboxed
     * @param moduleId The module ID
     * @return true if sandboxed
     */
    bool isSandboxed(const std::string& moduleId) const;

    /**
     * @brief Set default sandbox configuration
     * @param config Default configuration
     */
    void setDefaultConfig(const SandboxConfig& config);

    /**
     * @brief Get default sandbox configuration
     * @return Default configuration
     */
    SandboxConfig getDefaultConfig() const;

    /**
     * @brief Enable sandboxing globally
     * @param enabled true to enable
     */
    void setSandboxingEnabled(bool enabled);

    /**
     * @brief Check if sandboxing is enabled
     * @return true if enabled
     */
    bool isSandboxingEnabled() const;

    /**
     * @brief Load sandbox policies from configuration
     * @param configPath Path to configuration file
     * @return true if loaded successfully
     */
    bool loadPolicies(const std::string& configPath);

    /**
     * @brief Save sandbox policies to configuration
     * @param configPath Path to configuration file
     * @return true if saved successfully
     */
    bool savePolicies(const std::string& configPath) const;

private:
    /**
     * @brief Private constructor for singleton
     */
    SandboxManager();

    /**
     * @brief Destructor
     */
    ~SandboxManager();

    class Impl;
    std::unique_ptr<Impl> pImpl_;

    /**
     * @brief Generate unique sandbox ID
     * @return Sandbox ID
     */
    std::string generateSandboxId();

    /**
     * @brief Setup process-level sandbox
     * @param sandboxId The sandbox ID
     * @param config Sandbox configuration
     * @return true if successful
     */
    bool setupProcessSandbox(const std::string& sandboxId, const SandboxConfig& config);

    /**
     * @brief Setup namespace-based sandbox (Linux)
     * @param sandboxId The sandbox ID
     * @param config Sandbox configuration
     * @return true if successful
     */
    bool setupNamespaceSandbox(const std::string& sandboxId, const SandboxConfig& config);

    /**
     * @brief Setup seccomp filtering
     * @param sandboxId The sandbox ID
     * @param config Sandbox configuration
     * @return true if successful
     */
    bool setupSeccompFilter(const std::string& sandboxId, const SandboxConfig& config);

    /**
     * @brief Apply AppArmor profile
     * @param sandboxId The sandbox ID
     * @param profile Profile name
     * @return true if successful
     */
    bool applyAppArmorProfile(const std::string& sandboxId, const std::string& profile);

    /**
     * @brief Apply SELinux context
     * @param sandboxId The sandbox ID
     * @param context SELinux context
     * @return true if successful
     */
    bool applySELinuxContext(const std::string& sandboxId, const std::string& context);

    /**
     * @brief Set resource limits for sandboxed process
     * @param config Sandbox configuration
     * @return true if successful
     */
    bool setResourceLimits(const SandboxConfig& config);

    /**
     * @brief Drop privileges in sandboxed process
     * @return true if successful
     */
    bool dropPrivileges();
};

} // namespace security
} // namespace cdmf

#endif // CDMF_SANDBOX_MANAGER_H
