
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROTO_FILES ${PROTO_DIR}/protos/snapshot.proto)
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

add_subdirectory(memhawk)
