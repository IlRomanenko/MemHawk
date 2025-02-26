
set(target_name MemHawk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror=undef -Werror=return-type -g -flto=auto -fPIC")

# include(CheckSymbolExists)
# check_symbol_exists(cfree malloc.h HAVE_CFREE)
# check_symbol_exists(valloc stdlib.h HAVE_VALLOC)

add_library(${target_name} SHARED
    src/thread_tracker.cpp
    src/stacktrace_tracker.cpp
    src/log.cpp
    src/memhawk.cpp
    src/preload.cpp
    src/stacktrace.cpp
    src/global_storage.cpp
    src/memhawk_api.cpp
)

target_link_libraries(${target_name}
PRIVATE
    unwind
    unwind-generic
    fmt
    absl::flat_hash_map
    atomic
    dl
)

target_include_directories(${target_name}
PUBLIC
    include/
)

set(MEMHAWK_LIB_VERSION 1.0.00)
set(MEMHAWK_LIB_SOVERSION 1)


set_target_properties(${target_name} PROPERTIES
    VERSION ${MEMHAWK_LIB_VERSION}
    SOVERSION ${MEMHAWK_LIB_SOVERSION}
    PUBLIC_HEADER "include/MemHawk/memhawk_api.h"
)

install(TARGETS ${target_name}
    LIBRARY NAMELINK_COMPONENT
    PUBLIC_HEADER DESTINATION "include/MemHawk"
)

memhawk_build(${target_name})

if (MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
