project(gtest_memhawk)

collect_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
PRIVATE
    gtest
    gtest_main
    memhawk_impl
)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        $<TARGET_PROPERTY:memhawk_impl,SOURCE_DIR>/src
)

# embed_library(${PROJECT_NAME} memhawk_impl)

memhawk_build(${PROJECT_NAME})
