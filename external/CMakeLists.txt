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

include(libunwind.cmake)
include(xxhash.cmake)
