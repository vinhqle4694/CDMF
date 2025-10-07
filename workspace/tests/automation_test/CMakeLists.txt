# CMakeLists.txt for CDMF Automation Testing System

# Note: This CMakeLists.txt is included from the parent tests directory
# GTest is already available via parent CMakeLists.txt

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json/include
)

# Automation testing system library sources
set(AUTOMATION_LIB_SOURCES
    src/automation_manager.cpp
    src/log_analyzer.cpp
    src/config_analyzer.cpp
)

# Create automation library
add_library(cdmf_automation STATIC ${AUTOMATION_LIB_SOURCES})

target_include_directories(cdmf_automation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link with nlohmann_json
target_link_libraries(cdmf_automation
    nlohmann_json::nlohmann_json
)

# Test executable
add_executable(automation_system_test
    testcases/test_automation_system.cpp
)

# Link test executable
target_link_libraries(automation_system_test
    cdmf_automation
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
)

# Enable testing
enable_testing()

# Add test
add_test(NAME AutomationSystemTest COMMAND automation_system_test)

# Create logs directory for test output
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/logs)

# Print configuration
message(STATUS "CDMF Automation Tests Configuration:")
message(STATUS "  Test binary: automation_system_test")
message(STATUS "  Log directory: ${CMAKE_CURRENT_BINARY_DIR}/logs")
