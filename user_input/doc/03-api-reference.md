# CDMF API Reference

## Table of Contents
1. [Framework API](#framework-api)
2. [Module API](#module-api)
3. [Service Registry API](#service-registry-api)
4. [Module Context API](#module-context-api)
5. [Event System API](#event-system-api)
6. [Configuration API](#configuration-api)
7. [Security API](#security-api)
8. [Utility APIs](#utility-apis)
9. [Constants and Enumerations](#constants-and-enumerations)
10. [Error Handling](#error-handling)

## Framework API

### FrameworkFactory

Factory class for creating framework instances.

```cpp
namespace cdmf {

class FrameworkFactory {
public:
    /**
     * Creates a new framework instance with the specified properties.
     *
     * @param props Configuration properties for the framework
     * @return Unique pointer to the created framework
     * @throws FrameworkException if creation fails
     */
    static std::unique_ptr<Framework> createFramework(
        const FrameworkProperties& props = FrameworkProperties()
    );

    /**
     * Creates a framework with a configuration file.
     *
     * @param configPath Path to JSON configuration file
     * @return Unique pointer to the created framework
     * @throws FrameworkException if creation fails or config invalid
     */
    static std::unique_ptr<Framework> createFrameworkFromConfig(
        const std::string& configPath
    );
};

} // namespace cdmf
```

### Framework

Main framework class managing the module system.

```cpp
namespace cdmf {

class Framework {
public:
    virtual ~Framework() = default;

    /**
     * Initializes the framework.
     * Must be called before start().
     *
     * @throws FrameworkException if initialization fails
     */
    virtual void init() = 0;

    /**
     * Starts the framework and all auto-start modules.
     *
     * @throws FrameworkException if start fails
     */
    virtual void start() = 0;

    /**
     * Stops the framework and all active modules.
     *
     * @param timeout Maximum time to wait for graceful shutdown (ms)
     * @throws FrameworkException if stop fails
     */
    virtual void stop(int timeout = 5000) = 0;

    /**
     * Waits for the framework to stop.
     * Blocks until stop() is called from another thread.
     */
    virtual void waitForStop() = 0;

    /**
     * Gets the framework state.
     *
     * @return Current framework state
     */
    virtual FrameworkState getState() const = 0;

    /**
     * Gets the framework context.
     *
     * @return Framework's module context
     */
    virtual IModuleContext* getContext() = 0;

    /**
     * Installs a module from the specified path.
     *
     * @param path Path to module file (.so, .dll, .dylib)
     * @return Installed module
     * @throws ModuleException if installation fails
     */
    virtual Module* installModule(const std::string& path) = 0;

    /**
     * Installs a module from a stream.
     *
     * @param stream Input stream containing module data
     * @param location Deployment location
     * @return Installed module
     * @throws ModuleException if installation fails
     */
    virtual Module* installModule(
        std::istream& stream,
        const std::string& location = ""
    ) = 0;

    /**
     * Updates an existing module.
     *
     * @param module Module to update
     * @param newPath Path to new module version
     * @throws ModuleException if update fails
     */
    virtual void updateModule(
        Module* module,
        const std::string& newPath
    ) = 0;

    /**
     * Uninstalls a module.
     *
     * @param module Module to uninstall
     * @throws ModuleException if uninstall fails
     */
    virtual void uninstallModule(Module* module) = 0;

    /**
     * Gets all installed modules.
     *
     * @return Vector of all modules
     */
    virtual std::vector<Module*> getModules() const = 0;

    /**
     * Gets a module by symbolic name.
     *
     * @param symbolicName Module's symbolic name
     * @param version Optional version (latest if not specified)
     * @return Module or nullptr if not found
     */
    virtual Module* getModule(
        const std::string& symbolicName,
        const Version& version = Version()
    ) const = 0;

    /**
     * Gets framework properties.
     *
     * @return Framework configuration properties
     */
    virtual const FrameworkProperties& getProperties() const = 0;

    /**
     * Adds a framework listener.
     *
     * @param listener Listener to add
     */
    virtual void addFrameworkListener(IFrameworkListener* listener) = 0;

    /**
     * Removes a framework listener.
     *
     * @param listener Listener to remove
     */
    virtual void removeFrameworkListener(IFrameworkListener* listener) = 0;
};

} // namespace cdmf
```

### FrameworkProperties

Configuration properties for framework creation.

```cpp
namespace cdmf {

struct FrameworkProperties {
    // Core settings
    std::string storage_path = "./storage";
    std::string module_path = "./modules";
    std::string cache_path = "./cache";

    // Module settings
    bool auto_install_modules = false;
    bool auto_start_modules = false;
    int module_start_timeout_ms = 5000;

    // IPC settings
    bool enable_ipc = false;
    std::string ipc_transport = "unix-socket";  // unix-socket, shared-memory, grpc
    std::string ipc_endpoint = "/tmp/cdmf";

    // Security settings
    bool enable_security = false;
    bool verify_signatures = false;
    std::string trust_store_path = "./trust";

    // Performance settings
    int event_thread_pool_size = 4;
    int service_cache_size = 100;
    bool enable_lazy_loading = true;

    // Logging settings
    std::string log_level = "INFO";  // ERROR, WARN, INFO, DEBUG, TRACE
    std::string log_file = "";       // Empty = stdout

    // Process management (for out-of-process modules)
    bool enable_process_manager = false;
    int max_process_restarts = 3;
    int process_restart_delay_ms = 1000;
};

} // namespace cdmf
```

## Module API

### Module

Represents an installed module in the framework.

```cpp
namespace cdmf {

class Module {
public:
    virtual ~Module() = default;

    /**
     * Gets the module's symbolic name.
     *
     * @return Symbolic name from manifest
     */
    virtual const std::string& getSymbolicName() const = 0;

    /**
     * Gets the module version.
     *
     * @return Module version
     */
    virtual const Version& getVersion() const = 0;

    /**
     * Gets the module state.
     *
     * @return Current module state
     */
    virtual ModuleState getState() const = 0;

    /**
     * Starts the module.
     *
     * @throws ModuleException if start fails
     */
    virtual void start() = 0;

    /**
     * Stops the module.
     *
     * @throws ModuleException if stop fails
     */
    virtual void stop() = 0;

    /**
     * Gets the module context.
     *
     * @return Module's context for framework interaction
     */
    virtual IModuleContext* getContext() = 0;

    /**
     * Gets the module manifest.
     *
     * @return Parsed manifest as JSON
     */
    virtual const nlohmann::json& getManifest() const = 0;

    /**
     * Gets module headers/API.
     *
     * @return Map of exported header files
     */
    virtual std::map<std::string, std::string> getHeaders() const = 0;

    /**
     * Gets a module resource.
     *
     * @param path Relative path to resource
     * @return Resource data or empty if not found
     */
    virtual std::vector<uint8_t> getResource(const std::string& path) const = 0;

    /**
     * Gets module location.
     *
     * @return Module deployment location
     */
    virtual ModuleLocation getLocation() const = 0;

    /**
     * Gets registered services from this module.
     *
     * @return Vector of service registrations
     */
    virtual std::vector<ServiceRegistration> getRegisteredServices() const = 0;

    /**
     * Gets module dependencies.
     *
     * @return Vector of required modules
     */
    virtual std::vector<Module*> getDependencies() const = 0;

    /**
     * Gets dependent modules.
     *
     * @return Vector of modules that depend on this one
     */
    virtual std::vector<Module*> getDependents() const = 0;

    /**
     * Adds a module listener.
     *
     * @param listener Listener to add
     */
    virtual void addModuleListener(IModuleListener* listener) = 0;

    /**
     * Removes a module listener.
     *
     * @param listener Listener to remove
     */
    virtual void removeModuleListener(IModuleListener* listener) = 0;
};

} // namespace cdmf
```

### IModuleActivator

Interface that all modules must implement.

```cpp
namespace cdmf {

/**
 * Module activator - entry point for module lifecycle.
 * Every module must provide an implementation and export factory functions.
 */
class IModuleActivator {
public:
    virtual ~IModuleActivator() = default;

    /**
     * Called when the module is starting.
     *
     * @param context Module's execution context
     * @throws std::exception if startup fails
     */
    virtual void start(IModuleContext* context) = 0;

    /**
     * Called when the module is stopping.
     *
     * @param context Module's execution context
     * @throws std::exception (logged but not propagated)
     */
    virtual void stop(IModuleContext* context) = 0;
};

} // namespace cdmf

// Required exports for every module
extern "C" {
    /**
     * Creates a module activator instance.
     *
     * @return New activator instance
     */
    CDMF_EXPORT cdmf::IModuleActivator* createModuleActivator();

    /**
     * Destroys a module activator instance.
     *
     * @param activator Activator to destroy
     */
    CDMF_EXPORT void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
```

## Service Registry API

### ServiceRegistry

Central registry for service registration and discovery.

```cpp
namespace cdmf {

class ServiceRegistry {
public:
    /**
     * Registers a service.
     *
     * @param interfaceName Service interface name
     * @param service Service implementation
     * @param owner Module that owns the service
     * @param properties Service properties
     * @return Service registration handle
     */
    template<typename T>
    ServiceRegistration registerService(
        const std::string& interfaceName,
        std::shared_ptr<T> service,
        Module* owner,
        const Properties& properties = Properties()
    );

    /**
     * Gets a service by interface name.
     * Returns highest ranking service if multiple exist.
     *
     * @param interfaceName Service interface name
     * @return Service implementation or nullptr
     */
    template<typename T>
    std::shared_ptr<T> getService(const std::string& interfaceName);

    /**
     * Gets service references matching a filter.
     *
     * @param interfaceName Service interface name
     * @param filter LDAP-style filter string
     * @return Vector of matching service references
     */
    template<typename T>
    std::vector<ServiceReference<T>> getServiceReferences(
        const std::string& interfaceName,
        const std::string& filter = ""
    );

    /**
     * Unregisters a service.
     *
     * @param interfaceName Service interface name
     * @param serviceId Service ID to unregister
     */
    void unregisterService(const std::string& interfaceName, long serviceId);

    /**
     * Updates service properties.
     *
     * @param interfaceName Service interface name
     * @param serviceId Service ID
     * @param properties New properties
     */
    void updateServiceProperties(
        const std::string& interfaceName,
        long serviceId,
        const Properties& properties
    );

    /**
     * Adds a service listener.
     *
     * @param listener Listener to add
     * @param filter Optional filter for events
     */
    void addServiceListener(
        IServiceListener* listener,
        const std::string& filter = ""
    );

    /**
     * Removes a service listener.
     *
     * @param listener Listener to remove
     */
    void removeServiceListener(IServiceListener* listener);
};

} // namespace cdmf
```

### ServiceRegistration

Handle for a registered service.

```cpp
namespace cdmf {

class ServiceRegistration {
public:
    /**
     * Unregisters the service.
     */
    void unregister();

    /**
     * Updates service properties.
     *
     * @param properties New properties
     */
    void setProperties(const Properties& properties);

    /**
     * Gets the service reference.
     *
     * @return Reference to this service
     */
    ServiceReference<void> getReference() const;

    /**
     * Checks if registration is valid.
     *
     * @return true if still registered
     */
    bool isValid() const;

    /**
     * Gets service ID.
     *
     * @return Unique service ID
     */
    long getServiceId() const;
};

} // namespace cdmf
```

### ServiceReference

Reference to a service in the registry.

```cpp
namespace cdmf {

template<typename T>
class ServiceReference {
public:
    /**
     * Gets the service implementation.
     *
     * @return Service or nullptr if unregistered
     */
    std::shared_ptr<T> getService() const;

    /**
     * Gets service properties.
     *
     * @return Current service properties
     */
    Properties getProperties() const;

    /**
     * Gets the service ID.
     *
     * @return Unique service ID
     */
    long getServiceId() const;

    /**
     * Gets the module that registered this service.
     *
     * @return Owner module
     */
    Module* getModule() const;

    /**
     * Checks if using module matches target module.
     *
     * @param module Module to check
     * @return true if module is using this service
     */
    bool isAssignableTo(Module* module) const;

    /**
     * Compares service references.
     *
     * @param other Other reference
     * @return Comparison result based on ranking and ID
     */
    int compareTo(const ServiceReference& other) const;
};

} // namespace cdmf
```

### ServiceTracker

Automatic service discovery and tracking.

```cpp
namespace cdmf {

template<typename T>
class ServiceTracker {
public:
    /**
     * Creates a service tracker.
     *
     * @param context Module context
     * @param interfaceName Service interface to track
     * @param filter Optional LDAP filter
     */
    ServiceTracker(
        IModuleContext* context,
        const std::string& interfaceName,
        const std::string& filter = ""
    );

    /**
     * Starts tracking services.
     */
    void open();

    /**
     * Stops tracking services.
     */
    void close();

    /**
     * Sets callback for service addition.
     *
     * @param callback Function called when service added
     */
    void onServiceAdded(
        std::function<void(std::shared_ptr<T>, const Properties&)> callback
    );

    /**
     * Sets callback for service modification.
     *
     * @param callback Function called when service modified
     */
    void onServiceModified(
        std::function<void(std::shared_ptr<T>, const Properties&)> callback
    );

    /**
     * Sets callback for service removal.
     *
     * @param callback Function called when service removed
     */
    void onServiceRemoved(
        std::function<void(std::shared_ptr<T>, const Properties&)> callback
    );

    /**
     * Gets a tracked service.
     *
     * @return Service or nullptr if none tracked
     */
    std::shared_ptr<T> getService() const;

    /**
     * Gets all tracked services.
     *
     * @return Vector of tracked services
     */
    std::vector<std::shared_ptr<T>> getServices() const;

    /**
     * Gets count of tracked services.
     *
     * @return Number of services being tracked
     */
    size_t size() const;

    /**
     * Checks if tracker is active.
     *
     * @return true if tracking
     */
    bool isTracking() const;
};

} // namespace cdmf
```

## Module Context API

### IModuleContext

Module's interface to the framework.

```cpp
namespace cdmf {

class IModuleContext {
public:
    virtual ~IModuleContext() = default;

    // Service operations

    /**
     * Registers a service.
     *
     * @param interfaceName Service interface name
     * @param service Service implementation
     * @param properties Service properties
     * @return Registration handle
     */
    template<typename T>
    ServiceRegistration registerService(
        const std::string& interfaceName,
        std::shared_ptr<T> service,
        const Properties& properties = Properties()
    );

    /**
     * Gets a service.
     *
     * @param interfaceName Service interface name
     * @return Service or nullptr
     */
    template<typename T>
    std::shared_ptr<T> getService(const std::string& interfaceName);

    /**
     * Gets service references.
     *
     * @param interfaceName Service interface name
     * @param filter Optional LDAP filter
     * @return Matching service references
     */
    template<typename T>
    std::vector<ServiceReference<T>> getServiceReferences(
        const std::string& interfaceName,
        const std::string& filter = ""
    );

    // Module operations

    /**
     * Gets this module.
     *
     * @return The module owning this context
     */
    virtual Module* getModule() = 0;

    /**
     * Gets a module by symbolic name.
     *
     * @param symbolicName Module name
     * @return Module or nullptr
     */
    virtual Module* getModule(const std::string& symbolicName) = 0;

    /**
     * Gets all modules.
     *
     * @return All installed modules
     */
    virtual std::vector<Module*> getModules() = 0;

    // Event operations

    /**
     * Adds a module listener.
     *
     * @param listener Listener to add
     */
    virtual void addModuleListener(IModuleListener* listener) = 0;

    /**
     * Removes a module listener.
     *
     * @param listener Listener to remove
     */
    virtual void removeModuleListener(IModuleListener* listener) = 0;

    /**
     * Adds a service listener.
     *
     * @param listener Listener to add
     * @param filter Optional filter
     */
    virtual void addServiceListener(
        IServiceListener* listener,
        const std::string& filter = ""
    ) = 0;

    /**
     * Removes a service listener.
     *
     * @param listener Listener to remove
     */
    virtual void removeServiceListener(IServiceListener* listener) = 0;

    /**
     * Adds a framework listener.
     *
     * @param listener Listener to add
     */
    virtual void addFrameworkListener(IFrameworkListener* listener) = 0;

    /**
     * Removes a framework listener.
     *
     * @param listener Listener to remove
     */
    virtual void removeFrameworkListener(IFrameworkListener* listener) = 0;

    // Resource operations

    /**
     * Gets a data file path.
     *
     * @param filename Relative filename
     * @return Absolute path to file in module's data area
     */
    virtual std::string getDataFile(const std::string& filename) = 0;

    /**
     * Gets module properties.
     *
     * @return Module configuration properties
     */
    virtual Properties getProperties() = 0;

    /**
     * Gets a property value.
     *
     * @param key Property key
     * @return Property value or empty if not found
     */
    virtual std::string getProperty(const std::string& key) = 0;
};

} // namespace cdmf
```

## Event System API

### Event Types

```cpp
namespace cdmf {

enum class ModuleEventType {
    INSTALLED,
    RESOLVED,
    STARTING,
    STARTED,
    STOPPING,
    STOPPED,
    UNRESOLVED,
    UNINSTALLED,
    UPDATED
};

enum class ServiceEventType {
    REGISTERED,
    MODIFIED,
    UNREGISTERING,
    UNREGISTERED
};

enum class FrameworkEventType {
    STARTED,
    ERROR,
    WARNING,
    INFO,
    STOPPED,
    STOPPED_UPDATE
};

} // namespace cdmf
```

### IModuleListener

Listener for module events.

```cpp
namespace cdmf {

class IModuleListener {
public:
    virtual ~IModuleListener() = default;

    /**
     * Called when a module event occurs.
     *
     * @param event The module event
     */
    virtual void moduleChanged(const ModuleEvent& event) = 0;
};

struct ModuleEvent {
    ModuleEventType type;
    Module* module;
    Module* origin;  // Module that caused the event
    std::chrono::system_clock::time_point timestamp;
};

} // namespace cdmf
```

### IServiceListener

Listener for service events.

```cpp
namespace cdmf {

class IServiceListener {
public:
    virtual ~IServiceListener() = default;

    /**
     * Called when a service event occurs.
     *
     * @param event The service event
     */
    virtual void serviceChanged(const ServiceEvent& event) = 0;
};

struct ServiceEvent {
    ServiceEventType type;
    ServiceReference<void> reference;
    std::chrono::system_clock::time_point timestamp;
};

} // namespace cdmf
```

### IFrameworkListener

Listener for framework events.

```cpp
namespace cdmf {

class IFrameworkListener {
public:
    virtual ~IFrameworkListener() = default;

    /**
     * Called when a framework event occurs.
     *
     * @param event The framework event
     */
    virtual void frameworkEvent(const FrameworkEvent& event) = 0;
};

struct FrameworkEvent {
    FrameworkEventType type;
    Module* module;  // Related module (if any)
    std::exception_ptr error;  // Error details (if any)
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

} // namespace cdmf
```

## Configuration API

### IConfigurationAdmin

Service for managing module configurations.

```cpp
namespace cdmf {

class IConfigurationAdmin {
public:
    virtual ~IConfigurationAdmin() = default;

    /**
     * Gets configuration for a PID.
     *
     * @param pid Persistent identifier
     * @return Configuration object
     */
    virtual Configuration* getConfiguration(const std::string& pid) = 0;

    /**
     * Creates a factory configuration.
     *
     * @param factoryPid Factory PID
     * @return New configuration instance
     */
    virtual Configuration* createFactoryConfiguration(
        const std::string& factoryPid
    ) = 0;

    /**
     * Lists all configurations.
     *
     * @param filter Optional LDAP filter
     * @return Matching configurations
     */
    virtual std::vector<Configuration*> listConfigurations(
        const std::string& filter = ""
    ) = 0;
};

class Configuration {
public:
    virtual ~Configuration() = default;

    /**
     * Gets the PID.
     *
     * @return Persistent identifier
     */
    virtual std::string getPid() const = 0;

    /**
     * Gets configuration properties.
     *
     * @return Current properties
     */
    virtual Properties getProperties() const = 0;

    /**
     * Updates configuration.
     *
     * @param properties New properties
     */
    virtual void update(const Properties& properties) = 0;

    /**
     * Deletes this configuration.
     */
    virtual void remove() = 0;
};

} // namespace cdmf
```

### IManagedService

Interface for modules that accept configuration.

```cpp
namespace cdmf {

class IManagedService {
public:
    virtual ~IManagedService() = default;

    /**
     * Called when configuration is updated.
     *
     * @param properties New configuration properties
     */
    virtual void updated(const Properties& properties) = 0;
};

class IManagedServiceFactory {
public:
    virtual ~IManagedServiceFactory() = default;

    /**
     * Called when configuration is updated.
     *
     * @param pid Configuration PID
     * @param properties New configuration properties
     */
    virtual void updated(
        const std::string& pid,
        const Properties& properties
    ) = 0;

    /**
     * Called when configuration is deleted.
     *
     * @param pid Configuration PID
     */
    virtual void deleted(const std::string& pid) = 0;
};

} // namespace cdmf
```

## Security API

### SecurityManager

Manages security policies and permissions.

```cpp
namespace cdmf {

class SecurityManager {
public:
    /**
     * Checks if module has permission.
     *
     * @param module Module to check
     * @param permission Permission required
     * @throws SecurityException if denied
     */
    void checkPermission(Module* module, const Permission& permission);

    /**
     * Applies sandbox to process.
     *
     * @param pid Process ID
     * @param policy Security policy
     */
    void applySandbox(pid_t pid, const SecurityPolicy& policy);

    /**
     * Verifies module signature.
     *
     * @param path Module file path
     * @return true if signature valid
     */
    bool verifySignature(const std::string& path);

    /**
     * Gets module permissions.
     *
     * @param module Module to query
     * @return Granted permissions
     */
    std::vector<Permission> getPermissions(Module* module);
};

struct Permission {
    std::string type;     // service, file, network
    std::string target;   // Specific resource
    std::string actions;  // read, write, execute
};

struct SecurityPolicy {
    bool allowNetwork = false;
    bool allowFileSystem = false;
    std::vector<std::string> allowedPaths;
    size_t maxMemoryMB = 512;
    int maxCpuPercent = 50;
    int maxThreads = 20;
    std::set<int> allowedSyscalls;
};

} // namespace cdmf
```

## Utility APIs

### Properties

Key-value property container.

```cpp
namespace cdmf {

class Properties {
public:
    /**
     * Sets a property.
     *
     * @param key Property key
     * @param value Property value
     */
    void set(const std::string& key, const std::string& value);

    /**
     * Gets a property.
     *
     * @param key Property key
     * @return Value or empty if not found
     */
    std::string get(const std::string& key) const;

    /**
     * Gets a property with default.
     *
     * @param key Property key
     * @param defaultValue Default if not found
     * @return Value or default
     */
    std::string get(
        const std::string& key,
        const std::string& defaultValue
    ) const;

    /**
     * Gets property as integer.
     *
     * @param key Property key
     * @return Integer value or empty
     */
    std::optional<int> getInt(const std::string& key) const;

    /**
     * Gets property as boolean.
     *
     * @param key Property key
     * @return Boolean value or empty
     */
    std::optional<bool> getBool(const std::string& key) const;

    /**
     * Checks if property exists.
     *
     * @param key Property key
     * @return true if exists
     */
    bool contains(const std::string& key) const;

    /**
     * Removes a property.
     *
     * @param key Property key
     */
    void remove(const std::string& key);

    /**
     * Clears all properties.
     */
    void clear();

    /**
     * Gets all keys.
     *
     * @return Vector of property keys
     */
    std::vector<std::string> keys() const;

    /**
     * Gets property count.
     *
     * @return Number of properties
     */
    size_t size() const;
};

} // namespace cdmf
```

### Version

Semantic version implementation.

```cpp
namespace cdmf {

class Version {
public:
    /**
     * Creates a version from string.
     *
     * @param versionString Version string (e.g., "1.2.3")
     */
    explicit Version(const std::string& versionString = "0.0.0");

    /**
     * Creates a version from components.
     *
     * @param major Major version
     * @param minor Minor version
     * @param patch Patch version
     * @param qualifier Optional qualifier
     */
    Version(int major, int minor, int patch, const std::string& qualifier = "");

    /**
     * Gets major version.
     */
    int getMajor() const;

    /**
     * Gets minor version.
     */
    int getMinor() const;

    /**
     * Gets patch version.
     */
    int getPatch() const;

    /**
     * Gets qualifier.
     */
    const std::string& getQualifier() const;

    /**
     * Converts to string.
     */
    std::string toString() const;

    /**
     * Compares versions.
     */
    int compareTo(const Version& other) const;

    /**
     * Checks compatibility (same major version).
     */
    bool isCompatible(const Version& other) const;

    // Comparison operators
    bool operator==(const Version& other) const;
    bool operator!=(const Version& other) const;
    bool operator<(const Version& other) const;
    bool operator<=(const Version& other) const;
    bool operator>(const Version& other) const;
    bool operator>=(const Version& other) const;
};

} // namespace cdmf
```

### VersionRange

Version range specification.

```cpp
namespace cdmf {

class VersionRange {
public:
    /**
     * Creates version range from string.
     *
     * @param rangeString Range string (e.g., "[1.0.0, 2.0.0)")
     */
    explicit VersionRange(const std::string& rangeString);

    /**
     * Creates version range.
     *
     * @param min Minimum version
     * @param max Maximum version
     * @param includeMin Include minimum
     * @param includeMax Include maximum
     */
    VersionRange(
        const Version& min,
        const Version& max,
        bool includeMin = true,
        bool includeMax = false
    );

    /**
     * Checks if version is in range.
     *
     * @param version Version to check
     * @return true if in range
     */
    bool includes(const Version& version) const;

    /**
     * Gets minimum version.
     */
    const Version& getMinimum() const;

    /**
     * Gets maximum version.
     */
    const Version& getMaximum() const;

    /**
     * Checks if minimum is inclusive.
     */
    bool isMinimumInclusive() const;

    /**
     * Checks if maximum is inclusive.
     */
    bool isMaximumInclusive() const;

    /**
     * Converts to string.
     */
    std::string toString() const;
};

} // namespace cdmf
```

## Constants and Enumerations

### Module States

```cpp
namespace cdmf {

enum class ModuleState {
    INSTALLED,    // Module installed but not resolved
    RESOLVED,     // Dependencies satisfied
    STARTING,     // Module is starting
    ACTIVE,       // Module is active
    STOPPING,     // Module is stopping
    UNINSTALLED   // Module has been uninstalled
};

} // namespace cdmf
```

### Module Location

```cpp
namespace cdmf {

enum class ModuleLocation {
    IN_PROCESS,        // Same process (default)
    OUT_OF_PROCESS,    // Different process, same machine
    REMOTE             // Different machine
};

enum class IsolationLevel {
    NONE,              // No isolation
    STANDARD,          // Process isolation
    SANDBOX            // Sandboxed with restrictions
};

} // namespace cdmf
```

### Framework States

```cpp
namespace cdmf {

enum class FrameworkState {
    CREATED,      // Framework created but not initialized
    STARTING,     // Framework is starting
    ACTIVE,       // Framework is active
    STOPPING,     // Framework is stopping
    STOPPED       // Framework has stopped
};

} // namespace cdmf
```

### Service Properties

```cpp
namespace cdmf {

// Standard service property keys
const std::string SERVICE_ID = "service.id";
const std::string SERVICE_PID = "service.pid";
const std::string SERVICE_RANKING = "service.ranking";
const std::string SERVICE_VENDOR = "service.vendor";
const std::string SERVICE_DESCRIPTION = "service.description";
const std::string SERVICE_LOCATION = "service.location";

} // namespace cdmf
```

## Error Handling

### Exception Types

```cpp
namespace cdmf {

/**
 * Base exception for all CDMF errors.
 */
class CDMFException : public std::runtime_error {
public:
    explicit CDMFException(const std::string& message);
};

/**
 * Framework-related errors.
 */
class FrameworkException : public CDMFException {
public:
    explicit FrameworkException(const std::string& message);
};

/**
 * Module-related errors.
 */
class ModuleException : public CDMFException {
public:
    explicit ModuleException(const std::string& message);
    Module* getModule() const;
};

/**
 * Service-related errors.
 */
class ServiceException : public CDMFException {
public:
    explicit ServiceException(const std::string& message);
};

/**
 * Security violations.
 */
class SecurityException : public CDMFException {
public:
    explicit SecurityException(const std::string& message);
    const Permission& getPermission() const;
};

/**
 * Configuration errors.
 */
class ConfigurationException : public CDMFException {
public:
    explicit ConfigurationException(const std::string& message);
};

} // namespace cdmf
```

### Error Codes

```cpp
namespace cdmf {

enum class ErrorCode {
    SUCCESS = 0,

    // Framework errors (1000-1999)
    FRAMEWORK_NOT_INITIALIZED = 1001,
    FRAMEWORK_ALREADY_STARTED = 1002,
    FRAMEWORK_NOT_ACTIVE = 1003,
    FRAMEWORK_SHUTDOWN_TIMEOUT = 1004,

    // Module errors (2000-2999)
    MODULE_NOT_FOUND = 2001,
    MODULE_ALREADY_INSTALLED = 2002,
    MODULE_DEPENDENCY_MISSING = 2003,
    MODULE_CIRCULAR_DEPENDENCY = 2004,
    MODULE_START_FAILED = 2005,
    MODULE_STOP_FAILED = 2006,
    MODULE_INVALID_MANIFEST = 2007,
    MODULE_INCOMPATIBLE_VERSION = 2008,

    // Service errors (3000-3999)
    SERVICE_NOT_FOUND = 3001,
    SERVICE_ALREADY_REGISTERED = 3002,
    SERVICE_TYPE_MISMATCH = 3003,
    SERVICE_UNAVAILABLE = 3004,

    // Security errors (4000-4999)
    PERMISSION_DENIED = 4001,
    INVALID_SIGNATURE = 4002,
    UNTRUSTED_SOURCE = 4003,

    // Configuration errors (5000-5999)
    CONFIG_NOT_FOUND = 5001,
    CONFIG_INVALID = 5002,
    CONFIG_LOCKED = 5003,

    // IPC errors (6000-6999)
    IPC_CONNECTION_FAILED = 6001,
    IPC_TIMEOUT = 6002,
    IPC_SERIALIZATION_ERROR = 6003,
    IPC_REMOTE_EXCEPTION = 6004
};

} // namespace cdmf
```

---

*Previous: [Architecture and Design](02-architecture-design.md)*
*Next: [Developer Guide →](04-developer-guide.md)*