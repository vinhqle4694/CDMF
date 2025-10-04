#ifndef CDMF_MODULE_ACTIVATOR_H
#define CDMF_MODULE_ACTIVATOR_H

#include "module/module_types.h"

namespace cdmf {

/**
 * @brief Module activator interface
 *
 * Each module must provide an implementation of this interface to participate
 * in the module lifecycle. The activator is called during module start and stop.
 *
 * Module authors implement this interface and export factory functions via:
 * - createModuleActivator() - Create activator instance
 * - destroyModuleActivator() - Destroy activator instance
 *
 * Example implementation:
 * @code
 * class MyModuleActivator : public IModuleActivator {
 * public:
 *     void start(IModuleContext* context) override {
 *         // Register services
 *         context->registerService("com.example.IMyService", myService);
 *
 *         // Add event listeners
 *         context->addEventListener(listener, filter);
 *     }
 *
 *     void stop(IModuleContext* context) override {
 *         // Cleanup resources
 *         // Services are automatically unregistered
 *     }
 * };
 *
 * extern "C" {
 *     CDMF_EXPORT IModuleActivator* createModuleActivator() {
 *         return new MyModuleActivator();
 *     }
 *
 *     CDMF_EXPORT void destroyModuleActivator(IModuleActivator* activator) {
 *         delete activator;
 *     }
 * }
 * @endcode
 */
class IModuleActivator {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IModuleActivator() = default;

    /**
     * @brief Called when the module is starting
     *
     * This method is called during the module's transition from RESOLVED to ACTIVE.
     * The activator should:
     * - Register services the module provides
     * - Add event listeners
     * - Initialize module resources
     * - Acquire references to required services
     *
     * If this method throws an exception, the module transitions back to RESOLVED
     * and the start() operation fails.
     *
     * @param context Module context for framework interaction
     * @throws std::exception if module cannot start (logged, module stays RESOLVED)
     *
     * @note This method must complete within the configured timeout (default 10s)
     * @note This method is called with the module's state lock held
     */
    virtual void start(IModuleContext* context) = 0;

    /**
     * @brief Called when the module is stopping
     *
     * This method is called during the module's transition from ACTIVE to RESOLVED.
     * The activator should:
     * - Release all acquired service references
     * - Remove event listeners (optional, done automatically)
     * - Clean up module resources
     * - Unregister services (optional, done automatically)
     *
     * If this method throws an exception, the error is logged but the module
     * still transitions to RESOLVED.
     *
     * @param context Module context for framework interaction
     * @throws std::exception Exceptions are logged but module still stops
     *
     * @note This method must complete within the configured timeout (default 5s)
     * @note This method is called with the module's state lock held
     * @note Services are automatically unregistered after this method returns
     */
    virtual void stop(IModuleContext* context) = 0;
};

} // namespace cdmf

/**
 * @brief Module activator factory functions
 *
 * Every module shared library must export these two C functions:
 *
 * @code
 * extern "C" {
 *     CDMF_EXPORT IModuleActivator* createModuleActivator();
 *     CDMF_EXPORT void destroyModuleActivator(IModuleActivator* activator);
 * }
 * @endcode
 *
 * Platform-specific export macros:
 * - Linux/macOS: CDMF_EXPORT = __attribute__((visibility("default")))
 * - Windows: CDMF_EXPORT = __declspec(dllexport)
 */

// Platform-specific export macro
#if defined(_WIN32) || defined(_WIN64)
    #define CDMF_EXPORT __declspec(dllexport)
#else
    #define CDMF_EXPORT __attribute__((visibility("default")))
#endif

// Module activator factory function signatures
extern "C" {
    /**
     * @brief Create module activator instance
     * @return Pointer to new activator (caller must destroy via destroyModuleActivator)
     */
    typedef cdmf::IModuleActivator* (*CreateModuleActivatorFunc)();

    /**
     * @brief Destroy module activator instance
     * @param activator Activator to destroy (created by createModuleActivator)
     */
    typedef void (*DestroyModuleActivatorFunc)(cdmf::IModuleActivator* activator);
}

#endif // CDMF_MODULE_ACTIVATOR_H
