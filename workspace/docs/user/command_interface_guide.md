# CDMF Interactive Command Interface User Guide

## Overview

The CDMF (Component-based Distributed Modular Framework) provides an interactive command-line interface for managing modules at runtime. This allows you to start, stop, update, and monitor modules without restarting the framework.

## Quick Start

### Running the Framework

```bash
# Navigate to workspace directory
cd /workspace

# Run the framework
./scripts/run.sh
```

The framework will start and present an interactive command prompt:

```
CDMF Interactive Command Interface
Type 'help' for available commands, 'exit' to quit.

cdmf>
```

### Basic Usage

All terminal output is automatically logged to `/workspace/logs/cdmf.log` for troubleshooting and auditing.

## Available Commands

### 1. `list` - List Running Modules

Display all currently active modules with their details.

**Syntax:**
```
list
```

**Example:**
```
cdmf> list
Running modules (1):
  - cdmf.config.service (v1.0.0) [ID: 1] [State: ACTIVE]
```

**Output Fields:**
- **Module Name**: Symbolic name of the module
- **Version**: Semantic version number
- **ID**: Unique module identifier assigned by framework
- **State**: Current lifecycle state (ACTIVE, RESOLVED, etc.)

---

### 2. `start` - Start a Module

Start an installed module that is not currently running.

**Syntax:**
```
start <module_name>
```

**Parameters:**
- `<module_name>`: The symbolic name of the module to start

**Example:**
```
cdmf> start cdmf.config.service
Module 'cdmf.config.service' started successfully
```

**Notes:**
- Module must be in INSTALLED or RESOLVED state
- Module dependencies must be satisfied
- If module is already ACTIVE, command returns success without action

**Error Cases:**
- Module not found: `Module not found: <name>`
- Invalid state: `Module cannot be started from current state: <state>`
- Start failure: `Failed to start module: <error details>`

---

### 3. `stop` - Stop a Module

Stop a running module.

**Syntax:**
```
stop <module_name>
```

**Parameters:**
- `<module_name>`: The symbolic name of the module to stop

**Example:**
```
cdmf> stop cdmf.config.service
Module 'cdmf.config.service' stopped successfully
```

**Notes:**
- Module must be in ACTIVE state
- All registered services will be unregistered
- Module activator's `stop()` method will be called
- If module is already stopped, command returns success without action

**Error Cases:**
- Module not found: `Module not found: <name>`
- Not running: `Module is not running`
- Stop failure: `Failed to stop module: <error details>`

---

### 4. `update` - Update a Module

Update an existing module to a new version.

**Syntax:**
```
update <module_name> <path>
```

**Parameters:**
- `<module_name>`: The symbolic name of the module to update
- `<path>`: Path to the new module shared library (.so, .dll, .dylib)

**Example:**
```
cdmf> update cdmf.config.service /workspace/build/lib/config_service_module_v2.so
Module 'cdmf.config.service' updated successfully to version 2.0.0
```

**Process:**
1. Module is stopped if currently ACTIVE
2. Old shared library is unloaded
3. New shared library is loaded from specified path
4. New manifest is parsed
5. Dependencies are re-resolved
6. Module is started if auto-start is enabled

**Error Cases:**
- Module not found: `Module not found: <name>`
- Invalid path: `Failed to update module: <error details>`
- Version conflict: Details in error message

---

### 5. `help` - Show Help

Display all available commands with brief descriptions.

**Syntax:**
```
help
```

**Example:**
```
cdmf> help
Available commands:
  start <module_name>           - Start a module
  stop <module_name>            - Stop a module
  update <module_name> <path>   - Update a module to a new version
  list                          - List all running modules
  help                          - Show this help message
  exit                          - Exit the command interface
```

---

### 6. `exit` - Exit Framework

Gracefully shutdown the framework and exit the command interface.

**Syntax:**
```
exit
```

**Example:**
```
cdmf> exit
Exiting...
Exiting command interface.
[INFO] Stopping framework...
[INFO] Framework shutdown complete
```

**Shutdown Process:**
1. All active modules are stopped (in reverse dependency order)
2. All services are unregistered
3. Event dispatcher is stopped
4. All resources are released
5. Framework transitions to STOPPED state

---

## Script Options

The `run.sh` script supports several command-line options:

### Show Help
```bash
./scripts/run.sh --help
```

### Use Custom Configuration
```bash
./scripts/run.sh --config /path/to/custom/framework.json
```

### Run with Debug Output
```bash
./scripts/run.sh --debug
```

---

## Log Files

All framework output is logged to:
```
/workspace/logs/cdmf.log
```

The log file includes:
- Framework initialization messages
- Module installation and lifecycle events
- Service registration/unregistration events
- Command execution results
- Error messages and stack traces

**Viewing Logs:**
```bash
# View entire log
cat /workspace/logs/cdmf.log

# Follow log in real-time (in another terminal)
tail -f /workspace/logs/cdmf.log

# View last 50 lines
tail -50 /workspace/logs/cdmf.log
```

---

## Typical Workflows

### Workflow 1: Check Running Modules
```
cdmf> list
Running modules (1):
  - cdmf.config.service (v1.0.0) [ID: 1] [State: ACTIVE]
```

### Workflow 2: Restart a Module
```
cdmf> stop cdmf.config.service
Module 'cdmf.config.service' stopped successfully

cdmf> start cdmf.config.service
Module 'cdmf.config.service' started successfully
```

### Workflow 3: Update a Module to New Version
```
cdmf> list
Running modules (1):
  - cdmf.config.service (v1.0.0) [ID: 1] [State: ACTIVE]

cdmf> update cdmf.config.service /workspace/build/lib/config_service_module_v2.so
Module 'cdmf.config.service' updated successfully to version 2.0.0

cdmf> list
Running modules (1):
  - cdmf.config.service (v2.0.0) [ID: 1] [State: ACTIVE]
```

### Workflow 4: Graceful Shutdown
```
cdmf> exit
Exiting...
Exiting command interface.
[Framework performs graceful shutdown...]
```

---

## Troubleshooting

### Command Not Recognized
**Problem:** `Unknown command: <command>`

**Solution:** Type `help` to see available commands. Check spelling and syntax.

### Module Not Found
**Problem:** `Module not found: <module_name>`

**Solution:**
1. Use `list` to see available modules
2. Check module symbolic name (case-sensitive)
3. Verify module is installed in framework

### Module Won't Start
**Problem:** `Failed to start module: <error>`

**Solution:**
1. Check log file for detailed error: `tail -50 /workspace/logs/cdmf.log`
2. Verify all module dependencies are satisfied
3. Check module is in RESOLVED or INSTALLED state
4. Review module activator implementation

### Log File Too Large
**Problem:** Log file grows too large over time

**Solution:**
```bash
# Archive old logs
mv /workspace/logs/cdmf.log /workspace/logs/cdmf.log.$(date +%Y%m%d_%H%M%S)

# Or clear log
> /workspace/logs/cdmf.log
```

---

## Environment Variables

The following environment variables can be set to customize framework behavior:

| Variable | Description | Default |
|----------|-------------|---------|
| `CDMF_CONFIG` | Path to framework configuration file | `./config/framework.json` |
| `LD_LIBRARY_PATH` | Shared library search path | `./build/lib` |

**Example:**
```bash
export CDMF_CONFIG=/custom/path/framework.json
./scripts/run.sh
```

---

## Advanced Usage

### Running in Docker Container

```bash
# From host machine
docker exec -it <container_name> bash -c "cd /workspace && ./scripts/run.sh"
```

### Piping Commands (Non-Interactive)

```bash
# Execute commands from stdin
echo -e 'list\nexit' | ./scripts/run.sh

# Execute commands from file
cat commands.txt | ./scripts/run.sh
```

### Background Execution with Screen/tmux

```bash
# Using screen
screen -S cdmf
./scripts/run.sh
# Press Ctrl+A, D to detach

# Reattach later
screen -r cdmf

# Using tmux
tmux new -s cdmf
./scripts/run.sh
# Press Ctrl+B, D to detach

# Reattach later
tmux attach -t cdmf
```

---

## Best Practices

1. **Always use `list` before stopping modules** to verify module names and states
2. **Check logs after errors** for detailed diagnostics
3. **Use `exit` command** for graceful shutdown (not Ctrl+C)
4. **Archive logs periodically** to prevent disk space issues
5. **Test module updates in development** before production deployment
6. **Document module dependencies** to avoid start failures

---

## Module States Reference

| State | Description | Can Start? | Can Stop? |
|-------|-------------|------------|-----------|
| **INSTALLED** | Loaded, manifest parsed | Yes | No |
| **RESOLVED** | Dependencies satisfied | Yes | No |
| **STARTING** | In process of starting | No | No |
| **ACTIVE** | Fully operational | No | Yes |
| **STOPPING** | In process of stopping | No | No |
| **UNINSTALLED** | Removed from framework | No | No |

---

## Support

For issues or questions:
- Check logs: `/workspace/logs/cdmf.log`
- Review framework documentation: `/workspace/docs/`
- Check build documentation: `/workspace/docs/devops/build_and_verify.md`

---

**Last Updated:** 2025-10-04
**Framework Version:** 1.0.0
