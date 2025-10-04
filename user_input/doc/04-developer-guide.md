# CDMF Developer Guide

## Table of Contents
1. [Getting Started](#getting-started)
2. [Creating Your First Module](#creating-your-first-module)
3. [Service Development](#service-development)
4. [API Discovery Strategies](#api-discovery-strategies)
5. [Dependency Management](#dependency-management)
6. [Event-Driven Programming](#event-driven-programming)
7. [Configuration Management](#configuration-management)
8. [Testing Modules](#testing-modules)
9. [Debugging and Troubleshooting](#debugging-and-troubleshooting)
10. [Best Practices](#best-practices)
11. [Common Patterns](#common-patterns)
12. [Examples](#examples)

## Getting Started

### Prerequisites

Before developing CDMF modules, ensure you have:

- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.15 or higher
- CDMF Framework SDK installed
- JSON library (nlohmann/json)
- Development tools (debugger, profiler)

### Setting Up Your Development Environment

#### 1. Install CDMF SDK

```bash
# Ubuntu/Debian
sudo apt-get install cdmf-dev

# macOS
brew install cdmf

# From source
git clone https://github.com/cdmf/cdmf.git
cd cdmf
mkdir build && cd build
cmake ..
make && sudo make install
```

#### 2. Project Structure

Organize your module project as follows:

```
my-module/
├── CMakeLists.txt           # Build configuration
├── manifest.json            # Module metadata
├── include/                 # Public headers
│   └── IMyService.h        # Service interface
├── src/                     # Implementation
│   ├── MyModuleActivator.cpp
│   └── MyServiceImpl.cpp
├── resources/               # Module resources
│   └── config.json
├── tests/                   # Unit tests
│   └── MyServiceTest.cpp
└── docs/                    # Documentation
    └── README.md
```

#### 3. CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_module VERSION 1.0.0)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find CDMF
find_package(CDMF REQUIRED)

# Create module library
add_library(my_module SHARED
    src/MyModuleActivator.cpp
    src/MyServiceImpl.cpp
)

# Include directories
target_include_directories(my_module
    PUBLIC include
    PRIVATE src
)

# Link CDMF
target_link_libraries(my_module
    PRIVATE CDMF::cdmf
)

# Set module properties
set_target_properties(my_module PROPERTIES
    PREFIX ""  # No 'lib' prefix
    OUTPUT_NAME "my-module"
)

# Install
install(TARGETS my_module DESTINATION modules)
install(FILES manifest.json DESTINATION modules)
install(DIRECTORY include/ DESTINATION include/my-module)
```

## Creating Your First Module

### Step 1: Define Your Service Interface

Create a pure virtual interface that defines your service contract:

```cpp
// include/IGreetingService.h
#ifndef IGREETING_SERVICE_H
#define IGREETING_SERVICE_H

#include <string>

class IGreetingService {
public:
    virtual ~IGreetingService() = default;

    /**
     * Gets a greeting message.
     * @param name Name to greet
     * @return Greeting message
     */
    virtual std::string greet(const std::string& name) const = 0;

    /**
     * Sets the greeting format.
     * @param format Format string with %s for name placeholder
     */
    virtual void setFormat(const std::string& format) = 0;
};

#endif // IGREETING_SERVICE_H
```

### Step 2: Implement the Service

```cpp
// src/GreetingServiceImpl.cpp
#include "IGreetingService.h"
#include <mutex>
#include <fmt/format.h>

class GreetingServiceImpl : public IGreetingService {
private:
    mutable std::mutex mutex_;
    std::string format_ = "Hello, {}!";

public:
    std::string greet(const std::string& name) const override {
        std::lock_guard lock(mutex_);
        return fmt::format(format_, name);
    }

    void setFormat(const std::string& format) override {
        std::lock_guard lock(mutex_);
        format_ = format;
    }
};
```

### Step 3: Create Module Activator

The activator is your module's entry point:

```cpp
// src/GreetingModuleActivator.cpp
#include <cdmf/IModuleActivator.h>
#include <cdmf/IModuleContext.h>
#include "GreetingServiceImpl.h"
#include <memory>
#include <iostream>

class GreetingModuleActivator : public cdmf::IModuleActivator {
private:
    cdmf::ServiceRegistration serviceReg_;
    std::shared_ptr<GreetingServiceImpl> service_;

public:
    void start(cdmf::IModuleContext* context) override {
        std::cout << "Greeting Module starting..." << std::endl;

        // Create service instance
        service_ = std::make_shared<GreetingServiceImpl>();

        // Set service properties
        cdmf::Properties props;
        props.set("service.vendor", "Example Corp");
        props.set("service.ranking", "100");
        props.set("language", "en");
        props.set("version", "1.0.0");

        // Register service
        serviceReg_ = context->registerService<IGreetingService>(
            "com.example.IGreetingService",
            service_,
            props
        );

        std::cout << "Greeting Module started successfully" << std::endl;
    }

    void stop(cdmf::IModuleContext* context) override {
        std::cout << "Greeting Module stopping..." << std::endl;

        // Service automatically unregistered
        service_.reset();

        std::cout << "Greeting Module stopped" << std::endl;
    }
};

// Export factory functions
extern "C" {
    CDMF_EXPORT cdmf::IModuleActivator* createModuleActivator() {
        return new GreetingModuleActivator();
    }

    CDMF_EXPORT void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
```

### Step 4: Create Module Manifest

```json
{
  "module": {
    "symbolic-name": "com.example.greeting",
    "version": "1.0.0",
    "name": "Greeting Module",
    "description": "Provides greeting services",
    "vendor": "Example Corp",
    "activator": "GreetingModuleActivator",
    "category": "services"
  },
  "exports": {
    "services": [
      {
        "interface": "com.example.IGreetingService",
        "version": "1.0.0"
      }
    ]
  },
  "requirements": [
    "cpp.standard:17"
  ]
}
```

### Step 5: Build and Install

```bash
# Build
mkdir build && cd build
cmake ..
make

# Install to framework
cdmf-cli install greeting-module.so
```

## Service Development

### Service Registration

Services are registered with the framework to make them discoverable:

```cpp
// Basic registration
auto registration = context->registerService<IMyService>(
    "com.example.IMyService",
    serviceImpl
);

// With properties
cdmf::Properties props;
props.set("service.ranking", "100");  // Higher ranking = preferred
props.set("type", "premium");
props.set("max.connections", "100");

auto registration = context->registerService<IMyService>(
    "com.example.IMyService",
    serviceImpl,
    props
);
```

### Service Discovery

#### Direct Lookup

```cpp
// Get highest-ranking service
auto service = context->getService<ILogger>("com.example.ILogger");
if (service) {
    service->log("Service found!");
} else {
    std::cerr << "Logger service not available" << std::endl;
}
```

#### Filtered Lookup

```cpp
// Get service with specific properties
auto services = context->getServiceReferences<IDatabase>(
    "com.example.IDatabase",
    "(type=postgresql)"  // LDAP-style filter
);

for (const auto& ref : services) {
    auto db = ref.getService();
    if (db) {
        // Use database service
    }
}
```

#### Service Tracking

Automatically track service availability:

```cpp
class MyModule {
private:
    cdmf::ServiceTracker<ICache> cacheTracker_;
    std::shared_ptr<ICache> currentCache_;

public:
    MyModule(cdmf::IModuleContext* context)
        : cacheTracker_(context, "com.example.ICache") {

        // Setup callbacks
        cacheTracker_.onServiceAdded([this](auto cache, auto props) {
            std::cout << "Cache service available" << std::endl;
            currentCache_ = cache;
        });

        cacheTracker_.onServiceRemoved([this](auto cache, auto props) {
            std::cout << "Cache service removed" << std::endl;
            if (currentCache_ == cache) {
                currentCache_ = nullptr;
            }
        });

        // Start tracking
        cacheTracker_.open();
    }

    void doWork() {
        if (currentCache_) {
            currentCache_->put("key", "value");
        } else {
            // Fallback when cache not available
        }
    }
};
```

### Service Factories

For creating multiple service instances:

```cpp
class ConnectionFactory : public IConnectionFactory {
private:
    int nextId_ = 1;

public:
    std::shared_ptr<IConnection> createConnection(
        const std::string& url) override {

        auto conn = std::make_shared<ConnectionImpl>(url, nextId_++);
        return conn;
    }
};

// Register factory
context->registerService<IConnectionFactory>(
    "com.example.IConnectionFactory",
    std::make_shared<ConnectionFactory>()
);

// Use factory
auto factory = context->getService<IConnectionFactory>(
    "com.example.IConnectionFactory"
);
auto conn = factory->createConnection("tcp://localhost:5432");
```

## API Discovery Strategies

### Strategy 1: Shared Headers (Recommended)

Most straightforward approach for C++ modules:

#### Provider Module

```
provider-module/
├── include/
│   └── IDataProcessor.h    # Public API
├── src/
│   └── DataProcessorImpl.cpp
└── manifest.json
```

```json
// manifest.json
{
  "exports": {
    "packages": [{
      "package": "com.example.processor.api",
      "version": "1.0.0"
    }]
  }
}
```

#### Consumer Module

```cpp
// Build-time: Add provider's include path
target_include_directories(consumer_module
    PRIVATE ${PROVIDER_MODULE_INCLUDE}
)

// Runtime: Discover service
#include <IDataProcessor.h>

auto processor = context->getService<IDataProcessor>(
    "com.example.IDataProcessor"
);
```

### Strategy 2: C-Style ABI Stable Interface

For maximum compatibility across compilers:

```cpp
// data_processor_api.h
#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef void* DataProcessorHandle;

// C-style API
DataProcessorHandle data_processor_create(void);
void data_processor_destroy(DataProcessorHandle handle);
int data_processor_process(DataProcessorHandle handle,
                          const void* input,
                          size_t input_size,
                          void** output,
                          size_t* output_size);

#ifdef __cplusplus
}
#endif
```

Implementation:

```cpp
// Provider implementation
extern "C" {
    DataProcessorHandle data_processor_create() {
        return new DataProcessorImpl();
    }

    void data_processor_destroy(DataProcessorHandle handle) {
        delete static_cast<DataProcessorImpl*>(handle);
    }

    int data_processor_process(DataProcessorHandle handle,
                              const void* input,
                              size_t input_size,
                              void** output,
                              size_t* output_size) {
        auto impl = static_cast<DataProcessorImpl*>(handle);
        return impl->process(input, input_size, output, output_size);
    }
}
```

### Strategy 3: Header-Only API Bundles

For versioned API management:

```
api-bundle/
├── include/
│   └── v1/
│       └── IService.h
│   └── v2/
│       └── IService.h
└── manifest.json
```

```json
{
  "module": {
    "symbolic-name": "com.example.api",
    "version": "2.0.0",
    "type": "api-bundle"
  },
  "exports": {
    "packages": [
      {
        "package": "com.example.api.v1",
        "version": "1.0.0"
      },
      {
        "package": "com.example.api.v2",
        "version": "2.0.0"
      }
    ]
  }
}
```

## Dependency Management

### Declaring Dependencies

In your manifest.json:

```json
{
  "dependencies": {
    "require-modules": [
      {
        "symbolic-name": "com.example.logger",
        "version-range": "[1.0.0, 2.0.0)",
        "optional": false
      }
    ],
    "import-packages": [
      {
        "package": "com.example.common",
        "version-range": "[1.0.0, 1.5.0]"
      }
    ],
    "import-services": [
      {
        "interface": "com.example.IDatabase",
        "filter": "(type=sql)",
        "cardinality": "1..n"  // At least one required
      }
    ]
  }
}
```

### Version Ranges

- `1.0.0` - Exact version
- `[1.0.0, 2.0.0)` - 1.0.0 ≤ version < 2.0.0
- `[1.0.0, 2.0.0]` - 1.0.0 ≤ version ≤ 2.0.0
- `(1.0.0, 2.0.0)` - 1.0.0 < version < 2.0.0

### Optional Dependencies

Handle optional dependencies gracefully:

```cpp
void start(cdmf::IModuleContext* context) override {
    // Required service
    auto logger = context->getService<ILogger>("com.example.ILogger");
    if (!logger) {
        throw std::runtime_error("Required logger service not found");
    }

    // Optional service
    auto cache = context->getService<ICache>("com.example.ICache");
    if (cache) {
        std::cout << "Cache service available, enabling caching" << std::endl;
        enableCaching(cache);
    } else {
        std::cout << "Cache service not available, running without cache" << std::endl;
    }
}
```

### Circular Dependencies

Avoid circular dependencies through:

1. **Interface segregation**: Split interfaces into smaller pieces
2. **Event-based communication**: Use events instead of direct dependencies
3. **Service locator pattern**: Lookup services dynamically

## Event-Driven Programming

### Listening for Module Events

```cpp
class ModuleMonitor : public cdmf::IModuleListener {
public:
    void moduleChanged(const cdmf::ModuleEvent& event) override {
        switch (event.type) {
            case cdmf::ModuleEventType::STARTED:
                std::cout << "Module started: "
                         << event.module->getSymbolicName() << std::endl;
                break;
            case cdmf::ModuleEventType::STOPPED:
                std::cout << "Module stopped: "
                         << event.module->getSymbolicName() << std::endl;
                break;
            default:
                break;
        }
    }
};

// Register listener
auto monitor = std::make_unique<ModuleMonitor>();
context->addModuleListener(monitor.get());
```

### Listening for Service Events

```cpp
class ServiceMonitor : public cdmf::IServiceListener {
public:
    void serviceChanged(const cdmf::ServiceEvent& event) override {
        auto props = event.reference.getProperties();

        switch (event.type) {
            case cdmf::ServiceEventType::REGISTERED:
                handleServiceRegistered(event.reference);
                break;
            case cdmf::ServiceEventType::UNREGISTERING:
                handleServiceUnregistering(event.reference);
                break;
            case cdmf::ServiceEventType::MODIFIED:
                handleServiceModified(event.reference);
                break;
        }
    }
};

// Register with filter
context->addServiceListener(
    monitor.get(),
    "(objectClass=com.example.IDatabase)"  // Only database events
);
```

### Custom Events

Create your own event system:

```cpp
// Event definition
struct DataEvent {
    enum Type { DATA_RECEIVED, DATA_PROCESSED, DATA_ERROR };
    Type type;
    std::string source;
    std::any data;
    std::chrono::system_clock::time_point timestamp;
};

// Event dispatcher
class DataEventDispatcher {
private:
    std::vector<std::function<void(const DataEvent&)>> listeners_;
    std::mutex mutex_;

public:
    void subscribe(std::function<void(const DataEvent&)> listener) {
        std::lock_guard lock(mutex_);
        listeners_.push_back(listener);
    }

    void dispatch(const DataEvent& event) {
        std::lock_guard lock(mutex_);
        for (const auto& listener : listeners_) {
            listener(event);
        }
    }
};
```

## Configuration Management

### Reading Configuration

```cpp
class ConfigurableModule : public cdmf::IManagedService {
private:
    int maxConnections_ = 10;
    std::string serverUrl_;
    bool enableLogging_ = true;

public:
    void updated(const cdmf::Properties& properties) override {
        // Read configuration
        maxConnections_ = properties.getInt("max.connections").value_or(10);
        serverUrl_ = properties.get("server.url", "localhost:8080");
        enableLogging_ = properties.getBool("enable.logging").value_or(true);

        // Apply configuration
        reconfigure();
    }

private:
    void reconfigure() {
        // Apply new configuration
        std::cout << "Reconfigured with max connections: "
                  << maxConnections_ << std::endl;
    }
};
```

### Providing Default Configuration

In resources/config.json:

```json
{
  "max.connections": 10,
  "server.url": "localhost:8080",
  "enable.logging": true,
  "retry.attempts": 3,
  "timeout.seconds": 30
}
```

Load in activator:

```cpp
void start(cdmf::IModuleContext* context) override {
    // Load default configuration
    auto configPath = context->getDataFile("config.json");
    auto config = loadJsonFile(configPath);

    // Convert to properties
    cdmf::Properties props;
    for (auto& [key, value] : config.items()) {
        props.set(key, value.get<std::string>());
    }

    // Apply configuration
    service_->updated(props);
}
```

## Testing Modules

### Unit Testing

Test your module components in isolation:

```cpp
// test/GreetingServiceTest.cpp
#include <gtest/gtest.h>
#include "GreetingServiceImpl.h"

TEST(GreetingServiceTest, BasicGreeting) {
    GreetingServiceImpl service;
    EXPECT_EQ(service.greet("World"), "Hello, World!");
}

TEST(GreetingServiceTest, CustomFormat) {
    GreetingServiceImpl service;
    service.setFormat("Greetings, {}!");
    EXPECT_EQ(service.greet("User"), "Greetings, User!");
}
```

### Integration Testing

Test module interaction with framework:

```cpp
class ModuleIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<cdmf::Framework> framework_;

    void SetUp() override {
        cdmf::FrameworkProperties props;
        props.storage_path = "./test-storage";

        framework_ = cdmf::FrameworkFactory::createFramework(props);
        framework_->init();
        framework_->start();
    }

    void TearDown() override {
        framework_->stop();
        framework_.reset();
    }
};

TEST_F(ModuleIntegrationTest, ModuleLifecycle) {
    // Install module
    auto module = framework_->installModule("greeting-module.so");
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(module->getState(), cdmf::ModuleState::INSTALLED);

    // Start module
    module->start();
    EXPECT_EQ(module->getState(), cdmf::ModuleState::ACTIVE);

    // Get service
    auto context = framework_->getContext();
    auto service = context->getService<IGreetingService>(
        "com.example.IGreetingService"
    );
    ASSERT_NE(service, nullptr);

    // Use service
    EXPECT_EQ(service->greet("Test"), "Hello, Test!");

    // Stop module
    module->stop();
    EXPECT_EQ(module->getState(), cdmf::ModuleState::RESOLVED);
}
```

### Mock Framework

For testing without full framework:

```cpp
class MockModuleContext : public cdmf::IModuleContext {
private:
    std::map<std::string, std::any> services_;

public:
    template<typename T>
    cdmf::ServiceRegistration registerService(
        const std::string& name,
        std::shared_ptr<T> service,
        const cdmf::Properties& props) override {
        services_[name] = service;
        return cdmf::ServiceRegistration();
    }

    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) override {
        auto it = services_.find(name);
        if (it != services_.end()) {
            return std::any_cast<std::shared_ptr<T>>(it->second);
        }
        return nullptr;
    }

    // ... implement other methods
};
```

## Debugging and Troubleshooting

### Enable Debug Logging

```cpp
// In your module
#include <cdmf/Logger.h>

void start(cdmf::IModuleContext* context) override {
    CDMF_LOG_DEBUG("Module starting with context: {}", context);

    try {
        // Module initialization
        CDMF_LOG_DEBUG("Creating service instance");
        service_ = std::make_shared<MyServiceImpl>();

        CDMF_LOG_DEBUG("Registering service");
        serviceReg_ = context->registerService<IMyService>(...);

        CDMF_LOG_INFO("Module started successfully");
    } catch (const std::exception& e) {
        CDMF_LOG_ERROR("Failed to start module: {}", e.what());
        throw;
    }
}
```

### Common Issues and Solutions

#### Issue: Module Won't Load

```bash
# Check module with ldd/otool
ldd my-module.so  # Linux
otool -L my-module.dylib  # macOS

# Common causes:
# 1. Missing dependencies
# 2. Wrong architecture (32 vs 64-bit)
# 3. Incompatible C++ ABI
```

#### Issue: Service Not Found

```cpp
// Debug service registry
auto refs = context->getServiceReferences<IMyService>(
    "com.example.IMyService", ""
);
std::cout << "Found " << refs.size() << " services" << std::endl;

for (const auto& ref : refs) {
    auto props = ref.getProperties();
    std::cout << "Service ID: " << ref.getServiceId() << std::endl;
    std::cout << "Properties: " << props.toString() << std::endl;
}
```

#### Issue: Module Dependencies Not Resolved

```cpp
// Check module state and dependencies
auto module = framework->getModule("com.example.mymodule");
std::cout << "State: " << module->getState() << std::endl;

auto deps = module->getDependencies();
for (auto dep : deps) {
    std::cout << "Dependency: " << dep->getSymbolicName()
              << " State: " << dep->getState() << std::endl;
}
```

### Using Debugger

```bash
# Debug with GDB
gdb ./app
(gdb) set environment LD_LIBRARY_PATH=./modules
(gdb) break MyModuleActivator::start
(gdb) run
```

### Memory Leak Detection

```bash
# Valgrind (Linux)
valgrind --leak-check=full ./app

# AddressSanitizer (compile-time)
g++ -fsanitize=address -g ...
```

## Best Practices

### 1. Thread Safety

Always make your services thread-safe:

```cpp
class ThreadSafeService : public IService {
private:
    mutable std::shared_mutex mutex_;
    std::map<std::string, std::string> data_;

public:
    void write(const std::string& key, const std::string& value) {
        std::unique_lock lock(mutex_);
        data_[key] = value;
    }

    std::string read(const std::string& key) const {
        std::shared_lock lock(mutex_);
        auto it = data_.find(key);
        return it != data_.end() ? it->second : "";
    }
};
```

### 2. Resource Management

Use RAII consistently:

```cpp
class ResourceManager {
private:
    std::unique_ptr<Resource> resource_;
    std::thread workerThread_;
    std::atomic<bool> running_{false};

public:
    void start() {
        resource_ = std::make_unique<Resource>();
        running_ = true;
        workerThread_ = std::thread([this] { work(); });
    }

    void stop() {
        running_ = false;
        if (workerThread_.joinable()) {
            workerThread_.join();
        }
        resource_.reset();
    }

    ~ResourceManager() {
        if (running_) {
            stop();
        }
    }
};
```

### 3. Error Handling

Handle errors gracefully:

```cpp
void start(cdmf::IModuleContext* context) override {
    try {
        initializeResources();
        registerServices(context);
    } catch (const ResourceException& e) {
        CDMF_LOG_ERROR("Resource initialization failed: {}", e.what());
        cleanup();
        throw cdmf::ModuleException("Failed to start module");
    } catch (const std::exception& e) {
        CDMF_LOG_ERROR("Unexpected error: {}", e.what());
        cleanup();
        throw;
    }
}
```

### 4. Service Versioning

Version your interfaces:

```cpp
// v1/IService.h
namespace v1 {
    class IService {
        virtual void doWork() = 0;
    };
}

// v2/IService.h
namespace v2 {
    class IService {
        virtual void doWork() = 0;
        virtual void doMoreWork() = 0;  // New method
    };
}
```

### 5. Lazy Initialization

Initialize expensive resources on-demand:

```cpp
class LazyService : public IService {
private:
    mutable std::once_flag initFlag_;
    mutable std::unique_ptr<ExpensiveResource> resource_;

    void ensureInitialized() const {
        std::call_once(initFlag_, [this] {
            resource_ = std::make_unique<ExpensiveResource>();
        });
    }

public:
    void doWork() override {
        ensureInitialized();
        resource_->process();
    }
};
```

## Common Patterns

### Factory Pattern

```cpp
class ServiceFactory : public IServiceFactory {
public:
    std::shared_ptr<IService> createService(
        const std::string& type) override {

        if (type == "fast") {
            return std::make_shared<FastService>();
        } else if (type == "reliable") {
            return std::make_shared<ReliableService>();
        } else {
            return std::make_shared<DefaultService>();
        }
    }
};
```

### Whiteboard Pattern

```cpp
// Service that reacts to other services
class WhiteboardConsumer {
private:
    cdmf::ServiceTracker<IDataSource> tracker_;
    std::vector<std::shared_ptr<IDataSource>> sources_;

public:
    WhiteboardConsumer(cdmf::IModuleContext* context)
        : tracker_(context, "com.example.IDataSource") {

        tracker_.onServiceAdded([this](auto source, auto props) {
            sources_.push_back(source);
            reconfigure();
        });

        tracker_.onServiceRemoved([this](auto source, auto props) {
            sources_.erase(
                std::remove(sources_.begin(), sources_.end(), source),
                sources_.end()
            );
            reconfigure();
        });

        tracker_.open();
    }

private:
    void reconfigure() {
        // React to service changes
        std::cout << "Now tracking " << sources_.size()
                  << " data sources" << std::endl;
    }
};
```

### Adapter Pattern

```cpp
// Adapt old interface to new
class LegacyAdapter : public IModernService {
private:
    std::shared_ptr<ILegacyService> legacy_;

public:
    explicit LegacyAdapter(std::shared_ptr<ILegacyService> legacy)
        : legacy_(legacy) {}

    Result process(const Request& req) override {
        // Convert modern request to legacy format
        auto legacyData = convertToLegacy(req);

        // Call legacy service
        auto legacyResult = legacy_->oldProcess(legacyData);

        // Convert result to modern format
        return convertToModern(legacyResult);
    }
};
```

## Examples

### Example 1: Database Module

```cpp
// IDatabaseService.h
class IDatabaseService {
public:
    virtual ~IDatabaseService() = default;
    virtual void connect(const std::string& connectionString) = 0;
    virtual void disconnect() = 0;
    virtual ResultSet query(const std::string& sql) = 0;
    virtual void execute(const std::string& sql) = 0;
};

// PostgreSQLModule.cpp
class PostgreSQLService : public IDatabaseService {
private:
    pqxx::connection* conn_ = nullptr;

public:
    void connect(const std::string& connectionString) override {
        conn_ = new pqxx::connection(connectionString);
    }

    void disconnect() override {
        delete conn_;
        conn_ = nullptr;
    }

    ResultSet query(const std::string& sql) override {
        pqxx::work txn(*conn_);
        auto result = txn.exec(sql);
        txn.commit();
        return convertResult(result);
    }

    void execute(const std::string& sql) override {
        pqxx::work txn(*conn_);
        txn.exec(sql);
        txn.commit();
    }
};

class PostgreSQLModuleActivator : public cdmf::IModuleActivator {
private:
    cdmf::ServiceRegistration serviceReg_;
    std::shared_ptr<PostgreSQLService> service_;

public:
    void start(cdmf::IModuleContext* context) override {
        service_ = std::make_shared<PostgreSQLService>();

        // Read configuration
        auto connStr = context->getProperty("db.connection.string");
        if (!connStr.empty()) {
            service_->connect(connStr);
        }

        // Register service with properties
        cdmf::Properties props;
        props.set("database.type", "postgresql");
        props.set("database.version", "13.0");

        serviceReg_ = context->registerService<IDatabaseService>(
            "com.example.IDatabaseService",
            service_,
            props
        );
    }

    void stop(cdmf::IModuleContext* context) override {
        service_->disconnect();
        service_.reset();
    }
};
```

### Example 2: HTTP Server Module

```cpp
// IHttpServer.h
class IHttpServer {
public:
    virtual ~IHttpServer() = default;
    virtual void start(int port) = 0;
    virtual void stop() = 0;
    virtual void addRoute(const std::string& path,
                         std::function<Response(const Request&)> handler) = 0;
};

// HttpServerModule.cpp
class HttpServerImpl : public IHttpServer {
private:
    httplib::Server server_;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
    int port_;

public:
    void start(int port) override {
        port_ = port;
        running_ = true;

        serverThread_ = std::thread([this] {
            server_.listen("0.0.0.0", port_);
        });
    }

    void stop() override {
        running_ = false;
        server_.stop();
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

    void addRoute(const std::string& path,
                 std::function<Response(const Request&)> handler) override {
        server_.Post(path, [handler](const httplib::Request& req,
                                    httplib::Response& res) {
            auto response = handler(convertRequest(req));
            res.set_content(response.body, response.contentType);
            res.status = response.statusCode;
        });
    }
};
```

### Example 3: Plugin Architecture

```cpp
// Plugin host module
class PluginHost : public IPluginHost {
private:
    std::map<std::string, std::shared_ptr<IPlugin>> plugins_;
    cdmf::ServiceTracker<IPlugin> pluginTracker_;

public:
    PluginHost(cdmf::IModuleContext* context)
        : pluginTracker_(context, "com.example.IPlugin") {

        // Automatically track all plugins
        pluginTracker_.onServiceAdded([this](auto plugin, auto props) {
            auto name = props.get("plugin.name");
            plugins_[name] = plugin;
            plugin->initialize(this);
            std::cout << "Plugin loaded: " << name << std::endl;
        });

        pluginTracker_.onServiceRemoved([this](auto plugin, auto props) {
            auto name = props.get("plugin.name");
            plugin->shutdown();
            plugins_.erase(name);
            std::cout << "Plugin unloaded: " << name << std::endl;
        });

        pluginTracker_.open();
    }

    void executePlugin(const std::string& name,
                      const std::string& command) override {
        auto it = plugins_.find(name);
        if (it != plugins_.end()) {
            it->second->execute(command);
        }
    }

    std::vector<std::string> listPlugins() const override {
        std::vector<std::string> names;
        for (const auto& [name, _] : plugins_) {
            names.push_back(name);
        }
        return names;
    }
};
```

---

*Previous: [API Reference](03-api-reference.md)*
*Next: [Deployment and Operations Guide →](05-deployment-operations.md)*