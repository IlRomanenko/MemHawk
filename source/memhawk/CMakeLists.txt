
set(target_name memhawk)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Werror=undef -Werror=return-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion -Wunused  -Wold-style-cast -Wcast-align")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wnon-virtual-dtor -Wsuggest-override")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuseless-cast  -Wduplicated-cond -Wduplicated-branches")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmisleading-indentation -Wstrict-aliasing -Warray-bounds -Wimplicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized")

add_library(${target_name} SHARED
    src/thread_tracker.cpp
    src/stacktrace_tracker.cpp
    src/stacktrace_tracker_fixed_size.cpp
    src/log.cpp
    src/memhawk.cpp
    src/preload.cpp
    src/stacktrace.cpp
    src/global_storage.cpp
    src/memhawk_api.cpp
)

target_link_libraries(${target_name}
PRIVATE
    xxhash_static
    unwind_static
    fmt
    absl::flat_hash_map
    absl::synchronization
    absl::time
    absl::span
    absl::cleanup
    atomic
    dl
)

target_include_directories(${target_name}
PUBLIC
    include/
)

set_target_properties(${target_name} PROPERTIES
    VERSION ${MEMHAWK_LIB_VERSION}
    SOVERSION ${MEMHAWK_LIB_SOVERSION}
    PUBLIC_HEADER "include/MemHawk/memhawk_api.h"
)

install(TARGETS ${target_name}
    LIBRARY NAMELINK_COMPONENT DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/memhwak
)

memhawk_build(${target_name})

if (MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
