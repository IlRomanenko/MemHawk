include(ExternalProject)

# Download and build xxhash
set(XXHASH_INSTALL_DIR ${CMAKE_BINARY_DIR}/external/xxhash-install)
set(XXHASH_LIBRARY_DIR ${XXHASH_INSTALL_DIR}/lib)
set(XXHASH_INCLUDE_DIR ${XXHASH_INSTALL_DIR}/include)

set(XXHASH_INSTALL_OPTIONS)
list(APPEND XXHASH_INSTALL_OPTIONS "PREFIX=${XXHASH_INSTALL_DIR}")

ExternalProject_Add(
    xxhash_src
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG v0.8.3
    CONFIGURE_COMMAND ""
    BUILD_COMMAND make lib
    INSTALL_COMMAND ${XXHASH_INSTALL_OPTIONS} make install_libxxhash.a install_libxxhash.includes
    BUILD_IN_SOURCE ON
    UPDATE_DISCONNECTED 1
    INSTALL_BYPRODUCTS
        ${XXHASH_LIBRARY_DIR}/libxxhash.a
)


# todo: change it somehow, otherwise xxhash_static fails to create
# because xxhash_src is not evaluated on configure stage
file(MAKE_DIRECTORY ${XXHASH_LIBRARY_DIR})
file(MAKE_DIRECTORY ${XXHASH_INCLUDE_DIR})

add_library(xxhash_static STATIC IMPORTED GLOBAL)
set_target_properties(xxhash_static PROPERTIES
    IMPORTED_LOCATION ${XXHASH_LIBRARY_DIR}/libxxhash.a
    INTERFACE_INCLUDE_DIRECTORIES ${XXHASH_INCLUDE_DIR}
)

# Add a dependency to ensure libXXHASH is built before your target
add_dependencies(xxhash_static xxhash_src)
