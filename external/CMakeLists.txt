include(FetchContent)

message(NOTICE "Enabling `fmt` dependency")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.1.1
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(fmt)

# Set options before fetching Abseil
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

message(NOTICE "Enabling `absl` dependency")
FetchContent_Declare(
    absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG 20240722.0
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(absl)

if (MEMHAWK_BUILD_PROTOBUF)
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
    set(protobuf_LOCAL_DEPENDENCIES_ONLY ON CACHE BOOL "Only local dependencies" FORCE)
    set(protobuf_ALLOW_CCACHE ON CACHE BOOL "Allow ccache" FORCE)
    set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "Build protoc compiler")
    set(protobuf_BUILD_LIBPROTOBUF_LITE ON CACHE BOOL "Skip lite version" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs" FORCE)
    set(protobuf_WITH_ZLIB ${MEMHAWK_ZLIB} CACHE BOOL "Build with zlib support" FORCE)

    message(NOTICE "Enabling `protobuf` dependency")
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG v31.1
        EXCLUDE_FROM_ALL
        SYSTEM
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(protobuf)

    # Necessary include in order to get protobuf-generate function
    include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)
endif()

if(${MEMHAWK_BUILD_TESTING})

    message(NOTICE "Enabling `googletest` dependency")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(googletest)


    message(NOTICE "Enabling `benchmark` dependency")
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(benchmark)
endif()

find_package(Threads REQUIRED)
find_package(Boost 1.70.0 REQUIRED NO_MODULE COMPONENTS container)

# set flags for external dependencies
set(CFLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
set(CXXFLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

include(libunwind.cmake)
include(xxhash.cmake)
