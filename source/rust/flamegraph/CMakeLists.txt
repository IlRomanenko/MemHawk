project(flamegraph)

# Track Rust source files for rebuilds
file(GLOB_RECURSE RUST_SOURCES 
    "src/*.rs"
    "Cargo.toml"
    "Cargo.lock"
)

set(RUST_BINARY_NAME flamegraph)
# Use path from project root because cargo workspace was specified
set(RUST_BINARY_PATH ${MEMHAWK_ROOT_DIR}/target/release/${RUST_BINARY_NAME})

add_custom_target(${PROJECT_NAME} ALL
    BYPRODUCTS ${RUST_BINARY_PATH}
    COMMAND ${CARGO} build --release
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS ${RUST_SOURCES}
    COMMENT "Building Rust binary: ${RUST_BINARY_NAME}"
)

# todo: Replace full path with path from target if it's possible
install(PROGRAMS ${RUST_BINARY_PATH}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring/clickhouse/udfs
)

add_dependencies(memhawk_all ${PROJECT_NAME})
memhawk_devbuild_file(${PROJECT_NAME} ${RUST_BINARY_PATH})
