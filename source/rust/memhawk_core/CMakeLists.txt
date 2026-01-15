project(memhawk_core)

# Track Rust source files for rebuilds
file(GLOB_RECURSE RUST_SOURCES 
    "src/*.rs"
    "Cargo.toml"
    "Cargo.lock"
)

set(RUST_BINARY_NAME "lib${PROJECT_NAME}.rlib")
# Use path from project root because cargo workspace was specified
set(RUST_BINARY_PATH ${MEMHAWK_ROOT_DIR}/target/release/${RUST_BINARY_NAME})

add_custom_target(${PROJECT_NAME} ALL
    BYPRODUCTS ${RUST_BINARY_PATH}
    COMMAND 
        ${CMAKE_COMMAND} -E env PROTOC=$<TARGET_FILE:protobuf::protoc> 
        ${CARGO} build --release --package ${PROJECT_NAME}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS ${RUST_SOURCES} $<TARGET_FILE:protobuf::protoc>
    COMMENT "Building Rust binary: ${RUST_BINARY_NAME}"
)

add_dependencies(memhawk_all ${PROJECT_NAME})
