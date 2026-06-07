if(DEFINED DIST_DEVBUILD_DIR)
    file(MAKE_DIRECTORY ${DIST_DEVBUILD_DIR})
endif()

add_custom_target(memhawk_all)

function(copy_to_devbuild target)
    if(TARGET ${target} AND DEFINED DIST_DEVBUILD_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}> ${DIST_DEVBUILD_DIR}
            COMMENT "copying ${target} to devbuild")
        add_custom_target(copy_${target} ALL
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}> ${DIST_DEVBUILD_DIR}
            DEPENDS ${target}
            COMMENT "copying ${target} to devbuild")
        add_dependencies(memhawk_all copy_${target})
    endif()
endfunction()

function(memhawk_build target)
    add_dependencies(memhawk_all ${target})
    copy_to_devbuild(${target})
endfunction()

function(memhawk_devbuild_file target filename)
    if(DEFINED DIST_DEVBUILD_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${filename} ${DIST_DEVBUILD_DIR}
            COMMENT "copying ${filename} after build")
    endif()
endfunction()


set(EMBEDED_PROPERTIES
    INCLUDE_DIRECTORIES
    LINK_LIBRARIES
)

function(embed_library target)
    foreach(dependency ${ARGN})
        foreach(property ${EMBEDED_PROPERTIES})
            set_property(TARGET ${target} APPEND PROPERTY ${property} $<TARGET_GENEX_EVAL:${dependency},$<TARGET_PROPERTY:${dependency},${property}>>)
        endforeach()

        target_sources(${target} PRIVATE $<TARGET_OBJECTS:${dependency}>)
        target_include_directories(${target} 
            PRIVATE
                $<TARGET_PROPERTY:${dependency},SOURCE_DIR>/src
        )
    endforeach()
endfunction()

function(collect_files DIR FILES INCLUDE EXCLUDE)
    set(local)
    file(GLOB_RECURSE local FOLLOW_SYMLINKS RELATIVE ${DIR} ${INCLUDE})
    if (EXCLUDE STREQUAL "")
    else()
        list(FILTER local EXCLUDE REGEX ${EXCLUDE})
    endif()
    set(${FILES} ${local} PARENT_SCOPE)
endfunction()

macro(collect_sources HEADERS SOURCES DIR)
    collect_files(${DIR} HEADERS "*.h" "")
    collect_files(${DIR} SOURCES "*.cpp" "")
endmacro()

macro(collect_project_sources HEADERS SOURCES DIR)
    collect_files(${DIR} HEADERS "*.h" "tests/*")
    collect_files(${DIR} SOURCES "*.cpp" "tests/*")
endmacro()
