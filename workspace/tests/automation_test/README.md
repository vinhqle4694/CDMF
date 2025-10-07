# CDMF Automation Testing System

## Overview

The CDMF Automation Testing System provides a comprehensive framework for testing the CDMF (Component-Driven Modular Framework) process through automated test cases. The system runs the CDMF process, captures logs, and verifies correct behavior through log analysis and configuration validation.

## Architecture

```
automation_test/
├── src/                          # Testing system source code
│   ├── automation_manager.h      # Process lifecycle management
│   ├── automation_manager.cpp
│   ├── log_analyzer.h            # Log file analysis
│   ├── log_analyzer.cpp
│   ├── config_analyzer.h         # Configuration validation
│   └── config_analyzer.cpp
├── testcases/                    # Test cases using Google Test
│   └── test_automation_system.cpp
├── CMakeLists.txt                # Build configuration
└── README.md                     # This file
```

## Components

### 1. AutomationManager

**Purpose**: Manages the CDMF process lifecycle for testing

**Key Features**:
- Start CDMF process with custom configuration
- Monitor process status (running, stopped, crashed)
- Capture stdout/stderr to log file
- Stop process gracefully or forcefully
- Wait for process exit with timeout
- Clean up test artifacts

**Usage Example**:
```cpp
ProcessConfig config;
config.executable_path = "../bin/cdmf";
config.log_file = "./logs/test.log";

AutomationManager manager(config);
manager.start();                    // Start CDMF
std::this_thread::sleep_for(2s);    // Wait for initialization
manager.stop();                     // Stop gracefully
```

### 2. LogAnalyzer

**Purpose**: Parse and analyze CDMF log files

**Key Features**:
- Load and parse log files line by line
- Search for specific patterns (plain text or regex)
- Filter by log level (VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL)
- Count occurrences of patterns
- Extract captured groups from regex matches
- Verify log sequence ordering
- Get entries between two patterns

**Usage Example**:
```cpp
LogAnalyzer analyzer("./logs/cdmf.log");
analyzer.load();

// Check if framework started
bool started = analyzer.containsPattern("Framework started successfully");

// Count errors
int errors = analyzer.countLogLevel(LogLevel::ERROR);

// Verify sequence
std::vector<std::string> sequence = {
    "Creating framework instance",
    "Initializing framework",
    "Starting framework"
};
bool correct_order = analyzer.verifySequence(sequence);
```

### 3. ConfigAnalyzer

**Purpose**: Validate CDMF configuration files

**Key Features**:
- Load and parse JSON configuration files
- Get configuration values (string, int, bool, double)
- Verify configuration against expected values
- Validate configuration schema
- Check for missing or invalid fields
- Validate framework, module, IPC, and logging settings

**Usage Example**:
```cpp
ConfigAnalyzer analyzer("./config/framework.json");
analyzer.load();

// Get configuration values
std::string framework_id = analyzer.getString("framework.id");
int thread_pool_size = analyzer.getInt("event.thread_pool_size");
bool ipc_enabled = analyzer.getBool("ipc.enabled");

// Validate configuration
auto result = analyzer.validate();
if (!result.valid) {
    for (const auto& error : result.errors) {
        std::cerr << "Error: " << error << std::endl;
    }
}
```

## Building the Tests

### Prerequisites
- CMake 3.16 or higher
- Google Test library
- C++17 compiler
- nlohmann/json library (included in project)

### Build Steps

```bash
# From workspace/build directory
cd workspace/build

# Configure CMake with tests enabled
cmake ..

# Build the automation tests
make automation_system_test

# Or build everything
make
```

## Running the Tests

### Run all automation tests:
```bash
cd workspace/build/tests/automation_test
./automation_system_test
```

### Run with verbose output:
```bash
./automation_system_test --gtest_verbose
```

### Run specific test:
```bash
./automation_system_test --gtest_filter=AutomationSystemTest.BasicSystemTest
```

### Run with CTest:
```bash
cd workspace/build
ctest -R AutomationSystemTest -V
```

## Test Output

### Log Files
Test logs are saved to: `workspace/build/tests/automation_test/logs/`

Example log file: `test_automation.log`

### Test Results
Google Test outputs results to console with:
- Test name
- Pass/Fail status
- Assertion details
- Execution time

Example output:
```
=== Testing Automation System ===

1. Testing ConfigAnalyzer...
   Framework ID: cdmf-main

2. Testing AutomationManager - Starting CDMF...
   CDMF process started, PID: 12345
   Waiting for framework initialization (3 seconds)...
   Process status: RUNNING ✓

3. Testing LogAnalyzer - Analyzing logs...
   Total log entries: 45
   Found 'Framework started successfully': YES ✓
   Error count in log: 0

4. Stopping CDMF...
   Process stopped gracefully
   Exit code: 0

5. Verifying shutdown in log...
   Found 'Stopping framework': YES ✓

=== Automation System Test Completed ===

[       OK ] AutomationSystemTest.BasicSystemTest (5123 ms)
```

## Creating New Test Cases

To create a new test case:

1. Create a new `.cpp` file in `testcases/` directory
2. Include the testing system headers:
```cpp
#include "../src/automation_manager.h"
#include "../src/log_analyzer.h"
#include "../src/config_analyzer.h"
```

3. Write your test using Google Test macros:
```cpp
TEST(MyTestSuite, MyTestCase) {
    // Setup
    ProcessConfig config;
    config.executable_path = "../bin/cdmf";
    config.log_file = "./logs/my_test.log";

    // Execute
    AutomationManager manager(config);
    ASSERT_TRUE(manager.start());

    // Verify
    LogAnalyzer analyzer(manager.getLogFilePath());
    analyzer.load();
    EXPECT_TRUE(analyzer.containsPattern("Expected message"));

    // Cleanup
    manager.stop();
}
```

4. Add the test file to `CMakeLists.txt`:
```cmake
add_executable(my_test
    testcases/my_test.cpp
)
target_link_libraries(my_test cdmf_automation GTest::GTest GTest::Main)
```

## Test Scenarios Mapping

This automation system supports testing scenarios from `test-scenarios.md`:

- **TS-CORE-001**: Basic Lifecycle Operations
- **TS-CORE-002**: WaitForStop Blocking Behavior
- **TS-CORE-004**: State Machine Verification
- **TS-LOG-001**: Multi-Level Logging
- **TS-CFG-001**: Centralized Configuration Management
- And more...

## Troubleshooting

### Issue: Test fails to start CDMF process
**Solution**:
- Verify `executable_path` is correct (relative to test working directory)
- Check that CDMF binary is built: `ls workspace/build/bin/cdmf`
- Ensure config file exists: `ls workspace/build/config/framework.json`

### Issue: Log file not found
**Solution**:
- Ensure logs directory exists: `mkdir -p workspace/build/tests/automation_test/logs`
- Check file permissions
- Verify `log_file` path in ProcessConfig

### Issue: Process doesn't stop gracefully
**Solution**:
- Increase `shutdown_timeout_ms` in ProcessConfig
- Check CDMF logs for shutdown issues
- Use `manager.kill()` as fallback

### Issue: Pattern not found in log
**Solution**:
- Reload log after process stops: `analyzer.reload()`
- Print log content: `std::cout << analyzer.getStdout();`
- Check for timing issues (framework may not have started yet)

## Best Practices

1. **Always wait after starting**: Give CDMF time to initialize (2-3 seconds minimum)
2. **Reload logs before final checks**: Use `analyzer.reload()` after stopping process
3. **Clean up resources**: Always stop the process in test teardown
4. **Use timeouts**: Set appropriate timeouts for start/stop operations
5. **Check exit codes**: Verify process exits cleanly with code 0
6. **Validate configs first**: Use ConfigAnalyzer before starting process
7. **Check for errors**: Always verify no ERROR level logs during normal operation

## Future Enhancements

Potential improvements for the automation testing system:

- [ ] Support for Windows platform (currently Linux-only)
- [ ] Real-time log monitoring (tail -f style)
- [ ] Performance metrics extraction from logs
- [ ] Automated test report generation (HTML/XML)
- [ ] Integration with CI/CD pipelines
- [ ] Module-specific test fixtures
- [ ] IPC communication testing
- [ ] Multi-process test scenarios
- [ ] Memory leak detection integration
- [ ] Coverage report integration

## License

This automation testing system is part of the CDMF project.
