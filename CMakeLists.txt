cmake_minimum_required(VERSION 3.25.0)
project(alloc_tracker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)


option(MEMHAWK_BUILD_TESTING "Build memhawk tests" OFF)
option(MEMHAWK_ZLIB "Enable zlib support in libunwind" OFF)
option(MEMHAWK_LZMA "Enable lzma support in libunwind" OFF)
option(MEMHAWK_MSABI "Enable ms-abi support in libunwind" OFF)
option(MEMHAWK_THINLTO "Clang-specific link time optimization" OFF)
option(MEMHAWK_USE_CLANG_TIDY "Enable clang-tidy build" OFF)
option(MEMHAWK_OPT_TLS "Enable optimized tls model" ON)

set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN OFF)

set(MEMHAWK_COMMON_FLAGS "-fPIC -pthread -fno-omit-frame-pointer -fvisibility=hidden -funwind-tables")
# enabled by default
if(MEMHAWK_OPT_TLS)
    set(MEMHAWK_COMMON_FLAGS "${MEMHAWK_COMMON_FLAGS} -ftls-model=initial-exec")
endif()
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${MEMHAWK_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MEMHAWK_COMMON_FLAGS} -std=c++20")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3")

if (MEMHAWK_THINLTO)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=thin")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=thin")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=thin")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto=auto")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=auto")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto=auto")
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
add_subdirectory(source)
if (MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
