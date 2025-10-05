add_library(shm_consumer_service SHARED
    shm_consumer_service_impl.cpp
    shm_consumer_service_activator.cpp
)

target_include_directories(shm_consumer_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/framework/include
)

target_link_libraries(shm_consumer_service PRIVATE
    cdmf_core cdmf_module cdmf_service cdmf_ipc
)

set_target_properties(shm_consumer_service PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Copy manifest to config directory
add_custom_command(TARGET shm_consumer_service POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/manifest.json
        ${CMAKE_BINARY_DIR}/config/modules/shm_consumer_service.json
)
