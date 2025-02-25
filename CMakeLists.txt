cmake_minimum_required(VERSION 3.20)
project(alloc_tracker VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -flto=auto -fPIC")
# add_compile_options(-Wall -Wextra -Wpedantic -g -flto=auto -fPIC)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # Static library

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
