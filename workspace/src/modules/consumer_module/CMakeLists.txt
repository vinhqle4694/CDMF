# Consumer Module
cmake_minimum_required(VERSION 3.14)

# Module library
add_library(consumer_module SHARED
    consumer_activator.cpp
)

target_include_directories(consumer_module
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_SOURCE_DIR}/src/modules/calculator_module
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(consumer_module
    PRIVATE
        cdmf_core
        cdmf_module
        cdmf_service
        nlohmann_json::nlohmann_json
)

# Set module output to lib directory
set_target_properties(consumer_module PROPERTIES
    PREFIX ""
    OUTPUT_NAME "consumer_module"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy module config to config/modules directory
add_custom_command(TARGET consumer_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/consumer_module.json
    COMMENT "Installing consumer module configuration"
)

# Install
install(TARGETS consumer_module
    LIBRARY DESTINATION lib/cdmf/modules
    RUNTIME DESTINATION lib/cdmf/modules
)

install(FILES manifest.json
    DESTINATION lib/cdmf/modules
    RENAME consumer_module.manifest.json
)
