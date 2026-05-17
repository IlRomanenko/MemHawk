
project(flamegraph)

collect_project_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})
set(HEADERS_IMPL ${HEADERS})
set(SOURCES_IMPL ${SOURCES})

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
