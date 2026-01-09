
find_program(CARGO cargo REQUIRED)

add_subdirectory(flamegraph)
# todo: is it necessary to add custom target?
# add_subdirectory(memhawk_core)
add_subdirectory(symbolizer)

