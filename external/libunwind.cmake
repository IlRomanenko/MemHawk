include(ExternalProject)

# Download and build libunwind
set(UNWIND_INSTALL_DIR ${CMAKE_BINARY_DIR}/external/libunwind-install)
set(UNWIND_LIBRARY_DIR ${UNWIND_INSTALL_DIR}/lib)
set(UNWIND_INCLUDE_DIR ${UNWIND_INSTALL_DIR}/include)

set(LIBUNWIND_OPTIONS)
list(APPEND LIBUNWIND_OPTIONS "--enable-static" "--disable-shared" "--disable-tests")

# both variants for building with -fPIC
list(APPEND LIBUNWIND_OPTIONS "--enable-pic" "--with-pic")

IF(MEMHAWK_LZMA)
    find_package(lzma REQUIRED)
    list(APPEND LIBUNWIND_OPTIONS "--enable-minidebuginfo")
else()
    list(APPEND LIBUNWIND_OPTIONS "--disable-minidebuginfo")
endif()

IF(MEMHAWK_ZLIB)
    find_package(zlib REQUIRED)
    list(APPEND LIBUNWIND_OPTIONS "--enable-zlibdebuginfo")
else()
    list(APPEND LIBUNWIND_OPTIONS "--disable-zlibdebuginfo")
endif()

IF(MEMHAWK_MSABI)
    list(APPEND LIBUNWIND_OPTIONS "--enable-msabi-support")
else()
    list(APPEND LIBUNWIND_OPTIONS "--disable-msabi-support")
endif()

list(APPEND LIBUNWIND_OPTIONS "--prefix=${UNWIND_INSTALL_DIR}")
set(FLAGS "-O3 -fno-omit-frame-pointer")


message(NOTICE "Enabling `libunwind` dependency")
ExternalProject_Add(
    libunwind_src
    GIT_REPOSITORY https://github.com/libunwind/libunwind.git
    GIT_TAG v1.8.1
    CONFIGURE_COMMAND 
        autoreconf -i && 
        ${CMAKE_COMMAND} -E env 
            CC=${CMAKE_C_COMPILER} CXX=${cmake_CXX_COMPILER} CFLAGS=${FLAGS} CXXFLAGS=${FLAGS} 
            ./configure ${LIBUNWIND_OPTIONS}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE ON
    UPDATE_DISCONNECTED 1
    INSTALL_BYPRODUCTS
        ${UNWIND_LIBRARY_DIR}/libunwind.a
)


# todo: change it somehow, otherwise unwind_static fails to create
# because libunwind_src is not evaluated on configure stage
file(MAKE_DIRECTORY ${UNWIND_LIBRARY_DIR})
file(MAKE_DIRECTORY ${UNWIND_INCLUDE_DIR})

add_library(unwind_static STATIC IMPORTED GLOBAL)
set_target_properties(unwind_static PROPERTIES
    IMPORTED_LOCATION ${UNWIND_LIBRARY_DIR}/libunwind.a
    INTERFACE_INCLUDE_DIRECTORIES ${UNWIND_INCLUDE_DIR}
)

if(MEMHAWK_LZMA)
    find_library(LZMA_LIBRARY lzma REQUIRED)
    set_target_properties(unwind_static PROPERTIES
        INTERFACE_LINK_LIBRARIES "${LZMA_LIBRARY}"
    )
endif()

if(MEMHAWK_ZLIB)
    find_library(ZLIB_LIBRARY zlib REQUIRED)
    set_target_properties(unwind_static PROPERTIES
        INTERFACE_LINK_LIBRARIES "${ZLIB_LIBRARY}"
    )
endif()

# Add a dependency to ensure libunwind is built before your target
add_dependencies(unwind_static libunwind_src)
