add_executable(test_binary
    main.cpp
)
target_link_libraries(test_binary
PRIVATE
    memhawk
)
memhawk_build(test_binary)

add_executable(bench_allocs
    bench_allocs.cpp
)
memhawk_build(bench_allocs)

add_executable(test_dl_iter_phdr
    test_dl_iter_phdr.cpp
)
target_link_libraries(test_dl_iter_phdr
PRIVATE
)
memhawk_build(test_dl_iter_phdr)


add_executable(test_fork_process
    test_fork_process.cpp
)
target_link_libraries(test_fork_process
PRIVATE
)
memhawk_build(test_fork_process)


add_executable(test_not_joined_threads
    test_not_joined_threads.cpp
)
memhawk_build(test_not_joined_threads)

memhawk_devbuild_file(bench_allocs ${CMAKE_CURRENT_SOURCE_DIR}/run_benchmarks.sh)
