# Service Development Guide

## Overview

Services in CDMF are registered components that provide functionality to other modules through a service registry pattern.

**Basic Flow:**
```
Service Module → Registers Service → Service Registry → Consumer Module Looks Up Service
```

## Quick Start: 5 Essential Steps

1. **Define Interface** - Pure virtual interface class
2. **Implement Service** - Concrete class implementing the interface
3. **Create Activator** - Module lifecycle management
4. **Configure Manifest** - Module metadata (manifest.json)
5. **Build & Register** - Add to CMakeLists.txt

## Detailed Steps

### 1. Create Module Directory

```
workspace/src/modules/my_service_module/
├── my_service.h              # Service interface (pure virtual)
├── my_service_impl.h/cpp     # Implementation
├── my_service_activator.h/cpp # Module lifecycle
├── manifest.json             # Module metadata
└── CMakeLists.txt            # Build config
```

### 2. Define Service Interface

**File:** `my_service.h` - Pure virtual interface

```cpp
#pragma once
#include <string>

namespace cdmf::services {

class IMyService {
public:
    virtual ~IMyService() = default;
    virtual std::string doOperation(const std::string& input) = 0;
    virtual std::string getStatus() const = 0;
};

}
```

### 3. Implement Service

```cpp
#pragma once
#include "my_service.h"
#include <mutex>

namespace cdmf::services {

class MyServiceImpl : public IMyService {
public:
    void start();
    void stop();

    // IMyService interface
    std::string doOperation(const std::string& input) override;
    std::string getStatus() const override;

private:
    mutable std::mutex mutex_;
    bool running_ = false;
};

}
```

```cpp
#include "my_service_impl.h"
#include "utils/log.h"

namespace cdmf::services {

void MyServiceImpl::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    LOGI("MyService started");
}

void MyServiceImpl::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    LOGI("MyService stopped");
}

std::string MyServiceImpl::doOperation(const std::string& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) throw std::runtime_error("Service not running");
    return "Processed: " + input;
}

std::string MyServiceImpl::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ ? "Running" : "Stopped";
}

}
```

### 4. Create Module Activator

**Key Points:** Manages service lifecycle (creation, registration, cleanup)

**my_service_activator.h:**
```cpp
#pragma once
#include "module/module_activator.h"
#include "my_service_impl.h"

namespace cdmf::modules {

class MyServiceActivator : public IModuleActivator {
public:
    void start(IModuleContext* context) override;
    void stop(IModuleContext* context) override;

private:
    std::unique_ptr<services::MyServiceImpl> service_;
    ServiceRegistration* registration_ = nullptr;
};

}

extern "C" {
    cdmf::IModuleActivator* createModuleActivator();
    void destroyModuleActivator(cdmf::IModuleActivator* activator);
}
```

```cpp
#include "my_service_activator.h"
#include "utils/log.h"

namespace cdmf::modules {

void MyServiceActivator::start(IModuleContext* context) {
    // 1. Create and start service
    service_ = std::make_unique<services::MyServiceImpl>();
    service_->start();

    // 2. Register service with properties
    Properties props;
    props.set("service.description", "My custom service");
    props.set("service.vendor", "CDMF");

    registration_ = context->registerService("cdmf::IMyService", service_.get(), props);
    LOGI("My Service Module started");
}

void MyServiceActivator::stop(IModuleContext* context) {
    // 1. Unregister service
    if (registration_) {
        registration_->unregister();
        registration_ = nullptr;
    }

    // 2. Stop and cleanup
    if (service_) {
        service_->stop();
        service_.reset();
    }
    LOGI("My Service Module stopped");
}

}

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

**manifest.json:** Module metadata and configuration

```json
{
  "module": {
    "symbolic-name": "cdmf.myservice",
    "name": "My Service Module",
    "version": "1.0.0",
    "library": "my_service_module.so"
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
  }
}
```

### 6. Create CMakeLists.txt

```cmake
add_library(my_service_module SHARED
    my_service_impl.cpp
    my_service_activator.cpp
)

target_include_directories(my_service_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/framework/include
)

target_link_libraries(my_service_module PRIVATE
    cdmf_core cdmf_module cdmf_service
)

set_target_properties(my_service_module PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy manifest to config directory
add_custom_command(TARGET my_service_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/my_service_module.json
)
```

### 7. Register in Main CMakeLists.txt

Add to `workspace/CMakeLists.txt`:
```cmake
add_subdirectory(src/modules/my_service_module)
```

### 8. Build

```bash
docker exec <container> bash -c "cd /workspace/build && cmake --build . --target my_service_module"
```

### 9. Using the Service (Consumer Module)

```cpp
void ConsumerActivator::start(IModuleContext* context) {
    auto serviceRef = context->getServiceReference("cdmf::IMyService");
    if (serviceRef) {
        auto* myService = context->getService<cdmf::services::IMyService>(serviceRef);
        if (myService) {
            std::string result = myService->doOperation("test input");
            LOGI_FMT("Service result: " << result);
        }
    }
}
```

## Service Lifecycle

**Module Start** → Create Service → Register → **Service Available** → Consumers Use → **Module Stop** → Unregister → Destroy

## Service Tracking (Optional)

For dynamic service availability tracking:

```cpp
class MyServiceTracker : public ServiceTrackerCustomizer {
public:
    void addingService(ServiceReference* ref, void* service) override {
        // Service available - use it
    }
    void removedService(ServiceReference* ref, void* service) override {
        // Service removed - cleanup
    }
};

// In module start:
tracker_ = std::make_unique<ServiceTracker>(context, "cdmf::IMyService",
                                            std::make_unique<MyServiceTracker>());
tracker_->open();
```

## Best Practices

1. **Thread Safety** - Use mutexes for shared state
2. **Cleanup Order** - Unregister → Stop → Destroy
3. **Error Handling** - Always check service availability
4. **Logging** - Log lifecycle events (start/stop/errors)
5. **Interface Stability** - Keep interfaces backward compatible

## Common Service Properties

```cpp
Properties props;
props.set("service.description", "Description");
props.set("service.vendor", "Vendor");
props.set("service.ranking", "100");      // Higher = higher priority
props.set("service.scope", "singleton");  // or "prototype"
```

## Troubleshooting

| Issue | Check |
|-------|-------|
| **Service Not Found** | Module loaded? Interface name exact match? |
| **Registration Failed** | Valid service pointer? Correct interface name? |
| **Module Won't Load** | Library path in manifest.json? Dependencies available? |

## Reference Example

See `workspace/src/modules/config_service_module/` for a complete implementation example.
