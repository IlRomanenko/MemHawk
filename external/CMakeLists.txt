include(FetchContent)

FetchContent_Declare(
    fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.1.1
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(fmt)

# FetchContent_Declare(
#     unordered_dense
#         GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
#         GIT_TAG v4.5.0
#     EXCLUDE_FROM_ALL
# )
# FetchContent_MakeAvailable(unordered_dense)

FetchContent_Declare(
    xxHash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v0.8.2
        SOURCE_SUBDIR  cmake_unofficial
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(xxHash)


# Set options before fetching Abseil
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    absl
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG 20240722.0
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(absl)


FetchContent_Declare(
    googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.1
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(benchmark)
