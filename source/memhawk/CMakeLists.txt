
project(memhawk)

collect_project_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})
set(HEADERS_IMPL ${HEADERS})
set(SOURCES_IMPL ${SOURCES})
list(FILTER HEADERS EXCLUDE REGEX ".*/impl/.*")
list(FILTER HEADERS_IMPL INCLUDE REGEX ".*/impl/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/impl/.*")
list(FILTER SOURCES_IMPL INCLUDE REGEX ".*/impl/.*")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Werror=undef -Werror=return-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion -Wunused  -Wold-style-cast -Wcast-align")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wnon-virtual-dtor -Wsuggest-override")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmisleading-indentation -Wstrict-aliasing -Warray-bounds -Wimplicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized")

if (MEMHAWK_USE_CLANG_TIDY)
    set (CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_PATH}")
endif ()

# Add static library with all sources except preload part
add_library(${PROJECT_NAME}_impl STATIC ${HEADERS_IMPL} ${SOURCES_IMPL})

# Configure the header file
configure_file(
    ${PROJECT_SOURCE_DIR}/src/impl/config_defs.h.in
    ${PROJECT_SOURCE_DIR}/src/impl/config_defs.h
)


set(PROTO_DIR ${PROJECT_SOURCE_DIR})
set(PROTO_FILES ${PROJECT_SOURCE_DIR}/protos/snapshot.proto)
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
make_directory(${PROTO_BINARY_DIR})

set(PROTO_GENERATED_SRCS 
    "${PROTO_BINARY_DIR}/protos/snapshot.pb.h"
    "${PROTO_BINARY_DIR}/protos/snapshot.pb.cc"
)
add_custom_command(
  OUTPUT ${PROTO_GENERATED_SRCS}
  COMMAND ${CMAKE_COMMAND} -E env ${SANITIZERS_SUPPRESS_OPTIONS}
    $<TARGET_FILE:protobuf::protoc>
    --proto_path=${PROTO_DIR}
    --cpp_out=lite:${PROTO_BINARY_DIR}
    ${PROTO_FILES}
  DEPENDS ${PROTO_FILES}
  COMMENT "Generating protobuf sources"
  VERBATIM
)
add_library(proto_files OBJECT ${PROTO_GENERATED_SRCS})
target_link_libraries(proto_files PUBLIC protobuf::libprotobuf)

target_include_directories(proto_files PUBLIC
  "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>"
)

target_link_libraries(${PROJECT_NAME}_impl
PUBLIC
    Boost::range
    Boost::algorithm
    Boost::circular_buffer
    Boost::container
    Boost::hana
    Boost::multi_index
    xxHash::xxhash
    libunwind::libunwind
    fmt::fmt
    absl::flat_hash_map
    absl::synchronization
    absl::time
    absl::span
    absl::cleanup
    absl::stacktrace
    proto_files
)

target_include_directories(${PROJECT_NAME}_impl
PUBLIC
    include/
    ${PROJECT_SOURCE_DIR}/src/impl
)

add_library(${PROJECT_NAME} SHARED ${HEADERS} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_NAME}_impl
    dl
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${MEMHAWK_LIB_VERSION}
    SOVERSION ${MEMHAWK_LIB_SOVERSION}
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY NAMELINK_COMPONENT DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/memhwak
)

memhawk_build(${PROJECT_NAME})

if(MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
