cmake_minimum_required(VERSION 3.25.0)
project(alloc_tracker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -flto=auto -fPIC -pthread -fno-omit-frame-pointer")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprefetch-loop-arrays -funroll-loops")

option(MEMHAWK_BUILD_TESTING OFF) # build tests
option(MEMHAWK_ZLIB "enable zlib support in libunwind" OFF)
option(MEMHAWK_LZMA "enable lzma support in libunwind" OFF)
option(MEMHAWK_MSABI "enable ms-abi support in libunwind" OFF)

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

add_subdirectory(external)
add_subdirectory(source)
add_subdirectory(tests)
