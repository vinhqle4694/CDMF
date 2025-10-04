# CDMF Architecture and Design Guide

## Table of Contents
1. [Architectural Overview](#architectural-overview)
2. [Core Design Principles](#core-design-principles)
3. [System Architecture](#system-architecture)
4. [Component Architecture](#component-architecture)
5. [Communication Patterns](#communication-patterns)
6. [Module System Design](#module-system-design)
7. [Service Registry Architecture](#service-registry-architecture)
8. [Dependency Management](#dependency-management)
9. [Event System Architecture](#event-system-architecture)
10. [Security Architecture](#security-architecture)
11. [Design Patterns](#design-patterns)
12. [Architecture Decision Records](#architecture-decision-records)

## Architectural Overview

CDMF employs a layered, modular architecture that separates concerns across multiple abstraction levels. The framework is designed to provide maximum flexibility while maintaining high performance for system-level C++ applications.

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│         User Modules | Business Logic | Extensions          │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                    Framework Services                       │
│   Configuration | Logging | Security | Event Management     │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                      Service Layer                          │
│    Service Registry | Service Tracker | Service Proxy       │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                   Module Management Layer                   │
│    Lifecycle | Dependencies | Versioning | Isolation        │
└─────────────────────────────┬───────────────────────────────┘
                              │
┌─────────────────────────────▼───────────────────────────────┐
│                      Framework Core                         │
│   Dynamic Loading | IPC Transport | Platform Abstraction    │
└─────────────────────────────────────────────────────────────┘
```

## Core Design Principles

### 1. Modularity First
Every component is a module with clear boundaries, explicit dependencies, and well-defined interfaces. This enables:
- Independent development and testing
- Parallel compilation
- Clear ownership boundaries
- Reduced coupling

### 2. Interface-Based Programming
All module interactions occur through abstract interfaces, never concrete implementations:
```cpp
// Good: Interface-based
class ILogger {
public:
    virtual void log(const std::string& msg) = 0;
};

// Module uses interface
auto logger = context->getService<ILogger>();
```

### 3. Dynamic Discovery
Services are discovered at runtime through the service registry:
- No compile-time dependencies between modules
- Services can appear and disappear dynamically
- Multiple implementations of the same interface

### 4. Version Awareness
The framework explicitly manages versions:
- Semantic versioning (MAJOR.MINOR.PATCH)
- Multiple versions can coexist
- Version ranges for flexible dependency specification
- Automatic compatibility checking

### 5. Location Transparency
Services work identically regardless of location:
- In-process (direct function calls)
- Out-of-process (IPC on same machine)
- Remote (network IPC)

### 6. Fail-Safe Design
The framework handles failures gracefully:
- Module crashes don't affect the framework (with IPC)
- Failed dependencies prevent module start
- Automatic cleanup on failure
- Comprehensive error reporting

## System Architecture

### Deployment Architectures

CDMF supports three deployment models, each with different trade-offs:

#### 1. In-Process Architecture
All modules run in the same process space:

```
┌───────────────── Process: Application ──────────────────────┐
│                                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
│  │ Module A │  │ Module B │  │ Module C │  │ Module D │     │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘     │
│       │             │             │             │           │
│       └─────────────┴─────────────┴─────────────┘           │
│                     │                                       │
│       ┌─────────────▼──────────────┐                        │
│       │   Service Registry         │                        │
│       │   (in-memory hashmap)      │                        │
│       └─────────────┬──────────────┘                        │
│                     │                                       │
│       ┌─────────────▼──────────────┐                        │
│       │  Framework Core            │                        │
│       │  (dlopen/LoadLibrary)      │                        │
│       └────────────────────────────┘                        │
│                                                             │
│  Performance: Ultra-fast (5ns latency)                      │
│  Isolation: None                                            │
│  Use Case: Trusted modules, maximum performance             │
└─────────────────────────────────────────────────────────────┘
```

**Characteristics:**
- ⚡ Nanosecond latency (direct virtual function calls)
- 💾 Shared memory space (efficient)
- 🔧 Simple debugging
- ⚠️ No fault isolation

#### 2. Out-of-Process Architecture
Modules run in separate processes on the same machine:

```
┌──────────────────── Main Process ──────────────────────────┐
│                                                            │
│  ┌──────────┐  ┌──────────┐                                │
│  │ Module A │  │ Module B │  (Core/Trusted)                │
│  └────┬─────┘  └────┬─────┘                                │
│       └─────────────┘                                      │
│             │                                              │
│  ┌──────────▼──────────────┐                               │
│  │  Service Registry       │                               │
│  └──────────┬──────────────┘                               │
│             │                                              │
│  ┌──────────▼──────────────┐                               │
│  │  IPC Proxy Manager      │                               │
│  └──────────┬──────────────┘                               │
│             │                                              │
└─────────────┼──────────────────────────────────────────────┘
              │ Unix Socket / Shared Memory
              │
┌─────────────▼───────────────────────────────────────────────┐
│              Isolated Process                               │
│                                                             │
│  ┌──────────┐                                               │
│  │ Module C │  (Untrusted/Third-party)                      │
│  └────┬─────┘                                               │
│       │                                                     │
│  ┌────▼─────────────────────┐                               │
│  │  IPC Stub                │                               │
│  └──────────────────────────┘                               │
│                                                             │
│  Sandboxed with resource limits                             │
└─────────────────────────────────────────────────────────────┘
```

**Characteristics:**
- 🛡️ Process-level isolation
- ⚡ Microsecond latency (50μs typical)
- 🔒 Security sandboxing available
- 📊 Resource control (CPU, memory limits)

#### 3. Remote Architecture
Modules run on different machines:

```
┌──────────── Machine A: Main Application ────────────────────┐
│                                                             │
│  ┌──────────────────────────┐                               │
│  │    Framework Core        │                               │
│  └──────────┬───────────────┘                               │
│             │                                               │
│  ┌──────────▼──────────────┐                                │
│  │  gRPC Proxy Manager     │                                │
│  └──────────┬──────────────┘                                │
│             │                                               │
└─────────────┼───────────────────────────────────────────────┘
              │ Network (gRPC/HTTP2)
              │
┌─────────────▼───────────────────────────────────────────────┐
│         Machine B: Remote Service                           │
│                                                             │
│  ┌──────────┐                                               │
│  │ Module E │  (GPU Processing, ML Inference, etc.)         │
│  └──────────┘                                               │
│                                                             │
│  Specialized hardware or scaling                            │
└─────────────────────────────────────────────────────────────┘
```

**Characteristics:**
- 🌍 Network distribution
- ⏱️ Millisecond latency (5ms typical)
- 🚀 Horizontal scaling
- 🖥️ Specialized hardware access

### Hybrid Architecture

Most production deployments use a hybrid approach:

```
Main Process
├── Core Modules (in-process)
│   ├── Logger
│   ├── Configuration
│   └── Core Business Logic
│
├── Trusted Extensions (in-process)
│   ├── Database Connector
│   └── Cache Module
│
└── IPC Proxies
    ├── Untrusted Plugin (separate process)
    └── Remote GPU Service (network)
```

## Component Architecture

### Framework Core Components

#### 1. Framework Manager
Central coordinator for all framework operations:

```cpp
class Framework {
    ModuleRegistry moduleRegistry_;
    ServiceRegistry serviceRegistry_;
    EventDispatcher eventDispatcher_;
    DependencyResolver dependencyResolver_;
    ProcessManager processManager_;  // For out-of-process modules

public:
    void start();
    void stop();
    Module* installModule(const std::string& path);
    void uninstallModule(Module* module);
    void updateModule(Module* module, const std::string& newPath);
};
```

#### 2. Module Registry
Manages all installed modules:

```cpp
class ModuleRegistry {
    std::map<std::string, std::unique_ptr<Module>> modules_;
    std::map<std::string, std::vector<Module*>> versionMap_;

public:
    Module* install(const std::string& path);
    void uninstall(const std::string& symbolicName);
    Module* getModule(const std::string& name, const Version& version);
    std::vector<Module*> getModules();
};
```

#### 3. Service Registry
Central service directory (detailed in next section).

#### 4. Dependency Resolver
Ensures all module dependencies are satisfied:

```cpp
class DependencyResolver {
    struct DependencyGraph {
        std::map<Module*, std::set<Module*>> dependencies;
        std::map<Module*, std::set<Module*>> dependents;
    };

public:
    bool resolve(Module* module);
    std::vector<Module*> getStartOrder(const std::vector<Module*>& modules);
    std::vector<Module*> getStopOrder(const std::vector<Module*>& modules);
};
```

#### 5. Event Dispatcher
Asynchronous event distribution:

```cpp
class EventDispatcher {
    std::vector<IEventListener*> listeners_;
    ThreadPool workerPool_;

public:
    void fireEvent(const Event& event);
    void addEventListener(IEventListener* listener, const EventFilter& filter);
    void removeEventListener(IEventListener* listener);
};
```

## Communication Patterns

### Service Communication

#### Direct In-Process Communication
```cpp
// Caller
auto service = context->getService<ILogger>();
service->log("Message");  // Direct virtual function call
// Latency: ~5 nanoseconds
```

#### IPC via Proxy Pattern
```cpp
// Caller (same API!)
auto service = context->getService<ILogger>();
service->log("Message");  // Proxy intercepts, marshals, sends via IPC

// Behind the scenes:
class LoggerProxy : public ILogger {
    IPCChannel channel_;

    void log(const std::string& msg) override {
        Message request;
        request.method = "log";
        request.args = serialize(msg);
        channel_.send(request);
        // Wait for response if needed
    }
};
```

#### Remote Communication
```cpp
// Still the same API!
auto service = context->getService<IMLService>();
auto result = service->predict(data);  // gRPC call to remote server

// Implementation uses gRPC
class MLServiceProxy : public IMLService {
    grpc::Channel channel_;

    PredictionResult predict(const Data& data) override {
        MLServiceRequest request;
        MLServiceResponse response;
        // ... gRPC call ...
        return response.result();
    }
};
```

### Event Communication

Events provide loose coupling between modules:

```cpp
// Publisher
context->fireEvent(ModuleEvent{
    .type = ModuleEvent::STARTED,
    .module = this,
    .timestamp = now()
});

// Subscriber
class MyListener : public IModuleListener {
    void moduleChanged(const ModuleEvent& event) override {
        if (event.type == ModuleEvent::STARTED) {
            // React to module start
        }
    }
};

context->addEventListener(new MyListener(),
                         EventFilter("(event.type=STARTED)"));
```

## Module System Design

### Module Structure

Each module consists of:

1. **Manifest** (manifest.json): Metadata and dependencies
2. **Shared Library** (.so/.dll/.dylib): Compiled code
3. **Headers** (optional): Public API definitions
4. **Resources** (optional): Configuration, assets

### Module Lifecycle State Machine

```
        ┌──────────┐
        │INSTALLED │ ──── install() ───→
        └────┬─────┘
             │ resolve()
             ▼
        ┌──────────┐
        │ RESOLVED │ ──── dependencies ok ───→
        └────┬─────┘
             │ start()
             ▼
        ┌──────────┐
        │ STARTING │ ──── activator.start() ───→
        └────┬─────┘
             │ success
             ▼
        ┌──────────┐
        │  ACTIVE  │ ◄──── fully operational
        └────┬─────┘
             │ stop()
             ▼
        ┌──────────┐
        │ STOPPING │ ──── activator.stop() ───→
        └────┬─────┘
             │ complete
             ▼
        ┌──────────┐
     ◄──│UNINSTALLED│ ──── uninstall() ───→
        └──────────┘
```

### Module Context

Each module receives a context for framework interaction:

```cpp
class IModuleContext {
public:
    // Service operations
    template<typename T>
    std::shared_ptr<T> getService(const std::string& name);

    template<typename T>
    ServiceRegistration registerService(const std::string& name,
                                       std::shared_ptr<T> service,
                                       const Properties& props);

    // Module operations
    Module* getModule();
    std::vector<Module*> getModules();

    // Event operations
    void addEventListener(IEventListener* listener);
    void removeEventListener(IEventListener* listener);

    // Resource access
    std::string getDataFile(const std::string& name);
    Properties getProperties();
};
```

## Service Registry Architecture

### Registry Design

The service registry is a thread-safe, high-performance directory:

```cpp
class ServiceRegistry {
private:
    // Service name → list of implementations
    std::map<std::string, std::vector<ServiceRecord>> services_;

    // Service ID → service record (fast lookup)
    std::map<long, ServiceRecord*> idMap_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Service listeners
    std::vector<IServiceListener*> listeners_;

    // ID generation
    std::atomic<long> nextServiceId_{1};
};
```

### Service Record Structure

```cpp
struct ServiceRecord {
    std::any service;           // Type-erased service instance
    Properties properties;      // Metadata
    Module* ownerModule;        // Publishing module
    long serviceId;            // Unique identifier
    ServiceLocation location;   // IN_PROCESS, OUT_OF_PROCESS, REMOTE
    std::string ipcEndpoint;   // For remote services
    int ranking;               // Selection priority
    std::chrono::time_point registeredTime;
};
```

### Service Selection Algorithm

When multiple services implement the same interface:

1. Filter by properties (if filter specified)
2. Sort by ranking (highest first)
3. Sort by registration time (newest first for same ranking)
4. Return first match

```cpp
auto service = context->getService<ILogger>("(type=file)");
// Gets highest-ranking file logger
```

### Service Tracking

Automatic service discovery and monitoring:

```cpp
ServiceTracker<IDatabase> tracker(context, "com.example.IDatabase");

tracker.onServiceAdded([](auto service, auto props) {
    // New database service available
});

tracker.onServiceRemoved([](auto service, auto props) {
    // Database service gone - switch to backup
});

tracker.open();  // Start tracking
```

## Dependency Management

### Dependency Types

1. **Module Dependencies**: Require another module
   ```json
   "require-modules": [{
       "symbolic-name": "com.example.logger",
       "version-range": "[1.0.0, 2.0.0)"
   }]
   ```

2. **Package Dependencies**: Import API packages
   ```json
   "import-packages": [{
       "package": "com.example.api",
       "version-range": "[1.0.0, 1.5.0]"
   }]
   ```

3. **Service Dependencies**: Require service availability
   ```json
   "import-services": [{
       "interface": "com.example.ILogger",
       "cardinality": "mandatory"
   }]
   ```

### Resolution Algorithm

```
1. Parse manifest dependencies
2. Build dependency graph
3. Check for cycles (fail if found)
4. Topological sort for start order
5. Verify all dependencies available
6. Check version compatibility
7. Resolve package imports
8. Mark module as RESOLVED
```

### Version Compatibility

Semantic versioning rules:
- MAJOR: Breaking changes (incompatible)
- MINOR: New features (backward compatible)
- PATCH: Bug fixes (backward compatible)

```cpp
Version v1("1.2.3");
Version v2("1.3.0");
assert(v1.isCompatible(v2));  // true (same MAJOR)

Version v3("2.0.0");
assert(v1.isCompatible(v3));  // false (different MAJOR)
```

## Event System Architecture

### Event Types

```cpp
enum class EventType {
    // Module events
    MODULE_INSTALLED,
    MODULE_RESOLVED,
    MODULE_STARTING,
    MODULE_STARTED,
    MODULE_STOPPING,
    MODULE_STOPPED,
    MODULE_UNINSTALLED,

    // Service events
    SERVICE_REGISTERED,
    SERVICE_MODIFIED,
    SERVICE_UNREGISTERING,

    // Framework events
    FRAMEWORK_STARTED,
    FRAMEWORK_STOPPING
};
```

### Event Dispatcher Design

```cpp
class EventDispatcher {
    struct ListenerEntry {
        IEventListener* listener;
        EventFilter filter;
        std::thread::id threadId;  // For thread affinity
    };

    std::vector<ListenerEntry> listeners_;
    ThreadPool asyncPool_;

    void dispatch(const Event& event) {
        for (const auto& entry : listeners_) {
            if (entry.filter.matches(event)) {
                if (entry.threadId == std::this_thread::get_id()) {
                    // Synchronous delivery (same thread)
                    entry.listener->handleEvent(event);
                } else {
                    // Asynchronous delivery
                    asyncPool_.enqueue([=] {
                        entry.listener->handleEvent(event);
                    });
                }
            }
        }
    }
};
```

## Security Architecture

### Security Layers

1. **Code Verification**
   - Digital signatures on modules
   - Certificate validation
   - Integrity checking

2. **Permission System**
   ```cpp
   class Permission {
       std::string type;     // "service", "file", "network"
       std::string target;   // Specific resource
       std::string actions;  // "read", "write", "execute"
   };
   ```

3. **Process Isolation**
   - Separate process per untrusted module
   - Seccomp-BPF system call filtering
   - AppArmor/SELinux profiles
   - Resource limits (cgroups)

4. **Sandboxing**
   ```cpp
   struct SecurityPolicy {
       bool allowNetwork = false;
       bool allowFileSystem = false;
       std::vector<std::string> allowedPaths;
       size_t maxMemoryMB = 512;
       int maxCpuPercent = 50;
       std::set<int> allowedSyscalls;
   };
   ```

## Design Patterns

### 1. Service Locator Pattern
Central registry for service discovery:
```cpp
auto logger = ServiceLocator::get<ILogger>();
```

### 2. Proxy Pattern
Transparent IPC through proxy objects:
```cpp
class ServiceProxy : public IService {
    // Marshals calls over IPC
};
```

### 3. Observer Pattern
Event system for loose coupling:
```cpp
class Observer : public IEventListener {
    void onEvent(const Event& e) override;
};
```

### 4. Factory Pattern
Module creation through factories:
```cpp
extern "C" IModuleActivator* createModuleActivator();
```

### 5. Strategy Pattern
Multiple service implementations:
```cpp
// Different logging strategies
ConsoleLogger : ILogger
FileLogger : ILogger
SyslogLogger : ILogger
```

### 6. Facade Pattern
Module context as framework facade:
```cpp
class ModuleContext {
    // Simplified framework interface
};
```

## Architecture Decision Records

### ADR-001: Dynamic Loading Mechanism
**Decision**: Use dlopen/LoadLibrary for dynamic loading
**Rationale**: Standard, portable, well-understood
**Alternatives**: Custom loader (rejected - complexity)

### ADR-002: Service Discovery
**Decision**: Runtime registry with type erasure
**Rationale**: Flexibility, no compile-time coupling
**Alternatives**: Static registration (rejected - inflexible)

### ADR-003: IPC Mechanism
**Decision**: Pluggable transport (Unix sockets default)
**Rationale**: Performance on Linux, extensibility
**Alternatives**: Only gRPC (rejected - overhead)

### ADR-004: API Discovery
**Decision**: Multiple approaches (headers, C-API, bundles)
**Rationale**: Different use cases need different solutions
**Alternatives**: Single approach (rejected - too limiting)

### ADR-005: Version Management
**Decision**: Semantic versioning
**Rationale**: Industry standard, clear compatibility rules
**Alternatives**: Custom scheme (rejected - non-standard)

### ADR-006: Threading Model
**Decision**: Thread pool for events, module threads independent
**Rationale**: Balance between performance and isolation
**Alternatives**: Single thread (rejected - performance)

### ADR-007: Configuration Storage
**Decision**: JSON for manifests and configuration
**Rationale**: Human readable, standard tooling
**Alternatives**: Binary format (rejected - debugging difficulty)

---

*Previous: [Overview and Introduction](01-overview-introduction.md)*
*Next: [API Reference →](03-api-reference.md)*