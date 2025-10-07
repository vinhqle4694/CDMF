cmake_minimum_required(VERSION 3.15)

project(CDMF VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Project Configuration
# ==============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# Options
# ==============================================================================

option(CDMF_BUILD_TESTS "Build unit tests" OFF)
option(CDMF_BUILD_SHARED_LIBS "Build shared libraries" ON)
option(CDMF_ENABLE_COVERAGE "Enable code coverage" OFF)
option(CDMF_ENABLE_SANITIZERS "Enable sanitizers (address, undefined)" OFF)
option(CDMF_IGNORE_WARNINGS "Ignore all compiler warnings" ON)

# ==============================================================================
# Compiler Flags
# ==============================================================================

if(CDMF_IGNORE_WARNINGS)
    # Ignore all warnings
    if(MSVC)
        add_compile_options(/w)
    else()
        add_compile_options(-w)
    endif()
    message(STATUS "All compiler warnings disabled")
else()
    # Enable warnings
    if(MSVC)
        # Windows MSVC
        add_compile_options(/W4 /WX)
    else()
        # Linux/macOS GCC/Clang
        add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

# Coverage flags
if(CDMF_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

# Sanitizer flags
if(CDMF_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# ==============================================================================
# Dependencies
# ==============================================================================

# nlohmann/json library - using local third_party copy
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json ${CMAKE_BINARY_DIR}/nlohmann_json)

# ==============================================================================
# Platform Detection
# ==============================================================================

if(UNIX AND NOT APPLE)
    set(CDMF_PLATFORM "Linux")
    set(CDMF_PLATFORM_LINUX TRUE)
elseif(WIN32)
    set(CDMF_PLATFORM "Windows")
    set(CDMF_PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(CDMF_PLATFORM "macOS")
    set(CDMF_PLATFORM_MACOS TRUE)
else()
    set(CDMF_PLATFORM "Unknown")
endif()

message(STATUS "Building for platform: ${CDMF_PLATFORM}")

# ==============================================================================
# Framework Libraries and Executable
# ==============================================================================

add_subdirectory(src/framework)

# ==============================================================================
# Modules
# ==============================================================================

add_subdirectory(src/modules/hello_service_module)

# ==============================================================================
# Unit Tests
# ==============================================================================

if(CDMF_BUILD_TESTS)
    enable_testing()

    # Google Test - using local third_party copy
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest ${CMAKE_BINARY_DIR}/googletest)

    # Add tests subdirectory
    add_subdirectory(tests/unit)
endif()

# ==============================================================================
# Installation
# ==============================================================================

install(TARGETS
    cdmf_foundation
    cdmf_platform
    cdmf_core
    cdmf_ipc
    cdmf_module
    cdmf_service
    cdmf_security
    cdmf
    cdmf_main
    EXPORT CDMFTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY src/framework/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT CDMFTargets
    FILE CDMFTargets.cmake
    NAMESPACE CDMF::
    DESTINATION lib/cmake/CDMF
)

# ==============================================================================
# Summary
# ==============================================================================

message(STATUS "")
message(STATUS "CDMF Framework Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Platform: ${CDMF_PLATFORM}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared Libraries: ${CDMF_BUILD_SHARED_LIBS}")
message(STATUS "  Unit Tests: ${CDMF_BUILD_TESTS}")
message(STATUS "  Code Coverage: ${CDMF_ENABLE_COVERAGE}")
message(STATUS "  Sanitizers: ${CDMF_ENABLE_SANITIZERS}")
message(STATUS "  Ignore Warnings: ${CDMF_IGNORE_WARNINGS}")
message(STATUS "")
message(STATUS "Libraries:")
message(STATUS "  - cdmf_foundation (Foundation utilities)")
message(STATUS "  - cdmf_platform (Platform abstraction)")
message(STATUS "  - cdmf_core (Core framework)")
message(STATUS "  - cdmf_ipc (Inter-process communication)")
message(STATUS "  - cdmf_module (Module management)")
message(STATUS "  - cdmf_service (Service layer)")
message(STATUS "  - cdmf_security (Security subsystem)")
message(STATUS "  - cdmf (All-in-one)")
message(STATUS "")
message(STATUS "Modules:")
message(STATUS "  - hello_service_module (Sample service module)")
message(STATUS "")
message(STATUS "Executables:")
message(STATUS "  - cdmf_main -> cdmf (Main application)")
message(STATUS "")
