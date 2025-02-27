add_executable(test_binary
    main.cpp
)

target_link_libraries(test_binary
PRIVATE
    memhawk
)

memhawk_build(test_binary)
