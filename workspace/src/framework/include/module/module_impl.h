#ifndef CDMF_MODULE_IMPL_H
#define CDMF_MODULE_IMPL_H

#include "module/module.h"
#include "module/module_handle.h"
#include "module/manifest_parser.h"
#include <atomic>
#include <shared_mutex>
#include <memory>

namespace cdmf {

// Forward declaration
class Framework;
class ModuleContextImpl;

/**
 * @brief Concrete module implementation
 *
 * Implements the Module interface with full lifecycle management.
 *
 * Thread safety:
 * - All public methods are thread-safe
 * - Uses shared_mutex for read/write locking
 * - State transitions are atomic
 */
class ModuleImpl : public Module {
public:
    /**
     * @brief Construct module
     *
     * @param moduleId Unique module ID
     * @param handle Module handle (takes ownership)
     * @param manifest Parsed manifest
     * @param framework Framework instance (not owned)
     */
    ModuleImpl(uint64_t moduleId,
               std::unique_ptr<ModuleHandle> handle,
               const ModuleManifest& manifest,
               Framework* framework);

    /**
     * @brief Destructor
     */
    ~ModuleImpl() override;

    // Prevent copying
    ModuleImpl(const ModuleImpl&) = delete;
    ModuleImpl& operator=(const ModuleImpl&) = delete;

    // ==================================================================
    // Identity (Module interface)
    // ==================================================================

    std::string getSymbolicName() const override;
    Version getVersion() const override;
    std::string getLocation() const override;
    uint64_t getModuleId() const override;

    // ==================================================================
    // Lifecycle (Module interface)
    // ==================================================================

    void start() override;
    void stop() override;
    void update(const std::string& location) override;
    void uninstall() override;

    // ==================================================================
    // State (Module interface)
    // ==================================================================

    ModuleState getState() const override;

    // ==================================================================
    // Context (Module interface)
    // ==================================================================

    IModuleContext* getContext() override;

    // ==================================================================
    // Services (Module interface - PHASE_5 stubs)
    // ==================================================================

    std::vector<ServiceRegistration> getRegisteredServices() const override;
    std::vector<ServiceReference> getServicesInUse() const override;

    // ==================================================================
    // Metadata (Module interface)
    // ==================================================================

    const nlohmann::json& getManifest() const override;
    std::map<std::string, std::string> getHeaders() const override;

    // ==================================================================
    // Listeners (Module interface)
    // ==================================================================

    void addModuleListener(IModuleListener* listener) override;
    void removeModuleListener(IModuleListener* listener) override;

    // ==================================================================
    // Internal (used by Framework)
    // ==================================================================

    /**
     * @brief Transition to new state
     * @param newState Target state
     */
    void transitionTo(ModuleState newState);

private:
    // Identity
    uint64_t moduleId_;
    ModuleManifest manifest_;

    // State
    std::atomic<ModuleState> state_;

    // Resources
    std::unique_ptr<ModuleHandle> handle_;
    std::unique_ptr<IModuleActivator> activator_;
    std::unique_ptr<ModuleContextImpl> context_;

    // Framework reference (not owned)
    Framework* framework_;

    // Listeners
    std::vector<IModuleListener*> listeners_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    /**
     * @brief Fire module event
     * @param eventType Event type
     */
    void fireModuleEvent(ModuleEventType eventType);

    /**
     * @brief Create activator instance
     */
    void createActivator();

    /**
     * @brief Destroy activator instance
     */
    void destroyActivator();
};

} // namespace cdmf

#endif // CDMF_MODULE_IMPL_H
