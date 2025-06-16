project(gtest_memhawk)

collect_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
PRIVATE
    GTest::gtest_main
    GTest::gmock
)

embed_library(${PROJECT_NAME} libmemhawk_impl)

memhawk_build(${PROJECT_NAME})

add_test(NAME gtest_memhawk COMMAND gtest_memhawk)
