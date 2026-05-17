
project(flamegraph)

collect_project_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})
set(HEADERS_IMPL ${HEADERS})
set(SOURCES_IMPL ${SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Werror=undef -Werror=return-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-conversion -Wunused  -Wold-style-cast -Wcast-align")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual -Wnon-virtual-dtor -Wsuggest-override")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmisleading-indentation -Wstrict-aliasing -Warray-bounds -Wimplicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized")

add_executable(${PROJECT_NAME} ${HEADERS_IMPL} ${SOURCES_IMPL})

target_link_libraries(${PROJECT_NAME}
PRIVATE
    absl::flat_hash_map
)

install(TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/monitoring/clickhouse/udfs
)

memhawk_build(${PROJECT_NAME})

if(MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
