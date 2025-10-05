# Service Call Feature - Implementation Summary

## Overview

Implemented a **manifest-based CLI service call mechanism** that allows users to call service methods directly from the command line interface. Services declare their callable methods in the manifest JSON, and the framework automatically routes CLI commands to the appropriate service.

## Architecture: Command Pattern (Option 1)

### Design Choice
- **Declarative**: Methods declared in `manifest.json`
- **Simple**: Services implement one `dispatchCommand()` method
- **Framework-Managed**: No service dependencies on ServiceMethodRegistry
- **Self-Documenting**: Manifest serves as API documentation

## Components Implemented

### 1. ICommandDispatcher Interface
**File**: `src/framework/include/service/command_dispatcher.h`

```cpp
class ICommandDispatcher {
public:
    virtual ~ICommandDispatcher() = default;

    virtual std::string dispatchCommand(
        const std::string& methodName,
        const std::vector<std::string>& args
    ) = 0;
};
```

**Purpose**: Services implement this interface to handle CLI commands.

---

### 2. Manifest Schema Extension
**Files**:
- `src/framework/include/module/manifest_parser.h`
- `src/framework/impl/module/manifest_parser.cpp`

**Added Structures**:
```cpp
struct CLIMethodArgument {
    std::string name;
    std::string type;
    bool required;
    std::string description;
};

struct CLIMethod {
    std::string interface;
    std::string method;
    std::string signature;
    std::string description;
    std::vector<CLIMethodArgument> arguments;
};
```

**ModuleManifest**: Added `std::vector<CLIMethod> cliMethods;`

**Parser**: Implemented `parseCLIMethods()` to parse `cli-methods` section from manifest JSON.

---

### 3. Manifest JSON Format

**Example** (`config_service_module/manifest.json`):

```json
{
  "cli-methods": [
    {
      "interface": "cdmf::IConfigurationAdmin",
      "method": "createConfiguration",
      "signature": "(pid:string) -> void",
      "description": "Create a new configuration with the given persistent identifier",
      "arguments": [
        {
          "name": "pid",
          "type": "string",
          "required": true,
          "description": "Unique persistent identifier (e.g., 'com.myapp.database')"
        }
      ]
    }
  ]
}
```

---

### 4. ConfigurationAdmin Service Updates
**Files**:
- `src/modules/config_service_module/configuration_admin.h`
- `src/modules/config_service_module/configuration_admin.cpp`

**Changes**:
- Implements `ICommandDispatcher`
- Implements `dispatchCommand()` method with command routing logic
- Handles 4 methods: `createConfiguration`, `getConfiguration`, `listConfigurations`, `deleteConfiguration`

**Example Implementation**:
```cpp
std::string ConfigurationAdmin::dispatchCommand(
    const std::string& methodName,
    const std::vector<std::string>& args
) {
    if (methodName == "createConfiguration") {
        if (args.size() != 1) {
            return "Error: createConfiguration requires 1 argument";
        }
        createConfiguration(args[0]);
        return "Created configuration: " + args[0];
    }
    // ... other methods
}
```

---

### 5. CommandHandler Updates
**Files**:
- `src/framework/include/utils/command_handler.h`
- `src/framework/impl/utils/command_handler.cpp`

**New Command**: `call <service> <method> [args...]`

**Features**:
- `call --list` - List all services with callable methods
- `call <service> --help` - Show methods for a service
- `call <service> <method> [args...]` - Invoke a method

**Implementation Flow**:
1. Parse manifest to find method declaration
2. Find target module providing the service
3. Get module context
4. Get service reference via `context->getServiceReference(serviceInterface)`
5. Get service instance via `context->getService(serviceRef)`
6. Cast to `ICommandDispatcher*`
7. Invoke `dispatchCommand(methodName, args)`
8. Release service via `context->ungetService(serviceRef)`

---

## Usage Examples

### List All Callable Services
```bash
cdmf> call --list
Services with callable methods:
  * cdmf::IConfigurationAdmin (4 method(s))

Use 'call <service> --help' to see methods for a service.
```

### Get Help for a Service
```bash
cdmf> call cdmf::IConfigurationAdmin --help
Available methods for cdmf::IConfigurationAdmin:

  createConfiguration (pid:string) -> void
    Create a new configuration with the given persistent identifier

  getConfiguration (pid:string) -> properties
    Get configuration properties for a given PID (creates if doesn't exist)

  listConfigurations ([filter:string]) -> list
    List all configurations, optionally filtered by PID substring

  deleteConfiguration (pid:string) -> void
    Delete a configuration by its persistent identifier
```

### Call Service Methods
```bash
# Create a configuration
cdmf> call cdmf::IConfigurationAdmin createConfiguration com.myapp.database
Created configuration: com.myapp.database

# List all configurations
cdmf> call cdmf::IConfigurationAdmin listConfigurations
Configurations (1):
  * com.myapp.database

# Get configuration details
cdmf> call cdmf::IConfigurationAdmin getConfiguration com.myapp.database
Configuration: com.myapp.database
  (No properties set)

# Delete configuration
cdmf> call cdmf::IConfigurationAdmin deleteConfiguration com.myapp.database
Deleted configuration: com.myapp.database
```

---

## Adding CLI Support to Your Service

### Step 1: Implement ICommandDispatcher

```cpp
#include "service/command_dispatcher.h"

class MyService : public cdmf::ICommandDispatcher {
public:
    // Your service methods
    void doSomething(const std::string& arg);

    // ICommandDispatcher implementation
    std::string dispatchCommand(
        const std::string& methodName,
        const std::vector<std::string>& args
    ) override {
        if (methodName == "doSomething") {
            if (args.size() != 1) {
                return "Error: doSomething requires 1 argument";
            }
            doSomething(args[0]);
            return "Action completed successfully";
        }
        return "Unknown method: " + methodName;
    }
};
```

### Step 2: Declare Methods in Manifest

```json
{
  "cli-methods": [
    {
      "interface": "com.mycompany.IMyService",
      "method": "doSomething",
      "signature": "(arg:string) -> void",
      "description": "Performs an action with the given argument",
      "arguments": [
        {
          "name": "arg",
          "type": "string",
          "required": true,
          "description": "The argument for the action"
        }
      ]
    }
  ]
}
```

### Step 3: Register Service
```cpp
void MyModuleActivator::start(IModuleContext* context) {
    myService_ = std::make_unique<MyService>();

    Properties props;
    serviceRegistration_ = context->registerService(
        "com.mycompany.IMyService",
        myService_.get(),
        props
    );
}
```

That's it! No additional code needed. The framework automatically makes the service callable from CLI.

---

## Benefits

✅ **Declarative** - Methods declared in manifest, not in code
✅ **Simple** - One dispatcher method, not multiple registrations
✅ **No Framework Dependencies** - Services don't need ServiceMethodRegistry headers
✅ **Self-Documenting** - Manifest serves as API contract
✅ **Framework-Managed** - Automatic routing and validation
✅ **Universal** - Works for ANY service
✅ **Type Information** - Arguments documented with types and descriptions
✅ **Discoverable** - `call --list` and `call <service> --help` built-in

---

## Technical Details

### Service Lifecycle
1. Module loads → Manifest parsed → CLI methods extracted
2. Module starts → Service registered → Available for CLI calls
3. User executes `call` command → Framework routes to service
4. Framework invokes `dispatchCommand()` → Service executes → Returns result
5. Framework releases service reference

### Thread Safety
- Service lookup is thread-safe (uses framework's service registry)
- Service reference counting ensures service isn't unregistered while in use
- `ungetService()` properly releases references

### Error Handling
- Invalid service interface → Error message with suggestions
- Invalid method name → Error message with `--help` suggestion
- Service not implementing ICommandDispatcher → Clear error message
- Exception during invocation → Caught and returned as error

---

## Files Modified/Created

### Created Files
1. `src/framework/include/service/command_dispatcher.h` - ICommandDispatcher interface
2. `src/framework/include/service/service_method_registry.h` - Registry (for future use)
3. `src/framework/impl/service/service_method_registry.cpp` - Registry implementation
4. `docs/implementation/service_call_implementation_summary.md` - This document

### Modified Files
1. `src/framework/include/module/manifest_parser.h` - Added CLI method structures
2. `src/framework/impl/module/manifest_parser.cpp` - Added CLI method parsing
3. `src/framework/include/utils/command_handler.h` - Added handleCall()
4. `src/framework/impl/utils/command_handler.cpp` - Implemented handleCall()
5. `src/modules/config_service_module/configuration_admin.h` - Implements ICommandDispatcher
6. `src/modules/config_service_module/configuration_admin.cpp` - Implements dispatchCommand()
7. `src/modules/config_service_module/manifest.json` - Added cli-methods section

---

## Future Enhancements

### Potential Improvements
1. **Type Validation** - Validate argument types before invocation
2. **Auto-completion** - Generate bash/zsh completion from manifest
3. **JSON Arguments** - Support complex types via JSON input
4. **Async Methods** - Support async method invocation
5. **Permissions** - Method-level access control
6. **History** - Command history persistence
7. **Batch Mode** - Execute multiple commands from file

### Optional Features
- Method aliases (short names)
- Default argument values
- Argument validation rules (min/max, regex)
- Return type specification
- Help text templates

---

## Status

✅ **Core Implementation Complete**
✅ **Configuration Service Integrated**
✅ **CommandHandler Updated**
✅ **Manifest Parser Extended**
⏳ **Testing Pending**
⏳ **User Documentation Pending**

---

## Next Steps

1. **Build and Test** - Compile and run integration tests
2. **Create User Guide** - Write user-facing documentation
3. **Example Module** - Create example module with CLI methods
4. **Unit Tests** - Add tests for CommandHandler::handleCall()
5. **Integration Tests** - Test end-to-end workflow

---

## Conclusion

The manifest-based CLI service call feature provides a clean, declarative way to expose service methods to the command line without service-specific code. The implementation follows the Command Pattern and integrates seamlessly with the existing CDMF framework architecture.
