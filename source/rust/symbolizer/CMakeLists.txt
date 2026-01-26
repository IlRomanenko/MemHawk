project(symbolizer)

find_program(CARGO cargo REQUIRED)

# Track Rust source files for rebuilds
file(GLOB_RECURSE RUST_SOURCES 
    "src/*.rs"
    "Cargo.toml"
    "Cargo.lock"
)

set(RUST_BINARY_NAME symbolizer)
# Use path from project root because cargo workspace was specified
set(RUST_BINARY_PATH ${MEMHAWK_ROOT_DIR}/target/release/${RUST_BINARY_NAME})

add_custom_target(${PROJECT_NAME} ALL
    BYPRODUCTS ${RUST_BINARY_PATH}
    COMMAND 
        ${CMAKE_COMMAND} -E env PROTOC=$<TARGET_FILE:protobuf::protoc> 
        ${CARGO} build --release --package ${PROJECT_NAME}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS ${RUST_SOURCES} memhawk_core
    COMMENT "Building Rust binary: ${RUST_BINARY_NAME}"
)

install(PROGRAMS ${RUST_BINARY_PATH}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/bin
)

add_dependencies(memhawk_all ${PROJECT_NAME})
memhawk_devbuild_file(${PROJECT_NAME} ${RUST_BINARY_PATH})
