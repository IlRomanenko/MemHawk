add_subdirectory(proto)
add_subdirectory(memhawk)

if (MEMHAWK_BUILD_UI)
  add_subdirectory(symbolizer)
endif()
