add_library(unix_socket_client_service SHARED
    unix_socket_client_service_impl.cpp
    unix_socket_client_service_activator.cpp
)

target_include_directories(unix_socket_client_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/framework/include
)

target_link_libraries(unix_socket_client_service PRIVATE
    cdmf_core cdmf_module cdmf_service cdmf_ipc
)

set_target_properties(unix_socket_client_service PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy manifest to config directory
add_custom_command(TARGET unix_socket_client_service POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/unix_socket_client_service.json
)
