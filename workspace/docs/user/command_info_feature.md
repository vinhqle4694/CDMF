# Module Info Command - User Guide

## Overview

The `info` command provides detailed information about installed modules, including their metadata, provided APIs, dependencies, and runtime status.

## Usage

```bash
cdmf> info <module_name>
```

## Command Syntax

- **module_name**: The symbolic name of the module (e.g., "cdmf.config.service")

## Example Output

When you execute `info cdmf.config.service`, you'll see:

```
================================================================================
 Module Information
================================================================================

Symbolic Name: cdmf.config.service
Version:       1.0.0
Module ID:     1
State:         ACTIVE
Location:      /workspace/build/lib/config_service_module.so
Name:          Configuration Service Module
Description:   Provides configuration management services for CDMF applications
Vendor:        CDMF Project
Category:      framework-service

================================================================================
 Provided APIs (Exports)
================================================================================

  * cdmf::IConfigurationAdmin (v1.0.0)

================================================================================
 Registered Services (Runtime)
================================================================================

  1 service(s) registered
  (Use 'list' command to see detailed service information)

================================================================================
 Dependencies
================================================================================

  (No dependencies)

================================================================================
 Services In Use
================================================================================

  0 service(s) in use

================================================================================
 Module Properties
================================================================================

  * config.storage.dir = /workspace/build/config/store
  * config.auto.reload = true
  * config.persistence.enabled = true

================================================================================
```

## Information Sections

### 1. Module Information
Basic module identity and metadata:
- **Symbolic Name**: Unique module identifier
- **Version**: Semantic version (major.minor.patch)
- **Module ID**: Framework-assigned unique numeric ID
- **State**: Current lifecycle state (INSTALLED, RESOLVED, ACTIVE, etc.)
- **Location**: Path to the module's shared library file
- **Name**: Human-readable module name
- **Description**: Module description
- **Vendor**: Module author/organization
- **Category**: Module category/classification

### 2. Provided APIs (Exports)
Lists all interfaces/APIs that this module exports and makes available to other modules:
- Interface name (e.g., `cdmf::IConfigurationAdmin`)
- API version (if specified)

This section answers: **"What services does this module provide?"**

### 3. Registered Services (Runtime)
Shows how many services this module has registered at runtime. This indicates whether the module is actively providing its advertised services.

### 4. Dependencies
Lists all modules that this module depends on:
- Dependency symbolic name
- Version range requirement
- Optional flag (if applicable)

This section answers: **"What does this module need to function?"**

### 5. Services In Use
Shows how many services from other modules this module is currently using.

### 6. Module Properties
Module-specific configuration properties from the manifest:
- Property key-value pairs
- Configuration settings

## Error Messages

### Module Not Found
```
cdmf> info nonexistent_module
Module not found: nonexistent_module
```

### Missing Argument
```
cdmf> info
Usage: info <module_name>
```

## Use Cases

### 1. Discovering Available APIs
Before writing code that uses a module, check what APIs it provides:
```bash
cdmf> info cdmf.config.service
```
Look at the **Provided APIs** section to see available interfaces.

### 2. Troubleshooting Dependencies
If a module fails to start, check its dependencies:
```bash
cdmf> info my.custom.module
```
Look at the **Dependencies** section to ensure all required modules are installed.

### 3. Verifying Module Status
Check if a module is active and providing services:
```bash
cdmf> info cdmf.logging.service
```
Look at the **State** field and **Registered Services** count.

### 4. Understanding Module Configuration
View module-specific properties:
```bash
cdmf> info cdmf.config.service
```
Look at the **Module Properties** section.

## Related Commands

- `list` - List all active modules
- `start <module_name>` - Start a module
- `stop <module_name>` - Stop a module
- `help` - Show all available commands

## Tips

1. **Tab Completion** (if supported): Start typing a module name and press Tab to auto-complete
2. **Case Sensitivity**: Module names are case-sensitive
3. **Copy Module Names**: Use the `list` command output to get exact module names for the `info` command
4. **API Discovery**: The "Provided APIs" section is especially useful when developing new modules

## Technical Notes

### Information Sources

The `info` command gathers information from multiple sources:

1. **Module Manifest (JSON)**: Static metadata defined when the module was created
   - Module identity, version, name, description, vendor
   - Exported APIs
   - Dependencies
   - Properties

2. **Runtime State**: Dynamic information from the running framework
   - Current module state
   - Registered services count
   - Services in use count

### Module States

Possible module states you might see:
- **INSTALLED**: Module loaded but not yet resolved
- **RESOLVED**: Dependencies satisfied, ready to start
- **STARTING**: Module is starting up
- **ACTIVE**: Module fully operational
- **STOPPING**: Module is shutting down
- **UNINSTALLED**: Module removed (you won't see this via `info`)

## Examples

### Example 1: Checking a Framework Service Module
```bash
cdmf> info cdmf.config.service
# Shows configuration service details and APIs
```

### Example 2: Verifying Custom Module Dependencies
```bash
cdmf> info com.mycompany.analytics
# Check what this module depends on and provides
```

### Example 3: API Discovery for Integration
```bash
cdmf> info cdmf.logging.service
# Discover logging APIs before integrating logging into your module
```

## See Also

- [Module Development Guide](../development/module_development.md)
- [Service Registry Documentation](../api/service_registry.md)
- [Command Line Interface Reference](cli_reference.md)
