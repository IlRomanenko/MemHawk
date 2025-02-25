if(DEFINED DIST_DEVBUILD_DIR)
    file(MAKE_DIRECTORY ${DIST_DEVBUILD_DIR})
endif()

function(copy_to_devbuild target)
    if(TARGET ${target} AND DEFINED DIST_DEVBUILD_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}> ${DIST_DEVBUILD_DIR}
            COMMENT "copying ${target} after build")
    endif()
endfunction()

add_custom_target(memhawk_all)

function(memhawk_build target)
    add_dependencies(memhawk_all ${target})
    copy_to_devbuild(${target})
endfunction()
