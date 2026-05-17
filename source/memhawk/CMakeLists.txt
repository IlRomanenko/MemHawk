
project(memhawk)

collect_project_sources(HEADERS SOURCES ${PROJECT_SOURCE_DIR})
set(HEADERS_IMPL ${HEADERS})
set(SOURCES_IMPL ${SOURCES})
list(FILTER HEADERS EXCLUDE REGEX ".*/impl/.*")
list(FILTER HEADERS_IMPL INCLUDE REGEX ".*/impl/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/impl/.*")
list(FILTER SOURCES_IMPL INCLUDE REGEX ".*/impl/.*")

# Add static library with all sources except preload part
add_library(${PROJECT_NAME}_impl STATIC ${HEADERS_IMPL} ${SOURCES_IMPL})

# Configure the header file
configure_file(
    ${PROJECT_SOURCE_DIR}/src/impl/config_defs.h.in
    ${PROJECT_SOURCE_DIR}/src/impl/config_defs.h
)


target_link_libraries(${PROJECT_NAME}_impl
PUBLIC
    Boost::range
    Boost::algorithm
    Boost::circular_buffer
    Boost::container
    Boost::hana
    Boost::multi_index
    xxHash::xxhash
    libunwind::libunwind
    fmt::fmt
    absl::flat_hash_map
    absl::synchronization
    absl::time
    absl::span
    absl::cleanup
    absl::stacktrace
    libzstd_static
    proto_files
    dl
)

target_include_directories(${PROJECT_NAME}_impl
PUBLIC
    include/
    ${PROJECT_SOURCE_DIR}/src/impl
)

add_library(${PROJECT_NAME} SHARED ${HEADERS} ${SOURCES})
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    -Wl,--exclude-libs,ALL
    ${PROJECT_NAME}_impl
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${MEMHAWK_LIB_VERSION}
    SOVERSION ${MEMHAWK_LIB_SOVERSION}
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY NAMELINK_COMPONENT DESTINATION ${CMAKE_INSTALL_PREFIX}/memhawk/lib
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/memhwak
)

memhawk_build(${PROJECT_NAME})

if(MEMHAWK_BUILD_TESTING)
    add_subdirectory(tests)
endif()
