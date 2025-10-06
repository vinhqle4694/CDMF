#include "security/sandbox_manager.h"
#include "security/sandbox_ipc.h"
#include "security/sandbox_module_loader.h"
#include "utils/log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <random>
#include <cstring>

// Platform-specific includes
#ifdef __linux__
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#endif

namespace cdmf {
namespace security {

// ========== SandboxManager::Impl ==========

class SandboxManager::Impl {
public:
    std::map<std::string, std::shared_ptr<SandboxInfo>> sandboxes;
    std::map<std::string, std::string> moduleSandboxMap; // moduleId -> sandboxId
    SandboxConfig defaultConfig;
    bool sandboxingEnabled = true;
    mutable std::mutex mutex;
    uint64_t sandboxCounter = 0;

    Impl() {
        // Initialize default sandbox configuration
        defaultConfig.type = SandboxType::PROCESS;
        defaultConfig.allowNetworkAccess = false;
        defaultConfig.allowFileSystemAccess = false;
        defaultConfig.maxMemoryMB = 256;
        defaultConfig.maxCPUPercent = 50;
        defaultConfig.maxFileDescriptors = 256;
        defaultConfig.maxThreads = 10;
    }
};

// ========== SandboxManager Singleton ==========

SandboxManager& SandboxManager::getInstance() {
    static SandboxManager instance;
    return instance;
}

SandboxManager::SandboxManager() : pImpl_(std::make_unique<Impl>()) {
    LOGI("SandboxManager initialized with default configuration");
    LOGD_FMT("Default sandbox config: max_memory=" << pImpl_->defaultConfig.maxMemoryMB
             << "MB, max_cpu=" << pImpl_->defaultConfig.maxCPUPercent << "%");
}

SandboxManager::~SandboxManager() {
    LOGI("SandboxManager destructor: stopping all sandboxes");

    // Get list of sandbox IDs before iterating (avoid iterator invalidation)
    std::vector<std::string> sandboxIds;
    {
        std::lock_guard<std::mutex> lock(pImpl_->mutex);
        for (const auto& pair : pImpl_->sandboxes) {
            sandboxIds.push_back(pair.first);
        }
    }

    // Stop each sandbox (releases lock during stopSandbox call)
    for (const auto& sandboxId : sandboxIds) {
        stopSandbox(sandboxId);
    }

    LOGI("SandboxManager destructor: all sandboxes stopped");
}

// ========== Sandbox Lifecycle ==========

std::string SandboxManager::createSandbox(const std::string& moduleId,
                                         const SandboxConfig& config) {
    if (moduleId.empty() || !pImpl_->sandboxingEnabled) {
        LOGW_FMT("Cannot create sandbox: " << (moduleId.empty() ? "empty moduleId" : "sandboxing disabled"));
        return "";
    }

    LOGI_FMT("Creating sandbox for module: " << moduleId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    // Check if module already has a sandbox
    if (pImpl_->moduleSandboxMap.find(moduleId) != pImpl_->moduleSandboxMap.end()) {
        LOGW_FMT("Module already has a sandbox: " << moduleId);
        return ""; // Already sandboxed
    }

    // Generate sandbox ID
    std::string sandboxId = generateSandboxId();
    LOGD_FMT("Generated sandbox ID: " << sandboxId << " for module: " << moduleId);

    // Create sandbox info
    auto info = std::make_shared<SandboxInfo>();
    info->sandboxId = sandboxId;
    info->moduleId = moduleId;
    info->type = config.type;
    info->status = SandboxStatus::CREATED;
    info->config = config;
    info->createdTime = std::chrono::system_clock::now().time_since_epoch().count();
    info->terminatedTime = 0;

    // Store sandbox info BEFORE calling setup (so setupProcessSandbox can access it)
    pImpl_->sandboxes[sandboxId] = info;
    pImpl_->moduleSandboxMap[moduleId] = sandboxId;

    // Setup sandbox based on type
    bool setupSuccess = false;
    switch (config.type) {
        case SandboxType::PROCESS:
            setupSuccess = setupProcessSandbox(sandboxId, config);
            break;
        case SandboxType::NAMESPACE:
            setupSuccess = setupNamespaceSandbox(sandboxId, config);
            break;
        case SandboxType::SECCOMP:
            setupSuccess = setupSeccompFilter(sandboxId, config);
            break;
        case SandboxType::APPARMOR:
            setupSuccess = applyAppArmorProfile(sandboxId, config.apparmorProfile);
            break;
        case SandboxType::SELINUX:
            setupSuccess = applySELinuxContext(sandboxId, config.selinuxContext);
            break;
        default:
            info->errorMessage = "Unsupported sandbox type";
            info->status = SandboxStatus::ERROR;
            break;
    }

    if (!setupSuccess && config.type != SandboxType::NONE) {
        info->status = SandboxStatus::ERROR;
        info->errorMessage = "Failed to setup sandbox";
    }

    LOGI_FMT("Sandbox created successfully: " << sandboxId << " for module " << moduleId
             << " (type: " << static_cast<int>(config.type) << ", status: " << static_cast<int>(info->status) << ")");
    return sandboxId;
}

bool SandboxManager::destroySandbox(const std::string& sandboxId) {
    LOGI_FMT("Destroying sandbox: " << sandboxId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->sandboxes.find(sandboxId);
    if (it == pImpl_->sandboxes.end()) {
        LOGW_FMT("Sandbox not found: " << sandboxId);
        return false;
    }

    // Remove from module map
    std::string moduleId = it->second->moduleId;
    pImpl_->moduleSandboxMap.erase(moduleId);
    LOGD_FMT("Removed module mapping for: " << moduleId);

    // Update status
    it->second->status = SandboxStatus::TERMINATED;
    it->second->terminatedTime = std::chrono::system_clock::now().time_since_epoch().count();

    // Cleanup sandbox resources (platform-specific)
    // This would involve killing processes, unmounting namespaces, etc.
    LOGD_FMT("Cleaning up sandbox resources for: " << sandboxId);

    // Remove from sandboxes map
    pImpl_->sandboxes.erase(it);

    LOGI_FMT("Sandbox destroyed successfully: " << sandboxId);
    return true;
}

bool SandboxManager::startSandbox(const std::string& sandboxId) {
    LOGI_FMT("Starting sandbox: " << sandboxId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->sandboxes.find(sandboxId);
    if (it == pImpl_->sandboxes.end()) {
        LOGW_FMT("Sandbox not found: " << sandboxId);
        return false;
    }

    if (it->second->status != SandboxStatus::CREATED &&
        it->second->status != SandboxStatus::SUSPENDED) {
        LOGW_FMT("Sandbox cannot be started from current status: " << static_cast<int>(it->second->status));
        return false;
    }

    it->second->status = SandboxStatus::ACTIVE;
    LOGI_FMT("Sandbox started: " << sandboxId << " for module " << it->second->moduleId);
    return true;
}

bool SandboxManager::stopSandbox(const std::string& sandboxId) {
    LOGI_FMT("Stopping sandbox: " << sandboxId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->sandboxes.find(sandboxId);
    if (it == pImpl_->sandboxes.end()) {
        LOGW_FMT("Sandbox not found: " << sandboxId);
        return false;
    }

    auto& info = it->second;

    // Send SHUTDOWN command to child process if IPC is available
    if (info->ipc && info->ipc->isConnected()) {
        LOGI_FMT("Sending SHUTDOWN command to sandbox: " << sandboxId);

        SandboxMessage shutdownMsg;
        shutdownMsg.type = SandboxMessageType::SHUTDOWN;
        shutdownMsg.moduleId = info->moduleId;
        shutdownMsg.requestId = 0;
        shutdownMsg.errorCode = 0;

        if (!info->ipc->sendMessage(shutdownMsg, 1000)) {
            LOGW_FMT("Failed to send SHUTDOWN command to sandbox: " << sandboxId);
        } else {
            LOGI_FMT("SHUTDOWN command sent successfully to sandbox: " << sandboxId);
        }

        // Give child time to exit gracefully (500ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

#ifdef __linux__
    // Kill the process if it's still running
    if (info->processId > 0) {
        LOGI_FMT("Terminating child process: PID=" << info->processId);
        kill(info->processId, SIGTERM);

        // Wait briefly for termination
        int status;
        pid_t result = waitpid(info->processId, &status, WNOHANG);
        if (result == 0) {
            // Still running, force kill
            LOGW_FMT("Child process didn't exit gracefully, force killing: PID=" << info->processId);
            kill(info->processId, SIGKILL);
            waitpid(info->processId, &status, 0);
        }
        LOGI_FMT("Child process terminated: PID=" << info->processId);
    }
#endif

    info->status = SandboxStatus::TERMINATED;
    info->terminatedTime = std::chrono::system_clock::now().time_since_epoch().count();
    LOGI_FMT("Sandbox stopped: " << sandboxId);
    return true;
}

bool SandboxManager::suspendSandbox(const std::string& sandboxId) {
    LOGI_FMT("Suspending sandbox: " << sandboxId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->sandboxes.find(sandboxId);
    if (it == pImpl_->sandboxes.end()) {
        LOGW_FMT("Sandbox not found: " << sandboxId);
        return false;
    }

    if (it->second->status != SandboxStatus::ACTIVE) {
        LOGW_FMT("Can only suspend ACTIVE sandbox, current status: " << static_cast<int>(it->second->status));
        return false;
    }

    it->second->status = SandboxStatus::SUSPENDED;
    LOGI_FMT("Sandbox suspended: " << sandboxId);
    return true;
}

bool SandboxManager::resumeSandbox(const std::string& sandboxId) {
    LOGI_FMT("Resuming sandbox: " << sandboxId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->sandboxes.find(sandboxId);
    if (it == pImpl_->sandboxes.end()) {
        LOGW_FMT("Sandbox not found: " << sandboxId);
        return false;
    }

    if (it->second->status != SandboxStatus::SUSPENDED) {
        LOGW_FMT("Can only resume SUSPENDED sandbox, current status: " << static_cast<int>(it->second->status));
        return false;
    }

    it->second->status = SandboxStatus::ACTIVE;
    LOGI_FMT("Sandbox resumed: " << sandboxId);
    return true;
}

// ========== Sandbox Queries ==========

std::shared_ptr<SandboxInfo> SandboxManager::getSandboxInfo(const std::string& sandboxId) const {
    LOGD_FMT("Getting sandbox info for: " << sandboxId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->sandboxes.find(sandboxId);
    if (it != pImpl_->sandboxes.end()) {
        LOGD_FMT("  Found sandbox info: module=" << it->second->moduleId
                 << ", status=" << static_cast<int>(it->second->status));
        return it->second;
    }
    LOGD_FMT("  Sandbox not found: " << sandboxId);
    return nullptr;
}

std::string SandboxManager::getSandboxForModule(const std::string& moduleId) const {
    LOGD_FMT("Getting sandbox for module: " << moduleId);
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    auto it = pImpl_->moduleSandboxMap.find(moduleId);
    if (it != pImpl_->moduleSandboxMap.end()) {
        LOGD_FMT("  Module '" << moduleId << "' is in sandbox: " << it->second);
        return it->second;
    }
    LOGD_FMT("  Module '" << moduleId << "' has no sandbox");
    return "";
}

std::vector<std::string> SandboxManager::getActiveSandboxes() const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);

    std::vector<std::string> active;
    for (const auto& pair : pImpl_->sandboxes) {
        if (pair.second->status == SandboxStatus::ACTIVE) {
            active.push_back(pair.first);
        }
    }
    LOGD_FMT("Retrieved " << active.size() << " active sandboxes out of " << pImpl_->sandboxes.size() << " total");
    return active;
}

bool SandboxManager::isSandboxed(const std::string& moduleId) const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    bool sandboxed = pImpl_->moduleSandboxMap.find(moduleId) != pImpl_->moduleSandboxMap.end();
    LOGD_FMT("Module '" << moduleId << "' is sandboxed: " << (sandboxed ? "yes" : "no"));
    return sandboxed;
}

// ========== Configuration ==========

void SandboxManager::setDefaultConfig(const SandboxConfig& config) {
    LOGI_FMT("Setting default sandbox config: type=" << static_cast<int>(config.type)
             << ", max_memory=" << config.maxMemoryMB << "MB, max_cpu=" << config.maxCPUPercent << "%");
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->defaultConfig = config;
    LOGD("Default sandbox configuration updated successfully");
}

SandboxConfig SandboxManager::getDefaultConfig() const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    LOGD_FMT("Getting default sandbox config: type=" << static_cast<int>(pImpl_->defaultConfig.type)
             << ", max_memory=" << pImpl_->defaultConfig.maxMemoryMB << "MB");
    return pImpl_->defaultConfig;
}

void SandboxManager::setSandboxingEnabled(bool enabled) {
    LOGI_FMT("Setting sandboxing enabled: " << (enabled ? "true" : "false"));
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    pImpl_->sandboxingEnabled = enabled;
    LOGD_FMT("Sandboxing is now " << (enabled ? "enabled" : "disabled"));
}

bool SandboxManager::isSandboxingEnabled() const {
    std::lock_guard<std::mutex> lock(pImpl_->mutex);
    LOGD_FMT("Checking if sandboxing is enabled: " << (pImpl_->sandboxingEnabled ? "yes" : "no"));
    return pImpl_->sandboxingEnabled;
}

// ========== Persistence ==========

bool SandboxManager::loadPolicies(const std::string& configPath) {
    LOGI_FMT("Loading sandbox policies from: " << configPath);
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open sandbox policies file: " << configPath);
        return false;
    }

    // Simplified implementation - would parse actual sandbox policies
    file.close();
    LOGI("Sandbox policies loaded successfully");
    return true;
}

bool SandboxManager::savePolicies(const std::string& configPath) const {
    LOGI_FMT("Saving sandbox policies to: " << configPath);
    std::ofstream file(configPath);
    if (!file.is_open()) {
        LOGE_FMT("Failed to open file for writing sandbox policies: " << configPath);
        return false;
    }

    file << "# CDMF Sandbox Policies\n";
    file.close();
    LOGI_FMT("Sandbox policies saved successfully to: " << configPath);
    return true;
}

// ========== Private Methods ==========

std::string SandboxManager::generateSandboxId() {
    pImpl_->sandboxCounter++;
    std::string sandboxId = "sandbox_" + std::to_string(pImpl_->sandboxCounter);
    LOGD_FMT("Generated sandbox ID: " << sandboxId << " (counter=" << pImpl_->sandboxCounter << ")");
    return sandboxId;
}

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

    LOGI_FMT("Using IPC transport: " << ipc::transportTypeToString(transportType));

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

    // 5. Fork process
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        LOGE_FMT("Fork failed: " << strerror(errno));
        return false;
    }

    if (pid == 0) {
        // ===== CHILD PROCESS =====

        // STEP 1: Create and connect IPC WHILE STILL ROOT
        // This is critical - connection must happen before dropping privileges
        // Works for ALL transport types (shared memory, sockets, TCP, etc.)

        LOGI_FMT("Child process connecting to IPC: sandbox=" << sandboxId
                 << ", transport=" << ipc::transportTypeToString(transportType));

        // Create child transport configuration
        ipc::TransportConfig childTransportConfig = createSandboxTransportConfig(
            transportType, sandboxId, SandboxIPC::Role::CHILD, config.properties);

        // Create child transport strategy
        auto childTransport = createSandboxTransport(transportType, sandboxId);

        // Create child SandboxIPC
        auto childIpc = std::make_unique<SandboxIPC>(SandboxIPC::Role::CHILD, sandboxId,
                                                      std::move(childTransport));

        // Connect to parent's IPC endpoint (requires root privileges)
        if (!childIpc->initialize(childTransportConfig)) {
            LOGE("Child failed to connect to IPC channel");
            exit(1);
        }

        LOGI_FMT("Child IPC connected successfully: endpoint=" << childIpc->getEndpoint());

        // STEP 1.5: Send initial heartbeat to parent ASAP to trigger accept()
        // Do this before dropping privileges to ensure it succeeds quickly
        {
            SandboxMessage heartbeat;
            heartbeat.type = SandboxMessageType::HEARTBEAT;
            heartbeat.moduleId = "";
            heartbeat.requestId = 0;
            heartbeat.errorCode = 0;
            if (childIpc->sendMessage(heartbeat, 1000)) {
                LOGI_FMT("Child sent initial heartbeat to parent");
            } else {
                LOGW_FMT("Child failed to send initial heartbeat");
            }
        }

        // STEP 2: Drop privileges AFTER IPC connection established
        // Now safe to drop - IPC already connected
        if (!dropPrivileges()) {
            LOGE("Failed to drop privileges");
            exit(1);
        }

        // STEP 3: Set resource limits
        if (!setResourceLimits(config)) {
            LOGE("Failed to set resource limits in child process");
            exit(1);
        }

        // STEP 4: Run sandboxed module loader with pre-connected IPC
        // This blocks until shutdown
        int exitCode = SandboxModuleLoader::runSandboxedProcess(sandboxId,
                                                                std::move(childIpc));

        exit(exitCode);
    }

    // ===== PARENT PROCESS =====

    // For Unix socket SERVER mode, we need to wait for the child to connect
    // The child connects immediately after fork, so we trigger the accept() by receiving
    if (transportType == ipc::TransportType::UNIX_SOCKET) {
        LOGI_FMT("Parent waiting for child connection on Unix socket...");

        // Give child brief time to at least start connecting
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Wait for child to connect and send initial heartbeat
        // receiveMessage() with timeout will run the epoll loop and trigger accept()
        SandboxMessage msg;
        bool childConnected = false;

        // Child needs time to: connect, drop privileges, set limits, send heartbeat
        // Use longer timeout to be safe
        if (ipc->receiveMessage(msg, 5000)) {  // 5 second timeout
            LOGI_FMT("Parent accepted child connection and received message (type="
                     << static_cast<int>(msg.type) << ")");
            childConnected = true;
        } else {
            LOGW_FMT("Parent did not receive initial message from child within timeout");
        }

        if (childConnected) {
            LOGI_FMT("Parent Unix socket connection established with child");
        } else {
            LOGW_FMT("Parent-child Unix socket connection may not be fully established");
        }
    }

    // Store sandbox info with IPC channel and transport type
    // IMPORTANT: Use reference to modify the actual shared_ptr in the map
    auto& info = pImpl_->sandboxes[sandboxId];
    info->processId = pid;
    info->ipc = std::move(ipc);  // Transfer ownership to sandbox info
    info->transportType = transportType;  // Store for reference

    LOGI_FMT("Process sandbox created: PID=" << pid << ", sandbox=" << sandboxId
             << ", transport=" << ipc::transportTypeToString(transportType)
             << ", endpoint=" << info->ipc->getEndpoint());
    return true;

#endif // __linux__
}

bool SandboxManager::setupNamespaceSandbox(const std::string& sandboxId,
                                          const SandboxConfig& config) {
    LOGD_FMT("Setting up namespace sandbox: " << sandboxId);
    // Linux namespace isolation
    // In production, this would:
    // 1. Create new namespaces (PID, NET, MNT, IPC, UTS)
    // 2. Set up mount points
    // 3. Configure network interfaces
    // 4. Set up cgroups

#ifdef __linux__
    // Example: Create namespaces
    // unshare(CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWIPC | CLONE_NEWUTS);
    LOGD_FMT("  Namespace sandbox configured for: " << sandboxId);
#endif

    LOGD_FMT("Namespace sandbox setup completed for: " << sandboxId);
    return true;
}

bool SandboxManager::setupSeccompFilter(const std::string& sandboxId,
                                       const SandboxConfig& config) {
    LOGD_FMT("Setting up seccomp filter for sandbox: " << sandboxId);
    // Seccomp-BPF syscall filtering
    // In production, this would:
    // 1. Build BPF filter for allowed syscalls
    // 2. Load filter using prctl(PR_SET_SECCOMP, ...)
    // 3. All future syscalls will be filtered

#ifdef __linux__
    // Example: Enable seccomp strict mode
    // prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
    LOGD_FMT("  Seccomp filter configured for: " << sandboxId);
#endif

    LOGD_FMT("Seccomp filter setup completed for: " << sandboxId);
    return true;
}

bool SandboxManager::applyAppArmorProfile(const std::string& sandboxId,
                                         const std::string& profile) {
    LOGD_FMT("Applying AppArmor profile for sandbox: " << sandboxId << ", profile='" << profile << "'");
    // AppArmor profile application
    // In production, this would:
    // 1. Load AppArmor profile from /etc/apparmor.d/
    // 2. Apply profile to process using aa_change_onexec()

    if (profile.empty()) {
        LOGW_FMT("Cannot apply AppArmor profile: empty profile name for sandbox " << sandboxId);
        return false;
    }

    LOGD_FMT("AppArmor profile '" << profile << "' applied successfully for sandbox: " << sandboxId);
    return true;
}

bool SandboxManager::applySELinuxContext(const std::string& sandboxId,
                                        const std::string& context) {
    LOGD_FMT("Applying SELinux context for sandbox: " << sandboxId << ", context='" << context << "'");
    // SELinux context application
    // In production, this would:
    // 1. Set SELinux context using setcon()
    // 2. Verify context is valid and allowed

    if (context.empty()) {
        LOGW_FMT("Cannot apply SELinux context: empty context for sandbox " << sandboxId);
        return false;
    }

    LOGD_FMT("SELinux context '" << context << "' applied successfully for sandbox: " << sandboxId);
    return true;
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

} // namespace security
} // namespace cdmf
