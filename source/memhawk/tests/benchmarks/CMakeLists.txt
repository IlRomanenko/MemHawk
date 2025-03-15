
add_executable(benchmark_memhawk
    main.cpp
)

target_link_libraries(benchmark_memhawk
PRIVATE
    benchmark::benchmark
    unwind_static
    absl::stacktrace
)

memhawk_build(benchmark_memhawk)


add_executable(benchmark_compression
    compression_benchmark.cpp
)
target_link_libraries(benchmark_compression
PRIVATE
    benchmark::benchmark
    unwind_static
    absl::stacktrace
)
memhawk_build(benchmark_compression)
