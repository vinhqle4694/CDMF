# Creating a Simple Service Module in CDMF

## Overview

This tutorial guides you through creating a very simple service module in CDMF. We'll build a **Hello Service** that provides greeting functionality. This tutorial explains every detail of the code and configuration files.

**What you'll learn:**
- How to create a service interface
- How to implement the service
- How to create a module activator
- How to handle the BOOT_COMPLETED event
- How to configure the module manifest
- How to build and test your module

**Time to complete:** 30-45 minutes

---

## What is a Service Module?

In CDMF, a **service module** is a loadable plugin that:
1. Provides functionality through a **service interface** (like an API)
2. Registers itself with the framework so other modules can find it
3. Can depend on other services
4. Has a lifecycle (start, stop, cleanup)

**Architecture:**
```
+------------------------------------------+
|         Service Interface                | <- Pure virtual (abstract) interface
|   (what the service can do)              |
+--------------------+---------------------+
                     |
                     | implements
                     v
+------------------------------------------+
|      Service Implementation              | <- Actual implementation
|   (how the service does it)              |
+--------------------+---------------------+
                     |
                     | managed by
                     v
+------------------------------------------+
|        Module Activator                  | <- Lifecycle manager
|   (creates, registers, destroys)         |
+------------------------------------------+
```

---

## Step 1: Create Module Directory Structure

First, create the directory for your module:

```bash
mkdir -p workspace/src/modules/hello_service_module
cd workspace/src/modules/hello_service_module
```

**Files you'll create:**
```
hello_service_module/
├── hello_service.h              <- Service interface (API definition)
├── hello_service_impl.h         <- Service implementation header
├── hello_service_impl.cpp       <- Service implementation code
├── hello_service_activator.h    <- Module lifecycle header
├── hello_service_activator.cpp  <- Module lifecycle implementation
├── manifest.json                <- Module configuration
└── CMakeLists.txt               <- Build configuration
```

---

## Step 2: Define the Service Interface

**File:** `hello_service.h`

The service interface is a **pure virtual class** (abstract interface) that defines what the service can do, without saying how it does it.

```cpp
/**
 * @file hello_service.h
 * @brief Hello Service Interface - Pure virtual interface for greeting functionality
 */

#pragma once

#include <string>

namespace cdmf {
namespace services {

/**
 * @brief Interface for Hello Service
 *
 * This is a simple service that provides greeting functionality.
 * It demonstrates basic service interface design in CDMF.
 */
class IHelloService {
public:
    virtual ~IHelloService() = default;

    /**
     * @brief Get a greeting message
     * @param name The name to greet
     * @return A personalized greeting message
     */
    virtual std::string greet(const std::string& name) = 0;

    /**
     * @brief Get service status
     * @return Current service status (Running/Stopped)
     */
    virtual std::string getStatus() const = 0;

    /**
     * @brief Get greeting count
     * @return Number of greetings provided since service started
     */
    virtual int getGreetingCount() const = 0;
};

} // namespace services
} // namespace cdmf
```

### Code Explanation:

**Line-by-line breakdown:**

```cpp
#pragma once
```
- **Purpose:** Prevents the header file from being included multiple times
- **Why:** Avoids compilation errors from duplicate definitions

```cpp
namespace cdmf {
namespace services {
```
- **Purpose:** Organizes code and prevents name conflicts
- **Convention:**
  - `cdmf` = framework namespace
  - `services` = service interfaces namespace
- **Why:** If another library also has an `IHelloService`, there's no conflict

```cpp
class IHelloService {
```
- **Naming convention:** Interface classes start with `I`
- **Why:** Makes it clear this is an interface, not an implementation

```cpp
public:
    virtual ~IHelloService() = default;
```
- **Purpose:** Virtual destructor for proper cleanup
- **Why:** When deleting a pointer to `IHelloService`, the actual implementation's destructor must be called
- **`= default`:** Use the compiler-generated destructor

```cpp
virtual std::string greet(const std::string& name) = 0;
```
- **`virtual`:** Can be overridden by derived classes
- **`= 0`:** Pure virtual function (must be implemented by derived class)
- **`const std::string&`:** Pass by reference (efficient, avoids copying)
- **Why pure virtual:** This is an interface - implementations must define the behavior

**Key principles:**
- Interface has **no implementation** (all methods are `= 0`)
- Interface has **no data members** (no variables)
- Interface is **stable** (rarely changes - other code depends on it)

---

## Step 3: Implement the Service

### Service Implementation Header

**File:** `hello_service_impl.h`

```cpp
/**
 * @file hello_service_impl.h
 * @brief Hello Service Implementation
 */

#pragma once

#include "hello_service.h"
#include <mutex>
#include <atomic>

namespace cdmf {
namespace services {

/**
 * @brief Implementation of IHelloService interface
 */
class HelloServiceImpl : public IHelloService {
public:
    HelloServiceImpl();
    ~HelloServiceImpl() override;

    /**
     * @brief Start the service work (called after BOOT_COMPLETED event)
     */
    void startWork();

    /**
     * @brief Stop the service work
     */
    void stopWork();

    // IHelloService interface
    std::string greet(const std::string& name) override;
    std::string getStatus() const override;
    int getGreetingCount() const override;

private:
    mutable std::mutex mutex_;
    bool workStarted_;
    std::atomic<int> greetingCount_;
};

} // namespace services
} // namespace cdmf
```

### Code Explanation:

```cpp
#include "hello_service.h"
#include <mutex>
#include <atomic>
```
- **`hello_service.h`:** The interface we're implementing
- **`<mutex>`:** For thread safety (multiple threads might call our service)
- **`<atomic>`:** For thread-safe counter (greetingCount)

```cpp
class HelloServiceImpl : public IHelloService {
```
- **Inheritance:** `HelloServiceImpl` implements `IHelloService`
- **Convention:** Implementation classes end with `Impl`

```cpp
void startWork();
void stopWork();
```
- **Purpose:** Separate "create service" from "start working"
- **Why:** Service is created in `start()`, but actual work starts after BOOT_COMPLETED
- **Benefit:** Can safely access other services in `startWork()` (all services are ready)

```cpp
std::string greet(const std::string& name) override;
```
- **`override`:** Marks this as implementing the interface method
- **Benefit:** Compiler checks we're actually overriding something (catches typos)

```cpp
private:
    mutable std::mutex mutex_;
    bool workStarted_;
    std::atomic<int> greetingCount_;
```
- **`mutable std::mutex mutex_`:**
  - **Purpose:** Protects shared state from concurrent access
  - **`mutable`:** Can be locked in `const` methods like `getStatus()`
  - **Why:** Multiple threads might call our service simultaneously

- **`bool workStarted_`:**
  - **Purpose:** Track if service has started work
  - **Why:** Don't process requests until after BOOT_COMPLETED

- **`std::atomic<int> greetingCount_`:**
  - **Purpose:** Count how many greetings we've provided
  - **`atomic`:** Thread-safe increment without explicit locking
  - **Why:** Simple counters can use atomics instead of mutex

### Service Implementation Code

**File:** `hello_service_impl.cpp`

```cpp
/**
 * @file hello_service_impl.cpp
 * @brief Hello Service Implementation
 */

#include "hello_service_impl.h"
#include "utils/log.h"
#include <stdexcept>

namespace cdmf {
namespace services {

HelloServiceImpl::HelloServiceImpl()
    : workStarted_(false)
    , greetingCount_(0) {
    LOGI("HelloServiceImpl created (work NOT started yet)");
}

HelloServiceImpl::~HelloServiceImpl() {
    stopWork();
    LOGI("HelloServiceImpl destroyed");
}

void HelloServiceImpl::startWork() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (workStarted_) {
        LOGW("HelloServiceImpl: Work already started");
        return;
    }

    LOGI("HelloServiceImpl: Starting work...");

    // Initialize service (could start threads, open connections, etc.)
    workStarted_ = true;
    greetingCount_ = 0;

    LOGI("HelloServiceImpl: Work started successfully");
}

void HelloServiceImpl::stopWork() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!workStarted_) {
        return;
    }

    LOGI("HelloServiceImpl: Stopping work...");

    // Cleanup (stop threads, close connections, etc.)
    workStarted_ = false;

    LOGI_FMT("HelloServiceImpl: Work stopped (provided " << greetingCount_.load() << " greetings)");
}

std::string HelloServiceImpl::greet(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!workStarted_) {
        LOGW("HelloServiceImpl: Service not ready - work not started");
        throw std::runtime_error("Service not ready - work not started");
    }

    // Increment greeting counter
    greetingCount_++;

    std::string greeting = "Hello, " + name + "! Welcome to CDMF!";
    LOGI_FMT("HelloServiceImpl: Greeting #" << greetingCount_.load() << " - " << greeting);

    return greeting;
}

std::string HelloServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workStarted_ ? "Running" : "Stopped";
}

int HelloServiceImpl::getGreetingCount() const {
    return greetingCount_.load();
}

} // namespace services
} // namespace cdmf
```

### Code Explanation:

```cpp
HelloServiceImpl::HelloServiceImpl()
    : workStarted_(false)
    , greetingCount_(0) {
```
- **Constructor initializer list:** Initialize member variables
- **Purpose:** Create service but DON'T start work yet
- **Why:** Work starts after BOOT_COMPLETED (when all services are available)

```cpp
std::lock_guard<std::mutex> lock(mutex_);
```
- **Purpose:** Automatically lock/unlock the mutex
- **RAII principle:** Lock acquired when created, released when destroyed (goes out of scope)
- **Thread safety:** Only one thread can execute this code block at a time

```cpp
if (!workStarted_) {
    throw std::runtime_error("Service not ready - work not started");
}
```
- **Purpose:** Don't process requests before BOOT_COMPLETED
- **Why:** Service might need other services that aren't ready yet

```cpp
greetingCount_++;
```
- **Atomic operation:** Thread-safe increment
- **Why atomic works here:** Simple read-modify-write of integer

```cpp
LOGI_FMT("HelloServiceImpl: Greeting #" << greetingCount_.load() << " - " << greeting);
```
- **Logging:** Track what the service is doing
- **`LOGI`:** Info level log
- **`_FMT`:** Formatted output with `<<` operator
- **`.load()`:** Read atomic value

---

## Step 4: Create the Module Activator

The activator manages the service lifecycle: create -> register -> listen for events -> start work -> cleanup.

### Module Activator Header

**File:** `hello_service_activator.h`

```cpp
/**
 * @file hello_service_activator.h
 * @brief Module activator for Hello Service
 */

#pragma once

#include "module/module_activator.h"
#include "core/event_listener.h"
#include "hello_service_impl.h"
#include "service/service_registration.h"

namespace cdmf {
namespace modules {

/**
 * @brief Event listener for BOOT_COMPLETED event
 */
class HelloBootCompletedListener : public IEventListener {
public:
    explicit HelloBootCompletedListener(services::HelloServiceImpl* service);

    void handleEvent(const Event& event) override;

private:
    services::HelloServiceImpl* service_;
};

/**
 * @brief Module activator for Hello Service
 *
 * Manages the lifecycle of the Hello Service module:
 * 1. Creates service instance in start()
 * 2. Registers service with framework
 * 3. Listens for BOOT_COMPLETED event
 * 4. Starts actual work after BOOT_COMPLETED
 * 5. Cleans up in stop()
 */
class HelloServiceActivator : public IModuleActivator {
public:
    HelloServiceActivator();
    ~HelloServiceActivator() override;

    void start(IModuleContext* context) override;
    void stop(IModuleContext* context) override;

private:
    services::HelloServiceImpl* service_;
    HelloBootCompletedListener* bootListener_;
    ServiceRegistration registration_;
};

} // namespace modules
} // namespace cdmf

// Module factory functions
extern "C" {
    cdmf::IModuleActivator* createModuleActivator();
    void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
```

### Code Explanation:

```cpp
class HelloBootCompletedListener : public IEventListener {
```
- **Purpose:** Listen for the BOOT_COMPLETED event
- **Why separate class:** Separates event handling from module lifecycle

```cpp
explicit HelloBootCompletedListener(services::HelloServiceImpl* service);
```
- **`explicit`:** Prevents implicit conversions
- **Takes service pointer:** Needs to call `service->startWork()` when event fires

```cpp
class HelloServiceActivator : public IModuleActivator {
```
- **Purpose:** Manages module lifecycle
- **`IModuleActivator`:** Framework interface for module management
- **Methods:** `start()` and `stop()`

```cpp
void start(IModuleContext* context) override;
void stop(IModuleContext* context) override;
```
- **`start()`:** Called when framework loads the module
- **`stop()`:** Called when framework unloads the module
- **`context`:** Provides access to framework services (register, events, etc.)

```cpp
private:
    services::HelloServiceImpl* service_;
    HelloBootCompletedListener* bootListener_;
    ServiceRegistration registration_;
```
- **`service_`:** The service implementation we manage
- **`bootListener_`:** Event listener for BOOT_COMPLETED
- **`registration_`:** Handle to unregister service later

```cpp
extern "C" {
    cdmf::IModuleActivator* createModuleActivator();
    void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
```
- **`extern "C"`:** No C++ name mangling (framework can find these functions)
- **Purpose:** Framework calls these to create/destroy the activator
- **Factory pattern:** Framework doesn't need to know the activator class name

### Module Activator Implementation

**File:** `hello_service_activator.cpp`

```cpp
/**
 * @file hello_service_activator.cpp
 * @brief Module activator implementation for Hello Service
 */

#include "hello_service_activator.h"
#include "module/module_context.h"
#include "core/event.h"
#include "core/event_filter.h"
#include "utils/log.h"
#include "utils/properties.h"

namespace cdmf {
namespace modules {

// HelloBootCompletedListener implementation

HelloBootCompletedListener::HelloBootCompletedListener(services::HelloServiceImpl* service)
    : service_(service) {
}

void HelloBootCompletedListener::handleEvent(const Event& event) {
    LOGI_FMT("HelloBootCompletedListener: handleEvent called with type='" << event.getType() << "'");

    if (event.getType() == "BOOT_COMPLETED") {
        LOGI("HelloBootCompletedListener: Received BOOT_COMPLETED event");

        // Start the actual service work now that all services are ready
        if (service_) {
            service_->startWork();
        } else {
            LOGW("HelloBootCompletedListener: service_ is null!");
        }
    } else {
        LOGW_FMT("HelloBootCompletedListener: Event type mismatch - expected 'BOOT_COMPLETED', got '" << event.getType() << "'");
    }
}

// HelloServiceActivator implementation

HelloServiceActivator::HelloServiceActivator()
    : service_(nullptr)
    , bootListener_(nullptr)
    , registration_() {
}

HelloServiceActivator::~HelloServiceActivator() {
    // Cleanup is done in stop()
}

void HelloServiceActivator::start(IModuleContext* context) {
    LOGI("HelloServiceActivator: Starting module...");

    // 1. Create service instance (but don't start work yet)
    service_ = new services::HelloServiceImpl();

    // 2. Register service with framework
    Properties props;
    props.set("service.description", "Simple hello/greeting service");
    props.set("service.vendor", "CDMF");
    props.set("service.version", "1.0.0");

    registration_ = context->registerService("cdmf::IHelloService", service_, props);
    LOGI("HelloServiceActivator: Service registered");

    // 3. Create BOOT_COMPLETED listener
    bootListener_ = new HelloBootCompletedListener(service_);

    // 4. Register listener for BOOT_COMPLETED event
    EventFilter filter("(type=BOOT_COMPLETED)");
    LOGI_FMT("HelloServiceActivator: Created filter with string: '" << filter.toString() << "'");
    context->addEventListener(bootListener_, filter, 0, true);  // synchronous=true
    LOGI("HelloServiceActivator: BOOT_COMPLETED listener registered (synchronous=true)");

    LOGI("HelloServiceActivator: Module started (waiting for BOOT_COMPLETED)");
}

void HelloServiceActivator::stop(IModuleContext* context) {
    LOGI("HelloServiceActivator: Stopping module...");

    // 1. Remove event listener
    if (bootListener_) {
        context->removeEventListener(bootListener_);
        delete bootListener_;
        bootListener_ = nullptr;
    }

    // 2. Unregister service
    if (registration_.isValid()) {
        registration_.unregister();
    }

    // 3. Stop service work and cleanup
    if (service_) {
        service_->stopWork();
        delete service_;
        service_ = nullptr;
    }

    LOGI("HelloServiceActivator: Module stopped");
}

} // namespace modules
} // namespace cdmf

// Module factory functions
extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::HelloServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
```

### Code Explanation:

#### Event Listener:

```cpp
void HelloBootCompletedListener::handleEvent(const Event& event) {
    if (event.getType() == "BOOT_COMPLETED") {
        service_->startWork();
    }
}
```
- **Purpose:** When BOOT_COMPLETED fires, start the service work
- **`event.getType()`:** Get the event type string
- **Why:** All services are now registered and available

#### Activator start() method:

```cpp
void HelloServiceActivator::start(IModuleContext* context) {
```
- **Called by:** Framework when loading the module
- **`context`:** Interface to framework services

**Step 1: Create service**
```cpp
service_ = new services::HelloServiceImpl();
```
- **Create instance:** Allocate service on heap
- **Important:** Don't start work yet (wait for BOOT_COMPLETED)

**Step 2: Register service**
```cpp
Properties props;
props.set("service.description", "Simple hello/greeting service");
props.set("service.vendor", "CDMF");
props.set("service.version", "1.0.0");

registration_ = context->registerService("cdmf::IHelloService", service_, props);
```
- **`Properties`:** Metadata about the service (key-value pairs)
- **Common properties:**
  - `service.description`: Human-readable description
  - `service.vendor`: Who created it
  - `service.version`: Version number
  - `service.ranking`: Priority (higher = preferred)

- **`registerService()`:** Register with framework
  - **Parameter 1:** Interface name (what consumers look for)
  - **Parameter 2:** Service implementation pointer
  - **Parameter 3:** Properties (metadata)
  - **Returns:** `ServiceRegistration` (handle to unregister later)

- **Why register:** Makes service discoverable by other modules

**Step 3: Create event listener**
```cpp
bootListener_ = new HelloBootCompletedListener(service_);
```
- **Purpose:** Create listener that will start work when BOOT_COMPLETED fires
- **Passes `service_`:** Listener needs to call `service_->startWork()`

**Step 4: Register event listener**
```cpp
EventFilter filter("(type=BOOT_COMPLETED)");
context->addEventListener(bootListener_, filter, 0, true);
```
- **`EventFilter`:** Specifies which events we want to receive
  - **Syntax:** `"(type=BOOT_COMPLETED)"` - LDAP-style filter
  - **Matches:** Events where `type` property equals `"BOOT_COMPLETED"`

- **`addEventListener()`:**
  - **Parameter 1:** The listener object
  - **Parameter 2:** Event filter (which events to receive)
  - **Parameter 3:** Priority (0 = normal)
  - **Parameter 4:** Synchronous (true = handle immediately, false = queue for later)

#### Activator stop() method:

```cpp
void HelloServiceActivator::stop(IModuleContext* context) {
```
- **Called by:** Framework when unloading the module
- **Cleanup order matters:** Reverse of creation order

**Step 1: Remove event listener**
```cpp
if (bootListener_) {
    context->removeEventListener(bootListener_);
    delete bootListener_;
    bootListener_ = nullptr;
}
```
- **Remove first:** Prevent events during shutdown
- **Delete:** Free memory
- **Set to nullptr:** Prevent double-delete

**Step 2: Unregister service**
```cpp
if (registration_.isValid()) {
    registration_.unregister();
}
```
- **Unregister:** Remove service from framework
- **Why:** Other modules can't use a service being destroyed

**Step 3: Stop and delete service**
```cpp
if (service_) {
    service_->stopWork();
    delete service_;
    service_ = nullptr;
}
```
- **Stop work:** Clean shutdown (stop threads, close connections)
- **Delete:** Free memory
- **Set to nullptr:** Prevent double-delete

#### Factory functions:

```cpp
extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::HelloServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
```
- **`extern "C"`:** Framework can find these functions by name
- **`createModuleActivator()`:** Framework calls this to create activator
- **`destroyModuleActivator()`:** Framework calls this to delete activator
- **Why:** Framework doesn't need to know the activator class name

---

## Step 5: Create Module Manifest (Configuration)

**File:** `manifest.json`

```json
{
  "module": {
    "symbolic-name": "cdmf.hello_service",
    "name": "Hello Service Module",
    "version": "1.0.0",
    "description": "Simple greeting service demonstrating CDMF module development",
    "library": "hello_service_module.so"
  },
  "dependencies": [],
  "exports": [
    {
      "interface": "cdmf::IHelloService",
      "version": "1.0.0"
    }
  ],
  "activator": {
    "class": "cdmf::modules::HelloServiceActivator",
    "create-function": "createModuleActivator",
    "destroy-function": "destroyModuleActivator"
  }
}
```

### Manifest Explanation:

#### Module Section:
```json
"module": {
  "symbolic-name": "cdmf.hello_service",
  "name": "Hello Service Module",
  "version": "1.0.0",
  "description": "Simple greeting service demonstrating CDMF module development",
  "library": "hello_service_module.so"
}
```

**Field-by-field:**

- **`symbolic-name`**: Unique identifier for this module
  - **Format:** Reverse DNS style (e.g., `com.company.module`)
  - **Purpose:** Framework uses this to identify the module
  - **Must be unique:** No two modules can have the same symbolic name
  - **Example:** `"cdmf.hello_service"`

- **`name`**: Human-readable module name
  - **Purpose:** Display in logs, admin tools
  - **Example:** `"Hello Service Module"`

- **`version`**: Module version number
  - **Format:** Semantic versioning (major.minor.patch)
  - **Purpose:** Dependency resolution, upgrades
  - **Example:** `"1.0.0"`
    - `1` = major version (breaking changes)
    - `0` = minor version (new features)
    - `0` = patch version (bug fixes)

- **`description`**: What the module does
  - **Purpose:** Documentation
  - **Example:** `"Simple greeting service demonstrating CDMF module development"`

- **`library`**: Shared library filename
  - **Linux:** `"hello_service_module.so"`
  - **Windows:** `"hello_service_module.dll"`
  - **macOS:** `"hello_service_module.dylib"`
  - **Purpose:** Framework loads this library file
  - **Path:** Relative to library directory (e.g., `./lib/`)

#### Dependencies Section:
```json
"dependencies": []
```

- **Purpose:** List other modules this module needs
- **Empty array:** This module has no dependencies
- **Example with dependencies:**
  ```json
  "dependencies": [
    {
      "symbolic-name": "cdmf.config_service",
      "version-range": "[1.0.0, 2.0.0)"
    }
  ]
  ```
  - **`symbolic-name`:** Which module we depend on
  - **`version-range`:** Compatible versions
    - `[1.0.0, 2.0.0)` = version >= 1.0.0 AND < 2.0.0
    - `[` = inclusive, `)` = exclusive

#### Exports Section:
```json
"exports": [
  {
    "interface": "cdmf::IHelloService",
    "version": "1.0.0"
  }
]
```

- **Purpose:** Declare what services this module provides
- **`interface`:** The service interface name
  - **Must match:** The name used in `registerService()`
  - **Format:** C++ fully-qualified name with namespace
  - **Example:** `"cdmf::IHelloService"` (namespace::ClassName)

- **`version`:** Service interface version
  - **Purpose:** API versioning
  - **Breaking change:** Increment major version
  - **Example:** `"1.0.0"`

- **Multiple exports example:**
  ```json
  "exports": [
    {
      "interface": "cdmf::IHelloService",
      "version": "1.0.0"
    },
    {
      "interface": "cdmf::IGreetingProvider",
      "version": "1.0.0"
    }
  ]
  ```

#### Activator Section:
```json
"activator": {
  "class": "cdmf::modules::HelloServiceActivator",
  "create-function": "createModuleActivator",
  "destroy-function": "destroyModuleActivator"
}
```

- **Purpose:** Tell framework how to create/destroy the module activator

- **`class`**: Activator class name (for documentation)
  - **Value:** `"cdmf::modules::HelloServiceActivator"`
  - **Not used by framework:** Just for reference

- **`create-function`**: Function name to create activator
  - **Value:** `"createModuleActivator"`
  - **Must match:** The `extern "C"` function name in your code
  - **Framework calls:** This function to create the activator

- **`destroy-function`**: Function name to destroy activator
  - **Value:** `"destroyModuleActivator"`
  - **Must match:** The `extern "C"` function name in your code
  - **Framework calls:** This function to delete the activator

### Common Manifest Patterns:

**1. Module with dependencies:**
```json
{
  "module": {
    "symbolic-name": "cdmf.my_module",
    "name": "My Module",
    "version": "1.0.0",
    "library": "my_module.so"
  },
  "dependencies": [
    {
      "symbolic-name": "cdmf.config_service",
      "version-range": "[1.0.0, 2.0.0)"
    },
    {
      "symbolic-name": "cdmf.logging_service",
      "version-range": "[1.0.0, )"
    }
  ],
  "exports": [ ... ],
  "activator": { ... }
}
```

**2. Module with multiple service exports:**
```json
{
  "module": { ... },
  "dependencies": [],
  "exports": [
    {
      "interface": "cdmf::IUserService",
      "version": "1.0.0"
    },
    {
      "interface": "cdmf::IAuthService",
      "version": "1.0.0"
    }
  ],
  "activator": { ... }
}
```

---

## Step 6: Create Build Configuration

**File:** `CMakeLists.txt`

```cmake
# Hello Service Module CMakeLists

# Create shared library for the module
add_library(hello_service_module SHARED
    hello_service_impl.cpp
    hello_service_activator.cpp
)

# Include directories
target_include_directories(hello_service_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/workspace/src/framework/include
)

# Link with framework libraries
target_link_libraries(hello_service_module PRIVATE
    cdmf_core
    cdmf_module
    cdmf_service
)

# Set output properties
set_target_properties(hello_service_module PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy manifest to config directory
add_custom_command(TARGET hello_service_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/hello_service_module.json
    COMMENT "Copying hello_service_module manifest to config directory"
)
```

### CMake Explanation:

```cmake
add_library(hello_service_module SHARED
    hello_service_impl.cpp
    hello_service_activator.cpp
)
```
- **`add_library`**: Create a library
- **`hello_service_module`**: Library name (target name)
- **`SHARED`**: Shared library (.so / .dll) not static (.a / .lib)
- **Source files**: List of .cpp files to compile
- **Note**: Don't list .h files (they're included by .cpp files)

```cmake
target_include_directories(hello_service_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/workspace/src/framework/include
)
```
- **`target_include_directories`**: Add include paths
- **`PRIVATE`**: Only this target uses these paths (not propagated to dependents)
- **`${CMAKE_CURRENT_SOURCE_DIR}`**: Current directory (for `#include "hello_service.h"`)
- **Framework include**: For `#include "module/module_activator.h"` etc.

```cmake
target_link_libraries(hello_service_module PRIVATE
    cdmf_core
    cdmf_module
    cdmf_service
)
```
- **`target_link_libraries`**: Link with other libraries
- **Framework libraries**:
  - `cdmf_core`: Core framework (Event, Framework)
  - `cdmf_module`: Module management (IModuleActivator, IModuleContext)
  - `cdmf_service`: Service layer (ServiceRegistration, ServiceReference)

```cmake
set_target_properties(hello_service_module PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
```
- **`set_target_properties`**: Set library properties
- **`PREFIX ""`**: Don't add "lib" prefix
  - **Without:** `hello_service_module.so` (good)
  - **With default:** `libhello_service_module.so` (needs manifest update)
- **`LIBRARY_OUTPUT_DIRECTORY`**: Where to put the .so file
  - **Output:** `build/lib/hello_service_module.so`

```cmake
add_custom_command(TARGET hello_service_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/hello_service_module.json
    COMMENT "Copying hello_service_module manifest to config directory"
)
```
- **`add_custom_command`**: Run commands after build
- **`POST_BUILD`**: After library is built
- **Step 1**: Create `build/config/modules/` directory (if doesn't exist)
- **Step 2**: Copy `manifest.json` to `build/config/modules/hello_service_module.json`
- **Why**: Framework scans `config/modules/` for module manifests

---

## Step 7: Register Module in Main CMakeLists

**File:** `workspace/CMakeLists.txt`

Add this line in the "Modules" section:

```cmake
# ==============================================================================
# Modules
# ==============================================================================

# Hello Service Module - Simple greeting service example
add_subdirectory(src/modules/hello_service_module)
```

### Explanation:

```cmake
add_subdirectory(src/modules/hello_service_module)
```
- **Purpose**: Include the module's CMakeLists.txt in the build
- **Effect**: `hello_service_module` target is now part of the build
- **Path**: Relative to main CMakeLists.txt location

---

## Step 8: Build the Module

```bash
cd workspace/build
cmake ..
cmake --build . --target hello_service_module
```

### Expected Output:
```
[ 90%] Building CXX object src/modules/hello_service_module/CMakeFiles/hello_service_module.dir/hello_service_impl.cpp.o
[ 95%] Building CXX object src/modules/hello_service_module/CMakeFiles/hello_service_module.dir/hello_service_activator.cpp.o
[100%] Linking CXX shared library ../../../lib/hello_service_module.so
[100%] Built target hello_service_module
```

### Verify Build Output:
```bash
# Check library exists
ls -lh build/lib/hello_service_module.so

# Check manifest copied
ls -lh build/config/modules/hello_service_module.json
```

---

## Step 9: Run and Test

### Start the Framework:
```bash
cd build/bin
./cdmf
```

### Expected Log Output:
```
[INFO] Installing application modules...
[INFO] Found 2 module config(s)
[INFO]   - Processing config: ./config/modules/hello_service_module.json
[INFO]     Module installed: cdmf.hello_service v1.0.0
[INFO]     Module started: cdmf.hello_service
[INFO] HelloServiceActivator: Starting module...
[INFO] HelloServiceImpl created (work NOT started yet)
[INFO] HelloServiceActivator: Service registered
[INFO] HelloServiceActivator: BOOT_COMPLETED listener registered
[INFO] All services started - firing BOOT_COMPLETED event
[INFO] HelloBootCompletedListener: Received BOOT_COMPLETED event
[INFO] HelloServiceImpl: Starting work...
[INFO] HelloServiceImpl: Work started successfully
[INFO] Framework Status
[INFO] Loaded Modules: 2
[INFO]   - cdmf.config_service v1.0.0 [3]
[INFO]   - cdmf.hello_service v1.0.0 [3]
```

### Verify Module Loaded:

**Check module status:**
```
cdmf> list
```

**Expected:**
```
Loaded Modules: 2
  - cdmf.config_service v1.0.0 [ACTIVE]
  - cdmf.hello_service v1.0.0 [ACTIVE]
```

---

## Understanding the Module Lifecycle

### Timeline:

```
+------------------------------------------------------+
| 1. Framework starts                                  |
|    - Framework::init()                               |
|    - Framework::start()                              |
+------------------------+-----------------------------+
                         |
                         v
+------------------------------------------------------+
| 2. Module installation                               |
|    - Framework scans config/modules/                 |
|    - Finds hello_service_module.json                 |
|    - Loads hello_service_module.so                   |
|    - Calls createModuleActivator()                   |
+------------------------+-----------------------------+
                         |
                         v
+------------------------------------------------------+
| 3. Module start (HelloServiceActivator::start())     |
|    a. Create service (new HelloServiceImpl)          |
|    b. Register service with framework                |
|    c. Create BOOT_COMPLETED listener                 |
|    d. Register event listener                        |
|    * Service is REGISTERED but NOT working yet       |
+------------------------+-----------------------------+
                         |
                         v
+------------------------------------------------------+
| 4. All modules installed and started                 |
|    - All services registered                         |
|    - All event listeners registered                  |
+------------------------+-----------------------------+
                         |
                         v
+------------------------------------------------------+
| 5. BOOT_COMPLETED event fired                        |
|    - Event: "BOOT_COMPLETED"                         |
|    - Delivered to all registered listeners           |
+------------------------+-----------------------------+
                         |
                         v
+------------------------------------------------------+
| 6. Event handler (HelloBootCompletedListener)        |
|    - handleEvent() called                            |
|    - Calls service_->startWork()                     |
|    * Service NOW starts actual work                  |
+------------------------+-----------------------------+
                         |
                         v
+------------------------------------------------------+
| 7. Service operational                               |
|    - Can call greet(), getStatus(), etc.             |
|    - Other modules can use the service               |
+------------------------------------------------------+
```

### Why BOOT_COMPLETED?

**Problem without BOOT_COMPLETED:**
- Module A starts -> needs Module B -> but B isn't loaded yet -> crash!

**Solution with BOOT_COMPLETED:**
1. All modules install and register services (fast, no work yet)
2. BOOT_COMPLETED fires -> all services available
3. Each service starts actual work -> can safely use other services

**Example:**
```cpp
void HelloServiceImpl::startWork() {
    // Now safe to get other services!
    auto configService = context->getServiceReference("cdmf::IConfigService");
    if (configService) {
        // Use config service
    }
}
```

---

## Common Issues and Solutions

### 1. Module doesn't load

**Symptom:** Module not in `lsmod` output

**Check:**
```bash
# Manifest exists?
ls build/config/modules/hello_service_module.json

# Library exists?
ls build/lib/hello_service_module.so

# Library path in manifest correct?
cat build/config/modules/hello_service_module.json | grep library
```

**Solution:**
- Check CMakeLists.txt copies manifest correctly
- Check manifest `library` field matches actual filename

### 2. Service not registered

**Symptom:** "Service registered" log message not appearing

**Check logs for:**
```
HelloServiceActivator: Service registered
```

**Common causes:**
- Exception in `start()` method
- Wrong interface name in `registerService()`
- Activator `start()` not called

**Solution:**
- Check logs for exceptions
- Verify interface name matches manifest `exports` section

### 3. BOOT_COMPLETED not received

**Symptom:** "Work started successfully" not logged

**Check logs for:**
```
HelloServiceActivator: BOOT_COMPLETED listener registered
All services started - firing BOOT_COMPLETED event
```

**Common causes:**
- Event listener not registered correctly
- Event filter wrong
- Framework didn't fire event

**Solution:**
- Check `addEventListener()` called
- Verify filter syntax: `"(type=BOOT_COMPLETED)"`
- Check framework fires event in main.cpp

### 4. Compilation errors

**Common errors:**

**Error:** `undefined reference to cdmf::Event::Event`
**Solution:** Add `cdmf_core` to `target_link_libraries`

**Error:** `hello_service.h: No such file or directory`
**Solution:** Add `${CMAKE_CURRENT_SOURCE_DIR}` to `target_include_directories`

**Error:** `IModuleActivator is not a class or namespace`
**Solution:** Add framework include path to `target_include_directories`

---

## Next Steps

### Extend the Hello Service:

**1. Add service consumer:**
Create another module that uses IHelloService:

```cpp
void ConsumerActivator::start(IModuleContext* context) {
    // Get service reference
    auto serviceRef = context->getServiceReference("cdmf::IHelloService");
    if (serviceRef) {
        auto* helloService = context->getService<cdmf::services::IHelloService>(serviceRef);
        if (helloService) {
            std::string greeting = helloService->greet("World");
            LOGI_FMT("Received: " << greeting);
        }
    }
}
```

**2. Add service tracker:**
Track service availability dynamically:

```cpp
class HelloServiceTracker : public ServiceTrackerCustomizer {
public:
    void addingService(ServiceReference* ref, void* service) override {
        LOGI("Hello Service available!");
        auto* hello = static_cast<IHelloService*>(service);
        hello->greet("from tracker");
    }

    void removedService(ServiceReference* ref, void* service) override {
        LOGI("Hello Service removed!");
    }
};
```

**3. Add configuration:**
Read configuration from config service:

```cpp
void HelloServiceImpl::startWork() {
    auto configRef = context->getServiceReference("cdmf::IConfigService");
    if (configRef) {
        auto* config = context->getService<IConfigService>(configRef);
        std::string greeting = config->getString("hello.greeting", "Hello");
        // Use custom greeting
    }
}
```

**4. Add thread:**
Run background work:

```cpp
class HelloServiceImpl : public IHelloService {
private:
    std::thread workerThread_;

    void startWork() {
        workStarted_ = true;
        workerThread_ = std::thread(&HelloServiceImpl::workerLoop, this);
    }

    void workerLoop() {
        while (workStarted_) {
            // Do background work
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};
```

---

## Summary Checklist

When creating a new service module:

- [ ] Define service interface (pure virtual class)
- [ ] Implement service (concrete class)
- [ ] Create module activator (lifecycle management)
- [ ] Create event listener for BOOT_COMPLETED
- [ ] Create manifest.json with correct:
  - [ ] Symbolic name (unique)
  - [ ] Library filename (matches build output)
  - [ ] Exports (interface names)
  - [ ] Activator functions (match extern "C" functions)
- [ ] Create CMakeLists.txt with:
  - [ ] Source files
  - [ ] Include directories
  - [ ] Link libraries
  - [ ] Manifest copy command
- [ ] Add module to main CMakeLists.txt
- [ ] Build and verify:
  - [ ] Library file created
  - [ ] Manifest copied to config/modules
  - [ ] Module loads and starts
  - [ ] BOOT_COMPLETED received
  - [ ] Service work started

---

## Reference: Complete File List

```
workspace/src/modules/hello_service_module/
├── hello_service.h                  (Service interface)
├── hello_service_impl.h             (Implementation header)
├── hello_service_impl.cpp           (Implementation)
├── hello_service_activator.h        (Activator header)
├── hello_service_activator.cpp      (Activator implementation)
├── manifest.json                    (Module configuration)
└── CMakeLists.txt                   (Build configuration)

workspace/build/
├── lib/
│   └── hello_service_module.so      (Built library)
└── config/modules/
    └── hello_service_module.json    (Copied manifest)
```

**Total lines of code:** ~350 lines
**Compilation time:** ~5 seconds
**Module size:** ~385 KB

Congratulations! You've created a complete CDMF service module!
