# CDMF Build and Verification Guide

## Prerequisites
- Docker container running with `/workspace` mounted
- Replace `<container_name>` with your actual Docker container name

## Build Commands

### Full Build
```bash
docker exec <container_name> bash -c "cd /workspace/build && cmake --build ."
```

### Build Specific Targets
```bash
# Framework only
docker exec <container_name> bash -c "cd /workspace/build && cmake --build . --target cdmf_main"

# Config service module
docker exec <container_name> bash -c "cd /workspace/build && cmake --build . --target config_service_module"

# All tests
docker exec <container_name> bash -c "cd /workspace/build && cmake --build . --target all"
```

### Clean Build
```bash
docker exec <container_name> bash -c "rm -rf /workspace/build/* && cd /workspace/build && cmake .. && cmake --build ."
```

## Run Commands

### Run Framework from Workspace Root
```bash
docker exec <container_name> bash -c "cd /workspace && ./build/bin/cdmf"
```

### Run Framework from Build Directory
```bash
docker exec <container_name> bash -c "cd /workspace/build && ./bin/cdmf"
```

### Run with Timeout (for testing)
```bash
docker exec <container_name> bash -c "cd /workspace && timeout 5 ./build/bin/cdmf"
```

## Test Commands

### Run All Tests
```bash
docker exec <container_name> bash -c "cd /workspace/build && ctest --output-on-failure"
```

### Run Specific Test
```bash
docker exec <container_name> bash -c "cd /workspace/build && ./bin/test_configuration"
```

## Verification Commands

### Check Build Structure
```bash
# List libraries
docker exec <container_name> ls -la /workspace/build/lib/

# List module configs
docker exec <container_name> ls -la /workspace/build/config/modules/

# List executables
docker exec <container_name> ls -la /workspace/build/bin/
```

### Verify Module Loading
```bash
# Run framework and check logs for module installation
docker exec <container_name> bash -c "cd /workspace && timeout 5 ./build/bin/cdmf 2>&1 | grep -E '(Module installed|Module started)'"
```

### Check Running Processes
```bash
docker exec <container_name> ps aux | grep cdmf
```

## Directory Structure

### Build Output
```
/workspace/build/
├── bin/
│   ├── cdmf                          # Main executable
│   └── test_*                        # Test executables
├── lib/
│   ├── config_service_module.so      # Module library
│   ├── libcdmf_core.so
│   ├── libcdmf_foundation.so
│   ├── libcdmf_ipc.so
│   ├── libcdmf_module.so
│   ├── libcdmf_platform.so
│   ├── libcdmf_security.so
│   ├── libcdmf_service.so
│   └── libcdmf.so                    # All-in-one library
└── config/
    ├── framework.json                # Framework configuration
    └── modules/
        └── config_service_module.json # Module configuration
```

### Source Structure
```
/workspace/
├── src/
│   ├── framework/
│   │   ├── include/                  # Framework headers
│   │   ├── impl/                     # Framework implementation
│   │   ├── ipc/                      # IPC implementation
│   │   ├── main.cpp                  # Main entry point
│   │   └── CMakeLists.txt            # Framework build
│   └── modules/
│       └── config_service_module/    # Configuration service module
│           ├── *.cpp                 # Module implementation
│           ├── manifest.json         # Module manifest
│           └── CMakeLists.txt        # Module build
├── config/
│   └── framework.json                # Framework configuration (source)
├── tests/
│   ├── *.cpp                         # Test sources
│   └── CMakeLists.txt                # Tests build
└── build/                            # Build output (generated)
```

## Configuration Files

### framework.json Location
- Source: `/workspace/config/framework.json`
- Build: `/workspace/build/config/framework.json` (copied during build)

### Key Configuration Fields
```json
{
  "modules": {
    "config_path": "/workspace/build/config",     // Module configs location
    "lib_path": "/workspace/build/lib"            // Module libraries location
  }
}
```

## Common Issues

### Module Not Found
- **Symptom**: "Found 0 module config(s)"
- **Check**: Verify `config_path` in framework.json points to correct directory
- **Fix**: Update `modules.config_path` to absolute path

### Library Not Found
- **Symptom**: "cannot open shared object file"
- **Check**: Verify `lib_path` in framework.json and library exists
- **Fix**: Update `modules.lib_path` to absolute path

### Framework Config Not Found
- **Symptom**: "Configuration file not found: ./config/framework.json"
- **Solution**: Run from `/workspace` or `/workspace/build` directory

## Quick Commands Reference

```bash
# Build everything
docker exec <container_name> bash -c "cd /workspace/build && cmake --build ."

# Run framework
docker exec <container_name> bash -c "cd /workspace && ./build/bin/cdmf"

# Run tests
docker exec <container_name> bash -c "cd /workspace/build && ctest --output-on-failure"

# Check module loading
docker exec <container_name> bash -c "cd /workspace && timeout 5 ./build/bin/cdmf 2>&1 | grep 'Module installed'"
```

## Finding Your Container Name

```bash
# List all running containers
docker ps

# List all containers (including stopped)
docker ps -a

# Find container by image name
docker ps --filter ancestor=<image_name>
```
