project(monitoring)

# file(GLOB_RECURSE SOURCES ".*")

# add_custom_target(${PROJECT_NAME} ALL
#     BYPRODUCTS ${RUST_BINARY_PATH}
#     COMMAND ${CARGO} build --release
#     WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#     DEPENDS ${SOURCES}
#     COMMENT "Building Rust binary: ${RUST_BINARY_NAME}"
# )

install(DIRECTORY clickhouse 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring
)
install(DIRECTORY grafana 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring
)
install(DIRECTORY postgres 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring
)
install(FILES docker-compose.yaml 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring
)

# add_dependencies(memhawk_all ${PROJECT_NAME})
# memhawk_devbuild_file(${PROJECT_NAME} ${RUST_BINARY_PATH})
