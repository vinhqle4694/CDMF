# Hello Service Module CMakeLists

# Create shared library for the module
add_library(hello_service_module SHARED
    hello_service_impl.cpp
    hello_service_activator.cpp
)

# Include directories
target_include_directories(hello_service_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/workspace/src/framework/include
    ${nlohmann_json_SOURCE_DIR}/include
)

# Link with framework libraries
target_link_libraries(hello_service_module PRIVATE
    cdmf_core
    cdmf_module
    cdmf_service
    cdmf_security
)

# Set output properties
set_target_properties(hello_service_module PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy manifest to config directory
add_custom_command(TARGET hello_service_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/hello_service_module.json
    COMMENT "Copying hello_service_module manifest to config directory"
)
