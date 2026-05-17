project(gtest_flamegraph)

collect_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
PRIVATE
    GTest::gtest_main
    GTest::gmock
    absl::flat_hash_map
)

# embed_library(${PROJECT_NAME} flamegraph)

target_include_directories(${PROJECT_NAME} PRIVATE "../src")

memhawk_build(${PROJECT_NAME})

add_test(NAME gtest_flamegraph COMMAND gtest_flamegraph)
