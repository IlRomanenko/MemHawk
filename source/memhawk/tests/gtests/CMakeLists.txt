
add_executable(gtest_memhawk
    tests_lru_cache.cpp
)

target_link_libraries(gtest_memhawk
PRIVATE
    gtest
    gtest_main
    unwind_static
    absl::stacktrace
)

memhawk_build(gtest_memhawk)
