add_executable(test_binary
    main.cpp
)

target_link_libraries(test_binary
PRIVATE
    libmemhawk
)

memhawk_build(test_binary)

add_executable(bench_allocs
    bench_allocs.cpp
)

memhawk_build(bench_allocs)

memhawk_devbuild_file(bench_allocs ${CMAKE_CURRENT_SOURCE_DIR}/run_benchmarks.sh)
