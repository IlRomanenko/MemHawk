
set(target_name benchmark_memhawk)

add_executable(${target_name} 
    main.cpp
)

target_link_libraries(${target_name} 
PRIVATE
    benchmark::benchmark
    unwind_static
    absl::stacktrace
)

memhawk_build(${target_name})
