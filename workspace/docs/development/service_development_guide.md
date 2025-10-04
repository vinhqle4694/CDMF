# Service Development Guide

## Overview

Services in CDMF are registered components that provide specific functionality to modules and other services. This guide shows how to create a new service module.

## Service Architecture

```
Module (provides service)
         ↓
    (registers)
         ↓
Service Registry
         ↓
     (lookup)
         ↓
Consumer Module (uses service)
```

## Step-by-Step Guide

### 1. Create Module Directory Structure

```
workspace/src/modules/<service_name>_module/
├── <service_interface>.h          # Service interface
├── <service_impl>.h               # Service implementation header
├── <service_impl>.cpp             # Service implementation
├── <module>_activator.h           # Module activator header
├── <module>_activator.cpp         # Module activator implementation
├── manifest.json                  # Module metadata
└── CMakeLists.txt                 # Build configuration
```

### 2. Define Service Interface

Create the service interface header (e.g., `my_service.h`):

```cpp
#pragma once

#include <string>
#include <memory>

namespace cdmf {
namespace services {

/**
 * @brief My Service Interface
 *
 * Service interface that other modules can use
 */
class IMyService {
public:
    virtual ~IMyService() = default;

    /**
     * @brief Perform service operation
     * @param input Input parameter
     * @return Operation result
     */
    virtual std::string doOperation(const std::string& input) = 0;

    /**
     * @brief Get service status
     * @return Service status string
     */
    virtual std::string getStatus() const = 0;
};

} // namespace services
} // namespace cdmf
```

### 3. Implement Service Class

Create service implementation (e.g., `my_service_impl.h` and `my_service_impl.cpp`):

**my_service_impl.h:**
```cpp
#pragma once

#include "my_service.h"
#include <mutex>

namespace cdmf {
namespace services {

class MyServiceImpl : public IMyService {
public:
    MyServiceImpl();
    ~MyServiceImpl() override;

    // IMyService interface
    std::string doOperation(const std::string& input) override;
    std::string getStatus() const override;

    // Lifecycle
    void start();
    void stop();

private:
    mutable std::mutex mutex_;
    bool running_;
    std::string data_;
};

} // namespace services
} // namespace cdmf
```

**my_service_impl.cpp:**
```cpp
#include "my_service_impl.h"
#include "utils/log.h"

namespace cdmf {
namespace services {

MyServiceImpl::MyServiceImpl()
    : running_(false) {
    LOGI("MyServiceImpl created");
}

MyServiceImpl::~MyServiceImpl() {
    stop();
    LOGI("MyServiceImpl destroyed");
}

void MyServiceImpl::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        running_ = true;
        LOGI("MyService started");
    }
}

void MyServiceImpl::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        running_ = false;
        LOGI("MyService stopped");
    }
}

std::string MyServiceImpl::doOperation(const std::string& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        throw std::runtime_error("Service not running");
    }

    LOGI_FMT("Processing operation: " << input);
    return "Processed: " + input;
}

std::string MyServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ ? "Running" : "Stopped";
}

} // namespace services
} // namespace cdmf
```

### 4. Create Module Activator

**my_service_activator.h:**
```cpp
#pragma once

#include "module/module_activator.h"
#include "my_service_impl.h"
#include <memory>

namespace cdmf {
namespace modules {

class MyServiceActivator : public IModuleActivator {
public:
    void start(IModuleContext* context) override;
    void stop(IModuleContext* context) override;

private:
    std::unique_ptr<services::MyServiceImpl> service_;
    ServiceRegistration* registration_;
};

} // namespace modules
} // namespace cdmf

// Module entry points
extern "C" {
    cdmf::IModuleActivator* createModuleActivator();
    void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
```

**my_service_activator.cpp:**
```cpp
#include "my_service_activator.h"
#include "utils/log.h"

namespace cdmf {
namespace modules {

void MyServiceActivator::start(IModuleContext* context) {
    LOGI("Starting My Service Module...");

    // Create service instance
    service_ = std::make_unique<services::MyServiceImpl>();
    service_->start();

    // Register service
    Properties props;
    props.set("service.description", "My custom service");
    props.set("service.vendor", "CDMF");

    registration_ = context->registerService(
        "cdmf::IMyService",
        service_.get(),
        props
    );

    LOGI("My Service Module started successfully");
}

void MyServiceActivator::stop(IModuleContext* context) {
    LOGI("Stopping My Service Module...");

    // Unregister service
    if (registration_) {
        registration_->unregister();
        registration_ = nullptr;
    }

    // Stop and destroy service
    if (service_) {
        service_->stop();
        service_.reset();
    }

    LOGI("My Service Module stopped");
}

} // namespace modules
} // namespace cdmf

// Module entry points implementation
extern "C" {
    cdmf::IModuleActivator* createModuleActivator() {
        return new cdmf::modules::MyServiceActivator();
    }

    void destroyModuleActivator(cdmf::IModuleActivator* activator) {
        delete activator;
    }
}
```

### 5. Create Module Manifest

**manifest.json:**
```json
{
  "module": {
    "symbolic-name": "cdmf.myservice",
    "name": "My Service Module",
    "version": "1.0.0",
    "library": "my_service_module.so",
    "description": "Provides My Service functionality",
    "vendor": "CDMF Project",
    "category": "service"
  },

  "dependencies": [],

  "exports": [
    {
      "interface": "cdmf::IMyService",
      "version": "1.0.0"
    }
  ],

  "activator": {
    "class": "cdmf::modules::MyServiceActivator",
    "create-function": "createModuleActivator",
    "destroy-function": "destroyModuleActivator"
  },

  "properties": {
    "service.ranking": "100",
    "service.scope": "singleton"
  }
}
```

### 6. Create CMakeLists.txt

```cmake
# My Service Module
cmake_minimum_required(VERSION 3.14)

# Module library
add_library(my_service_module SHARED
    my_service_impl.cpp
    my_service_activator.cpp
)

target_include_directories(my_service_module
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/framework/include
)

target_link_libraries(my_service_module
    PRIVATE
        cdmf_core
        cdmf_module
        cdmf_service
)

# Set module output to lib directory
set_target_properties(my_service_module PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my_service_module"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy module config to config/modules directory
add_custom_command(TARGET my_service_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/my_service_module.json
    COMMENT "Installing module configuration"
)

# Install
install(TARGETS my_service_module
    LIBRARY DESTINATION lib/cdmf/modules
    RUNTIME DESTINATION lib/cdmf/modules
)
```

### 7. Register Module in Main CMakeLists.txt

Add to `workspace/CMakeLists.txt`:

```cmake
# Add your service module
add_subdirectory(src/modules/my_service_module)
```

### 8. Build the Module

```bash
docker exec <container_name> bash -c "cd /workspace/build && cmake --build . --target my_service_module"
```

### 9. Using the Service in Another Module

**In consumer module:**

```cpp
#include "my_service.h"
#include "module/module_activator.h"

void ConsumerActivator::start(IModuleContext* context) {
    // Get service reference
    auto serviceRef = context->getServiceReference("cdmf::IMyService");
    if (serviceRef) {
        // Get service instance
        auto* myService = context->getService<cdmf::services::IMyService>(serviceRef);
        if (myService) {
            // Use service
            std::string result = myService->doOperation("test input");
            LOGI_FMT("Service result: " << result);
        }
    } else {
        LOGE("My Service not available");
    }
}
```

## Service Registration Properties

Common properties to include when registering a service:

```cpp
Properties props;
props.set("service.description", "Service description");
props.set("service.vendor", "Vendor name");
props.set("service.ranking", "100");           // Higher = higher priority
props.set("service.scope", "singleton");       // or "prototype"
props.set("service.version", "1.0.0");
```

## Service Lifecycle

1. **Module Start**
   - Activator's `start()` called
   - Service instance created
   - Service registered in service registry

2. **Service Usage**
   - Consumers lookup service by interface name
   - Get service instance via service reference
   - Call service methods

3. **Module Stop**
   - Activator's `stop()` called
   - Service unregistered
   - Service instance destroyed

## Service Tracking

To track service availability dynamically:

```cpp
#include "service/service_tracker.h"

class MyServiceTracker : public ServiceTrackerCustomizer {
public:
    void addingService(ServiceReference* ref, void* service) override {
        auto* myService = static_cast<cdmf::services::IMyService*>(service);
        LOGI("My Service became available");
        // Use service
    }

    void removedService(ServiceReference* ref, void* service) override {
        LOGI("My Service removed");
        // Cleanup
    }
};

// In module start:
tracker_ = std::make_unique<ServiceTracker>(
    context,
    "cdmf::IMyService",
    std::make_unique<MyServiceTracker>()
);
tracker_->open();
```

## Best Practices

1. **Thread Safety**: Always protect shared state with mutexes
2. **Resource Cleanup**: Unregister services before destroying them
3. **Error Handling**: Validate service availability before use
4. **Logging**: Log service lifecycle events
5. **Properties**: Use service properties for configuration
6. **Versioning**: Include version in service exports
7. **Dependencies**: Declare module dependencies in manifest
8. **Interface Stability**: Keep service interfaces stable across versions

## Testing the Service

Create a test module or unit test:

```cpp
#include <gtest/gtest.h>
#include "my_service_impl.h"

TEST(MyServiceTest, BasicOperation) {
    cdmf::services::MyServiceImpl service;
    service.start();

    std::string result = service.doOperation("test");
    EXPECT_EQ(result, "Processed: test");

    service.stop();
}

TEST(MyServiceTest, StatusCheck) {
    cdmf::services::MyServiceImpl service;
    EXPECT_EQ(service.getStatus(), "Stopped");

    service.start();
    EXPECT_EQ(service.getStatus(), "Running");

    service.stop();
    EXPECT_EQ(service.getStatus(), "Stopped");
}
```

## Troubleshooting

### Service Not Found
- **Issue**: `getServiceReference()` returns null
- **Check**:
  - Module is loaded and started
  - Service interface name matches exactly
  - Service registration succeeded

### Service Registration Failed
- **Issue**: `registerService()` returns null
- **Check**:
  - Service pointer is valid
  - Interface name is correct
  - No duplicate registrations

### Module Won't Load
- **Issue**: Module installation fails
- **Check**:
  - Library path is correct in manifest.json
  - Dependencies are available
  - Activator exports are defined

## Example: Configuration Service

See the built-in configuration service module for a complete example:
- Location: `workspace/src/modules/config_service_module/`
- Interface: `IConfigurationAdmin`
- Implementation: `ConfigurationAdmin`
- Activator: `ConfigServiceActivator`
