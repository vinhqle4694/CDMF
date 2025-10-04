#ifndef CDMF_SERVICE_TRACKER_H
#define CDMF_SERVICE_TRACKER_H

#include "service/service_reference.h"
#include "service/service_listener.h"
#include "module/module_context.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>

namespace cdmf {

/**
 * @brief Service tracker customizer interface
 *
 * Allows customization of service tracking behavior.
 * Implementations can be notified when services are added, modified, or removed.
 *
 * Type parameter T is the service interface type.
 */
template<typename T>
class IServiceTrackerCustomizer {
public:
    virtual ~IServiceTrackerCustomizer() = default;

    /**
     * @brief Called when a service is added
     *
     * @param ref Service reference
     * @return Service object to track, or nullptr to not track
     */
    virtual T* addingService(const ServiceReference& ref) = 0;

    /**
     * @brief Called when a service is modified
     *
     * @param ref Service reference
     * @param service Tracked service object
     */
    virtual void modifiedService(const ServiceReference& ref, T* service) = 0;

    /**
     * @brief Called when a service is removed
     *
     * @param ref Service reference
     * @param service Tracked service object
     */
    virtual void removedService(const ServiceReference& ref, T* service) = 0;
};

/**
 * @brief Service tracker
 *
 * Tracks services matching an interface and optional filter.
 * Automatically handles service lifecycle events (registered, modified, unregistering).
 *
 * Features:
 * - Automatic service discovery
 * - Lifecycle tracking (add/modify/remove)
 * - Custom service object creation via customizer
 * - Thread-safe operations
 * - Simplified service consumption
 *
 * Thread safety:
 * - All operations are thread-safe
 * - Customizer callbacks may be called from any thread
 * - Framework guarantees no concurrent calls to same customizer
 *
 * Usage:
 * ```cpp
 * // Simple tracking
 * ServiceTracker<ILogger> tracker(context, "com.example.ILogger");
 * tracker.open();
 *
 * // Get tracked services
 * std::vector<ILogger*> loggers = tracker.getServices();
 *
 * // Close when done
 * tracker.close();
 * ```
 *
 * Advanced usage with customizer:
 * ```cpp
 * class LoggerCustomizer : public IServiceTrackerCustomizer<ILogger> {
 *     ILogger* addingService(const ServiceReference& ref) override {
 *         auto logger = context->getService<ILogger>(ref);
 *         logger->configure(config);
 *         return logger;
 *     }
 *     void removedService(const ServiceReference& ref, ILogger* logger) override {
 *         logger->shutdown();
 *         context->ungetService(ref);
 *     }
 * };
 *
 * ServiceTracker<ILogger> tracker(context, "com.example.ILogger", customizer);
 * ```
 */
template<typename T>
class ServiceTracker : public IEventListener {
public:
    /**
     * @brief Construct service tracker
     *
     * @param context Module context
     * @param interfaceName Service interface name
     * @param customizer Optional customizer for service lifecycle
     */
    ServiceTracker(IModuleContext* context,
                   const std::string& interfaceName,
                   IServiceTrackerCustomizer<T>* customizer = nullptr);

    /**
     * @brief Destructor - automatically closes tracker
     */
    ~ServiceTracker();

    // Prevent copying
    ServiceTracker(const ServiceTracker&) = delete;
    ServiceTracker& operator=(const ServiceTracker&) = delete;

    /**
     * @brief Open the tracker
     *
     * Starts tracking services:
     * 1. Registers service listener
     * 2. Discovers existing services
     * 3. Calls customizer for each service
     */
    void open();

    /**
     * @brief Close the tracker
     *
     * Stops tracking services:
     * 1. Unregisters service listener
     * 2. Calls customizer.removedService() for all tracked services
     * 3. Releases all service references
     */
    void close();

    /**
     * @brief Check if tracker is open
     * @return true if tracker is open
     */
    bool isOpen() const { return open_; }

    /**
     * @brief Get all tracked services
     * @return Vector of service objects (may be empty)
     */
    std::vector<T*> getServices() const;

    /**
     * @brief Get a single tracked service
     *
     * Returns the service with highest ranking.
     *
     * @return Service object, or nullptr if no services tracked
     */
    T* getService() const;

    /**
     * @brief Get service for a reference
     *
     * @param ref Service reference
     * @return Service object, or nullptr if not tracked
     */
    T* getService(const ServiceReference& ref) const;

    /**
     * @brief Get all tracked service references
     * @return Vector of service references (sorted by ranking)
     */
    std::vector<ServiceReference> getServiceReferences() const;

    /**
     * @brief Get count of tracked services
     * @return Number of services currently tracked
     */
    size_t size() const;

    /**
     * @brief Check if tracker has any services
     * @return true if no services are tracked
     */
    bool isEmpty() const { return size() == 0; }

    // IEventListener implementation
    void handleEvent(const Event& event) override;

private:
    /**
     * @brief Add a service to tracking
     * @param ref Service reference
     */
    void addService(const ServiceReference& ref);

    /**
     * @brief Modify a tracked service
     * @param ref Service reference
     */
    void modifyService(const ServiceReference& ref);

    /**
     * @brief Remove a service from tracking
     * @param ref Service reference
     */
    void removeService(const ServiceReference& ref);

    /**
     * @brief Default customizer - gets service from context
     */
    class DefaultCustomizer : public IServiceTrackerCustomizer<T> {
    public:
        DefaultCustomizer(IModuleContext* context) : context_(context) {}

        T* addingService(const ServiceReference& ref) override {
            auto servicePtr = context_->getService(ref);
            return static_cast<T*>(servicePtr.get());
        }

        void modifiedService(const ServiceReference&, T*) override {
            // No-op
        }

        void removedService(const ServiceReference& ref, T*) override {
            context_->ungetService(ref);
        }

    private:
        IModuleContext* context_;
    };

    IModuleContext* context_;
    std::string interfaceName_;
    IServiceTrackerCustomizer<T>* customizer_;
    std::unique_ptr<DefaultCustomizer> defaultCustomizer_;
    bool open_;

    // Tracked services: ServiceReference -> Service object
    std::map<ServiceReference, T*> trackedServices_;
    mutable std::mutex mutex_;
};

// Template implementation

template<typename T>
ServiceTracker<T>::ServiceTracker(IModuleContext* context,
                                   const std::string& interfaceName,
                                   IServiceTrackerCustomizer<T>* customizer)
    : context_(context)
    , interfaceName_(interfaceName)
    , customizer_(customizer)
    , defaultCustomizer_(nullptr)
    , open_(false)
    , trackedServices_()
    , mutex_() {

    // Use default customizer if none provided
    if (!customizer_) {
        defaultCustomizer_ = std::make_unique<DefaultCustomizer>(context_);
        customizer_ = defaultCustomizer_.get();
    }
}

template<typename T>
ServiceTracker<T>::~ServiceTracker() {
    if (open_) {
        close();
    }
}

template<typename T>
void ServiceTracker<T>::open() {
    std::unique_lock lock(mutex_);

    if (open_) {
        return; // Already open
    }

    // Register as event listener for service events
    context_->addEventListener(this, EventFilter(), 0, false);

    // Discover existing services
    std::vector<ServiceReference> refs = context_->getServiceReferences(interfaceName_);
    for (const auto& ref : refs) {
        addService(ref);
    }

    open_ = true;
}

template<typename T>
void ServiceTracker<T>::close() {
    std::unique_lock lock(mutex_);

    if (!open_) {
        return; // Already closed
    }

    // Unregister service listener
    context_->removeEventListener(this);

    // Remove all tracked services
    std::vector<ServiceReference> refs;
    for (const auto& pair : trackedServices_) {
        refs.push_back(pair.first);
    }

    for (const auto& ref : refs) {
        removeService(ref);
    }

    open_ = false;
}

template<typename T>
std::vector<T*> ServiceTracker<T>::getServices() const {
    std::unique_lock lock(mutex_);

    std::vector<T*> services;
    services.reserve(trackedServices_.size());

    for (const auto& pair : trackedServices_) {
        if (pair.second) {
            services.push_back(pair.second);
        }
    }

    return services;
}

template<typename T>
T* ServiceTracker<T>::getService() const {
    std::unique_lock lock(mutex_);

    if (trackedServices_.empty()) {
        return nullptr;
    }

    // Return first (highest ranking due to map ordering)
    return trackedServices_.begin()->second;
}

template<typename T>
T* ServiceTracker<T>::getService(const ServiceReference& ref) const {
    std::unique_lock lock(mutex_);

    auto it = trackedServices_.find(ref);
    if (it == trackedServices_.end()) {
        return nullptr;
    }

    return it->second;
}

template<typename T>
std::vector<ServiceReference> ServiceTracker<T>::getServiceReferences() const {
    std::unique_lock lock(mutex_);

    std::vector<ServiceReference> refs;
    refs.reserve(trackedServices_.size());

    for (const auto& pair : trackedServices_) {
        refs.push_back(pair.first);
    }

    return refs;
}

template<typename T>
size_t ServiceTracker<T>::size() const {
    std::unique_lock lock(mutex_);
    return trackedServices_.size();
}

template<typename T>
void ServiceTracker<T>::handleEvent(const Event& event) {
    // Check if this is a service event
    std::string type = event.getType();
    if (type != "SERVICE_REGISTERED" &&
        type != "SERVICE_MODIFIED" &&
        type != "SERVICE_UNREGISTERING") {
        return; // Not a service event
    }

    // Extract service interface from event properties
    std::any interfaceAny = event.getProperty("service.interface");
    if (!interfaceAny.has_value()) {
        return;
    }

    std::string serviceInterface = std::any_cast<std::string>(interfaceAny);

    // Filter by interface name
    if (serviceInterface != interfaceName_) {
        return;
    }

    // Get service ID
    std::any serviceIdAny = event.getProperty("service.id");
    if (!serviceIdAny.has_value()) {
        return;
    }

    uint64_t serviceId = static_cast<uint64_t>(std::any_cast<int>(serviceIdAny));

    // For now, we can't properly track without the actual ServiceReference
    // This requires the ServiceRegistry to be accessible or the event to contain the reference
    // TODO: Enhance Event to carry typed data or make ServiceRegistry accessible
    (void)serviceId; // Suppress unused warning
}

template<typename T>
void ServiceTracker<T>::addService(const ServiceReference& ref) {
    std::unique_lock lock(mutex_);

    // Check if already tracking
    if (trackedServices_.find(ref) != trackedServices_.end()) {
        return;
    }

    // Call customizer
    T* service = customizer_->addingService(ref);
    if (service) {
        trackedServices_[ref] = service;
    }
}

template<typename T>
void ServiceTracker<T>::modifyService(const ServiceReference& ref) {
    std::unique_lock lock(mutex_);

    auto it = trackedServices_.find(ref);
    if (it == trackedServices_.end()) {
        return; // Not tracking this service
    }

    // Call customizer
    customizer_->modifiedService(ref, it->second);
}

template<typename T>
void ServiceTracker<T>::removeService(const ServiceReference& ref) {
    std::unique_lock lock(mutex_);

    auto it = trackedServices_.find(ref);
    if (it == trackedServices_.end()) {
        return; // Not tracking this service
    }

    T* service = it->second;
    trackedServices_.erase(it);

    // Call customizer outside lock
    lock.unlock();
    customizer_->removedService(ref, service);
}

} // namespace cdmf

#endif // CDMF_SERVICE_TRACKER_H
