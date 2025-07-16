project(benchmark_impl)

collect_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
PRIVATE
    benchmark::benchmark
    Boost::concept_check
)
embed_library(${PROJECT_NAME} memhawk_impl)

memhawk_build(${PROJECT_NAME})
