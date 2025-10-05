# Calculator Service Module
cmake_minimum_required(VERSION 3.14)

# Module library
add_library(calculator_module SHARED
    calculator_activator.cpp
    calculator_service.cpp
)

target_include_directories(calculator_module
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/include
        ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(calculator_module
    PRIVATE
        cdmf_core
        cdmf_module
        cdmf_service
        nlohmann_json::nlohmann_json
)

# Set module output to lib directory
set_target_properties(calculator_module PROPERTIES
    PREFIX ""
    OUTPUT_NAME "calculator_module"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy module config to config/modules directory
add_custom_command(TARGET calculator_module POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config/modules
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/calculator_module.json
    COMMENT "Installing calculator module configuration"
)

# Install
install(TARGETS calculator_module
    LIBRARY DESTINATION lib/cdmf/modules
    RUNTIME DESTINATION lib/cdmf/modules
)

install(FILES manifest.json
    DESTINATION lib/cdmf/modules
    RENAME calculator_module.manifest.json
)
