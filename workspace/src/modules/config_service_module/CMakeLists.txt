# Configuration Service Module
cmake_minimum_required(VERSION 3.14)

# Module library (includes configuration service implementation)
add_library(config_service_module SHARED
    config_service_activator.cpp
    configuration.cpp
    persistence_manager.cpp
    configuration_event.cpp
    configuration_admin.cpp
)

target_include_directories(config_service_module
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/include
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(config_service_module
    PRIVATE
        cdmf_core
        cdmf_module
        cdmf_service
        nlohmann_json::nlohmann_json
)

# Set module output to lib directory
set_target_properties(config_service_module PROPERTIES
    PREFIX ""
    OUTPUT_NAME "config_service_module"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy module config to config/modules directory
add_custom_command(TARGET config_service_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/config_service_module.json
    COMMENT "Installing module configuration"
)

# Install
install(TARGETS config_service_module
    LIBRARY DESTINATION lib/cdmf/modules
    RUNTIME DESTINATION lib/cdmf/modules
)

install(FILES manifest.json
    DESTINATION lib/cdmf/modules
    RENAME config_service_module.manifest.json
)
