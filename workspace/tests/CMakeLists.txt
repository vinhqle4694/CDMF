# ==============================================================================
# CDMF Unit Tests
# ==============================================================================

# PHASE_1 Tests (Platform Layer)
add_executable(test_dynamic_loaders unit/test_dynamic_loaders.cpp)
target_link_libraries(test_dynamic_loaders PRIVATE cdmf_platform GTest::gtest GTest::gtest_main)
add_test(NAME DynamicLoaders COMMAND test_dynamic_loaders)

add_executable(test_platform_abstraction unit/test_platform_abstraction.cpp)
target_link_libraries(test_platform_abstraction PRIVATE cdmf_platform GTest::gtest GTest::gtest_main)
add_test(NAME PlatformAbstraction COMMAND test_platform_abstraction)

add_executable(test_blocking_queue unit/test_blocking_queue.cpp)
target_link_libraries(test_blocking_queue PRIVATE cdmf_foundation GTest::gtest GTest::gtest_main)
add_test(NAME BlockingQueue COMMAND test_blocking_queue)

add_executable(test_thread_pool unit/test_thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE cdmf_foundation GTest::gtest GTest::gtest_main)
add_test(NAME ThreadPool COMMAND test_thread_pool)

# PHASE_2 Tests (Core Utilities)
add_executable(test_version unit/test_version.cpp)
target_link_libraries(test_version PRIVATE cdmf_core GTest::gtest GTest::gtest_main)
add_test(NAME Version COMMAND test_version)

add_executable(test_properties unit/test_properties.cpp)
target_link_libraries(test_properties PRIVATE cdmf_core GTest::gtest GTest::gtest_main)
add_test(NAME Properties COMMAND test_properties)

add_executable(test_event unit/test_event.cpp)
target_link_libraries(test_event PRIVATE cdmf_core GTest::gtest GTest::gtest_main)
add_test(NAME Event COMMAND test_event)

# PHASE_3 Tests (Framework Core)
add_executable(test_framework_properties unit/test_framework_properties.cpp)
target_link_libraries(test_framework_properties PRIVATE cdmf_core GTest::gtest GTest::gtest_main)
add_test(NAME FrameworkProperties COMMAND test_framework_properties)

add_executable(test_event_dispatcher unit/test_event_dispatcher.cpp)
target_link_libraries(test_event_dispatcher PRIVATE cdmf_core GTest::gtest GTest::gtest_main)
add_test(NAME EventDispatcher COMMAND test_event_dispatcher)

# PHASE_4 Tests (Module Management)
add_executable(test_module_types unit/test_module_types.cpp)
target_link_libraries(test_module_types PRIVATE cdmf_module nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME ModuleTypes COMMAND test_module_types)

add_executable(test_module_handle unit/test_module_handle.cpp)
target_link_libraries(test_module_handle PRIVATE cdmf_module GTest::gtest GTest::gtest_main)
add_test(NAME ModuleHandle COMMAND test_module_handle)

add_executable(test_manifest_parser unit/test_manifest_parser.cpp)
target_link_libraries(test_manifest_parser PRIVATE cdmf_module nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME ManifestParser COMMAND test_manifest_parser)

add_executable(test_module unit/test_module.cpp)
target_link_libraries(test_module PRIVATE cdmf_module nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME Module COMMAND test_module)

add_executable(test_module_registry unit/test_module_registry.cpp)
target_link_libraries(test_module_registry PRIVATE cdmf_module nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME ModuleRegistry COMMAND test_module_registry)

# Dependency Resolution Tests (NEW)
add_executable(test_dependency_graph unit/test_dependency_graph.cpp)
target_link_libraries(test_dependency_graph PRIVATE cdmf_module GTest::gtest GTest::gtest_main)
add_test(NAME DependencyGraph COMMAND test_dependency_graph)

add_executable(test_dependency_resolver unit/test_dependency_resolver.cpp)
target_link_libraries(test_dependency_resolver PRIVATE cdmf_module nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME DependencyResolver COMMAND test_dependency_resolver)

# PHASE_5 Tests (Service Layer)
add_executable(test_service_reference unit/test_service_reference.cpp)
target_link_libraries(test_service_reference PRIVATE cdmf_service GTest::gtest GTest::gtest_main)
add_test(NAME ServiceReference COMMAND test_service_reference)

add_executable(test_service_registration unit/test_service_registration.cpp)
target_link_libraries(test_service_registration PRIVATE cdmf_service GTest::gtest GTest::gtest_main)
add_test(NAME ServiceRegistration COMMAND test_service_registration)

add_executable(test_service_registry unit/test_service_registry.cpp)
target_link_libraries(test_service_registry PRIVATE cdmf_service GTest::gtest GTest::gtest_main)
add_test(NAME ServiceRegistry COMMAND test_service_registry)

add_executable(test_service_tracker unit/test_service_tracker.cpp)
target_link_libraries(test_service_tracker PRIVATE cdmf_service GTest::gtest GTest::gtest_main)
add_test(NAME ServiceTracker COMMAND test_service_tracker)

# PHASE_6 Tests (IPC Layer)
add_executable(test_message unit/test_message.cpp)
target_link_libraries(test_message PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME Message COMMAND test_message)

add_executable(test_ring_buffer unit/test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME RingBuffer COMMAND test_ring_buffer)

add_executable(test_serializers unit/test_serializers.cpp)
target_link_libraries(test_serializers PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME Serializers COMMAND test_serializers)

add_executable(test_transports unit/test_transports.cpp)
target_link_libraries(test_transports PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME Transports COMMAND test_transports)

add_executable(test_proxy_stub unit/test_proxy_stub.cpp)
target_link_libraries(test_proxy_stub PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME ProxyStub COMMAND test_proxy_stub)

add_executable(test_proxy_generator unit/test_proxy_generator.cpp)
target_link_libraries(test_proxy_generator PRIVATE cdmf_ipc nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME ProxyGenerator COMMAND test_proxy_generator)

add_executable(test_proxy_factory unit/test_proxy_factory.cpp)
target_link_libraries(test_proxy_factory PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME ProxyFactory COMMAND test_proxy_factory)

add_executable(test_service_proxy_factory unit/test_service_proxy_factory.cpp)
target_link_libraries(test_service_proxy_factory PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME ServiceProxyFactory COMMAND test_service_proxy_factory)

add_executable(test_reliability unit/test_reliability.cpp)
target_link_libraries(test_reliability PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME Reliability COMMAND test_reliability)

add_executable(test_connection_management unit/test_connection_management.cpp)
target_link_libraries(test_connection_management PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME ConnectionManagement COMMAND test_connection_management)

# PHASE_7 Tests (Security)
add_executable(test_permission unit/test_permission.cpp)
target_link_libraries(test_permission PRIVATE cdmf_security GTest::gtest GTest::gtest_main)
add_test(NAME Permission COMMAND test_permission)

add_executable(test_permission_manager unit/test_permission_manager.cpp)
target_link_libraries(test_permission_manager PRIVATE cdmf_security GTest::gtest GTest::gtest_main)
add_test(NAME PermissionManager COMMAND test_permission_manager)

add_executable(test_code_verifier unit/test_code_verifier.cpp)
target_link_libraries(test_code_verifier PRIVATE cdmf_security GTest::gtest GTest::gtest_main)
add_test(NAME CodeVerifier COMMAND test_code_verifier)

add_executable(test_sandbox_manager unit/test_sandbox_manager.cpp)
target_link_libraries(test_sandbox_manager PRIVATE cdmf_security GTest::gtest GTest::gtest_main)
add_test(NAME SandboxManager COMMAND test_sandbox_manager)

add_executable(test_resource_limiter unit/test_resource_limiter.cpp)
target_link_libraries(test_resource_limiter PRIVATE cdmf_security GTest::gtest GTest::gtest_main)
add_test(NAME ResourceLimiter COMMAND test_resource_limiter)

# ==============================================================================
# Additional Comprehensive Tests
# ==============================================================================

# Circuit Breaker Tests (IPC Reliability)
add_executable(test_circuit_breaker unit/test_circuit_breaker.cpp)
target_link_libraries(test_circuit_breaker PRIVATE cdmf_ipc GTest::gtest GTest::gtest_main)
add_test(NAME CircuitBreaker COMMAND test_circuit_breaker)

# File Watcher Tests (Utilities)
add_executable(test_file_watcher unit/test_file_watcher.cpp)
target_link_libraries(test_file_watcher PRIVATE cdmf_foundation GTest::gtest GTest::gtest_main)
add_test(NAME FileWatcher COMMAND test_file_watcher)

# Version Manager Tests (Utilities)
add_executable(test_version_manager unit/test_version_manager.cpp)
target_link_libraries(test_version_manager PRIVATE cdmf_core GTest::gtest GTest::gtest_main)
add_test(NAME VersionManager COMMAND test_version_manager)

# Command Handler Tests (Utilities)
add_executable(test_command_handler unit/test_command_handler.cpp)
target_link_libraries(test_command_handler PRIVATE cdmf_foundation cdmf_core cdmf_service GTest::gtest GTest::gtest_main)
add_test(NAME CommandHandler COMMAND test_command_handler)

# Module Reloader Tests (Module Management)
add_executable(test_module_reloader unit/test_module_reloader.cpp)
target_link_libraries(test_module_reloader PRIVATE cdmf_module cdmf_foundation cdmf_core nlohmann_json::nlohmann_json GTest::gtest GTest::gtest_main)
add_test(NAME ModuleReloader COMMAND test_module_reloader)
