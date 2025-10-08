# ==============================================================================
# CDMF Framework Libraries and Executable
# ==============================================================================

cmake_minimum_required(VERSION 3.15)

# ==============================================================================
# Library: cdmf_foundation (Foundation utilities)
# ==============================================================================

set(CDMF_FOUNDATION_SOURCES
    impl/utils/thread_pool.cpp
    impl/utils/file_watcher.cpp
)

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_foundation SHARED ${CDMF_FOUNDATION_SOURCES})
else()
    add_library(cdmf_foundation STATIC ${CDMF_FOUNDATION_SOURCES})
endif()

target_include_directories(cdmf_foundation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cdmf_foundation
    PUBLIC pthread
)

# ==============================================================================
# Library: cdmf_platform (Platform abstraction)
# ==============================================================================

set(CDMF_PLATFORM_SOURCES
    impl/platform/platform_abstraction.cpp
)

if(CDMF_PLATFORM_LINUX)
    list(APPEND CDMF_PLATFORM_SOURCES
        impl/platform/linux_loader.cpp
    )
elseif(CDMF_PLATFORM_WINDOWS)
    list(APPEND CDMF_PLATFORM_SOURCES
        impl/platform/windows_loader.cpp
    )
elseif(CDMF_PLATFORM_MACOS)
    list(APPEND CDMF_PLATFORM_SOURCES
        impl/platform/macos_loader.cpp
    )
endif()

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_platform SHARED ${CDMF_PLATFORM_SOURCES})
else()
    add_library(cdmf_platform STATIC ${CDMF_PLATFORM_SOURCES})
endif()

target_include_directories(cdmf_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cdmf_platform
    PUBLIC
        cdmf_foundation
)

if(CDMF_PLATFORM_LINUX)
    target_link_libraries(cdmf_platform PUBLIC dl)
endif()

# ==============================================================================
# Library: cdmf_core (Core framework - Version, Properties, Events)
# ==============================================================================

set(CDMF_CORE_SOURCES
    impl/utils/version.cpp
    impl/utils/version_range.cpp
    impl/utils/version_manager.cpp
    impl/utils/properties.cpp
    impl/core/event.cpp
    impl/core/event_filter.cpp
    impl/core/framework_properties.cpp
    impl/core/event_dispatcher.cpp
    impl/config/configuration_event.cpp
    impl/config/configuration_impl.cpp
    impl/config/configuration_admin_impl.cpp
)

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_core SHARED ${CDMF_CORE_SOURCES})
else()
    add_library(cdmf_core STATIC ${CDMF_CORE_SOURCES})
endif()

target_include_directories(cdmf_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(cdmf_core
    PUBLIC
        cdmf_foundation
)

# ==============================================================================
# Library: cdmf_service (Service layer - Basic types)
# ==============================================================================

set(CDMF_SERVICE_SOURCES
    impl/service/service_reference.cpp
    impl/service/service_entry.cpp
    impl/service/service_event.cpp
    impl/service/service_registration.cpp
    impl/service/service_registry.cpp
    impl/utils/command_handler.cpp
)

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_service SHARED ${CDMF_SERVICE_SOURCES})
else()
    add_library(cdmf_service STATIC ${CDMF_SERVICE_SOURCES})
endif()

target_include_directories(cdmf_service
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cdmf_service
    PUBLIC
        cdmf_core
    PRIVATE
        nlohmann_json::nlohmann_json
)

# Link readline library for command history support (Linux/Unix only)
if(UNIX AND NOT APPLE)
    find_library(READLINE_LIBRARY readline)
    if(READLINE_LIBRARY)
        target_link_libraries(cdmf_service PRIVATE ${READLINE_LIBRARY})
        message(STATUS "Found readline library: ${READLINE_LIBRARY}")
    else()
        message(STATUS "readline library not found - command history will be disabled")
    endif()
elseif(APPLE)
    find_library(READLINE_LIBRARY readline)
    if(READLINE_LIBRARY)
        target_link_libraries(cdmf_service PRIVATE ${READLINE_LIBRARY})
        message(STATUS "Found readline library: ${READLINE_LIBRARY}")
    else()
        message(STATUS "readline library not found - command history will be disabled")
    endif()
endif()

# ==============================================================================
# Library: cdmf_ipc (Inter-Process Communication)
# ==============================================================================

set(CDMF_IPC_SOURCES
    impl/ipc/metadata.cpp
    impl/ipc/ring_buffer.cpp
    impl/ipc/retry_policy.cpp
    impl/ipc/reflection_proxy_generator.cpp
    impl/ipc/message.cpp
    impl/ipc/circuit_breaker.cpp
    impl/ipc/serializer.cpp
    impl/ipc/transport.cpp
    impl/ipc/protobuf_serializer.cpp
    impl/ipc/unix_socket_transport.cpp
    impl/ipc/flatbuffers_serializer.cpp
    impl/ipc/shared_memory_transport.cpp
    impl/ipc/grpc_transport.cpp
    impl/ipc/serializer_factory.cpp
    impl/ipc/service_proxy.cpp
    impl/ipc/service_stub.cpp
    impl/ipc/health_checker.cpp
    impl/ipc/connection_pool.cpp
    impl/ipc/connection_manager.cpp
    impl/ipc/service_proxy_factory.cpp
    impl/ipc/proxy_factory.cpp
)

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_ipc SHARED ${CDMF_IPC_SOURCES})
else()
    add_library(cdmf_ipc STATIC ${CDMF_IPC_SOURCES})
endif()

target_include_directories(cdmf_ipc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cdmf_ipc
    PUBLIC
        cdmf_core
        cdmf_foundation
)

# ==============================================================================
# Library: cdmf_module (Module management)
# ==============================================================================

set(CDMF_MODULE_SOURCES
    impl/module/module_handle.cpp
    impl/module/manifest_parser.cpp
    impl/module/module_impl.cpp
    impl/module/module_registry.cpp
    impl/module/module_reloader.cpp
    impl/module/dependency_graph.cpp
    impl/module/dependency_resolver.cpp
)

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_module SHARED ${CDMF_MODULE_SOURCES})
else()
    add_library(cdmf_module STATIC ${CDMF_MODULE_SOURCES})
endif()

target_include_directories(cdmf_module
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(cdmf_module
    PUBLIC
        cdmf_platform
        cdmf_core
        cdmf_service
    PRIVATE
        nlohmann_json::nlohmann_json
)

# ==============================================================================
# Library: cdmf_security (Security subsystem)
# ==============================================================================

set(CDMF_SECURITY_SOURCES
    impl/security/permission.cpp
    impl/security/permission_manager.cpp
    impl/security/code_verifier.cpp
    impl/security/sandbox_manager.cpp
    impl/security/sandbox_ipc.cpp
    impl/security/sandbox_module_loader.cpp
    impl/security/resource_limiter.cpp
)

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf_security SHARED ${CDMF_SECURITY_SOURCES})
else()
    add_library(cdmf_security STATIC ${CDMF_SECURITY_SOURCES})
endif()

target_include_directories(cdmf_security
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(cdmf_security
    PUBLIC
        cdmf_foundation
        cdmf_core
        cdmf_ipc
    PRIVATE
        nlohmann_json::nlohmann_json
)

# Link OpenSSL for cryptographic operations
find_package(OpenSSL QUIET)

if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found, fetching from GitHub...")
    include(FetchContent)

    # Fetch OpenSSL source
    FetchContent_Declare(
        openssl
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG openssl-3.0.13
        GIT_SHALLOW TRUE
    )

    # Configure OpenSSL build options
    set(OPENSSL_NO_APPS ON CACHE BOOL "" FORCE)
    set(OPENSSL_NO_TESTS ON CACHE BOOL "" FORCE)
    set(OPENSSL_NO_DOCS ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(openssl)

    # Create imported targets for compatibility
    add_library(OpenSSL::SSL ALIAS ssl)
    add_library(OpenSSL::Crypto ALIAS crypto)

    message(STATUS "OpenSSL fetched and configured successfully")
endif()

target_link_libraries(cdmf_security PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# ==============================================================================
# Convenience library: cdmf (All-in-one)
# ==============================================================================

if(CDMF_BUILD_SHARED_LIBS)
    add_library(cdmf SHARED
        impl/cdmf.cpp
        impl/core/framework.cpp
    )
else()
    add_library(cdmf STATIC
        impl/cdmf.cpp
        impl/core/framework.cpp
    )
endif()

target_include_directories(cdmf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(cdmf
    PUBLIC
        cdmf_foundation
        cdmf_platform
        cdmf_core
        cdmf_ipc
        cdmf_module
        cdmf_service
        cdmf_security
)

# ==============================================================================
# Executable: cdmf_main (Main application)
# ==============================================================================

add_executable(cdmf_main main.cpp)

target_include_directories(cdmf_main
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(cdmf_main
    PRIVATE
        cdmf
)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(cdmf_main PRIVATE pthread dl)
elseif(WIN32)
    # Windows-specific libraries if needed
endif()

# Set output name
set_target_properties(cdmf_main PROPERTIES OUTPUT_NAME cdmf)

# Copy configuration files to build directory
add_custom_command(TARGET cdmf_main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/config
        ${CMAKE_BINARY_DIR}/config
    COMMENT "Copying configuration files to build directory"
)
