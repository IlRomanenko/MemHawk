cmake_minimum_required(VERSION 3.25.0)
project(MemHawk LANGUAGES C CXX)

include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MEMHAWK_BUILD_TESTING "Build memhawk tests" OFF)
option(MEMHAWK_ZLIB "Enable zlib support in libunwind" OFF)
option(MEMHAWK_LZMA "Enable lzma support in libunwind" OFF)
option(MEMHAWK_MSABI "Enable ms-abi support in libunwind" OFF)
option(MEMHAWK_THINLTO "Clang-specific link time optimization" OFF)
option(MEMHAWK_USE_CLANG_TIDY "Enable clang-tidy build" OFF)
option(MEMHAWK_OPT_TLS "Enable optimized tls model" ON)
option(MEMHAWK_AVX "Compile with avx support" OFF)
option(MEMHAWK_LLD "Link with lld" OFF)
option(MEMHAWK_BUILD_PROTOBUF "Build protobuf library from sources" ON)
option(MEMHAWK_SELF_PROFILING "Build memhawk with self profiling" OFF)
option(MEMHAWK_BUILD_UI "Build hawk_visualizer" ON)

set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN OFF)

set(MEMHAWK_ROOT_DIR "${PROJECT_SOURCE_DIR}")

message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C Compiler version: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Linker: ${CMAKE_LINKER}")
message(STATUS)


set(MEMHAWK_COMMON_FLAGS "-fPIC -pthread -fno-omit-frame-pointer -fvisibility=hidden -funwind-tables")

# enabled by default
if(MEMHAWK_OPT_TLS)
    set(MEMHAWK_COMMON_FLAGS "${MEMHAWK_COMMON_FLAGS} -ftls-model=initial-exec")
endif()

if(MEMHAWK_AVX)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

    if(COMPILER_SUPPORTS_AVX2)
        message(STATUS "AVX-256 is supported")
        set(MEMHAWK_COMMON_FLAGS "${MEMHAWK_COMMON_FLAGS} -mavx2")
    else()
        message(WARNING "AVX-512 is not supported")
    endif()

    check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)

    if(COMPILER_SUPPORTS_AVX512)
        message(STATUS "AVX-512 is supported")
        set(MEMHAWK_COMMON_FLAGS "${MEMHAWK_COMMON_FLAGS} -mavx512f")
    else()
        message(WARNING "AVX-512 is not supported")
    endif()
endif()

check_cxx_compiler_flag("-fuse-ld=lld" HAS_LLD)
if(HAS_LLD AND MEMHAWK_LLD)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=lld")
    message(STATUS "Using LLD linker")
else()
    message(STATUS "Using default linker")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   ${MEMHAWK_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MEMHAWK_COMMON_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3")

if (NOT ASAN AND NOT TSAN)
    if(MEMHAWK_THINLTO)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=thin")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=thin")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=auto")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=auto")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=auto")
    endif()
endif()

set(MEMHAWK_LIB_VERSION 1.0.00)
set(MEMHAWK_LIB_SOVERSION 1)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
else()
    message(WARNING "ccache not found! Compilation will be slower.")
endif()

include(cmake/Sanitize.cmake)
include(cmake/DevBuild.cmake)
include(cmake/ClangTidy.cmake)

add_subdirectory(external)

enable_testing()
add_subdirectory(source)

if(MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
