# Written at build time rather than configure time, so it cannot go stale.
# Run with -P to write the header. A release has an empty revision.
if (CMAKE_SCRIPT_MODE_FILE)
    set(revision "")

    if (NOT QZDL_RELEASE)
        set(revision "unknown")

        find_package(Git QUIET)

        if (GIT_FOUND)
            execute_process(
                    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty=-dirty --abbrev=12
                    WORKING_DIRECTORY "${QZDL_SOURCE_DIR}"
                    OUTPUT_VARIABLE described
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE result)

            if (result EQUAL 0 AND described)
                set(revision "${described}")
            endif ()
        endif ()
    endif ()

    set(content "#pragma once\n\n#define QZDL_GIT_REVISION \"${revision}\"\n")

    if (EXISTS "${QZDL_HEADER}")
        file(READ "${QZDL_HEADER}" existing)
    else ()
        set(existing "")
    endif ()

    # Rewriting an identical header would rebuild everything that includes it.
    if (NOT existing STREQUAL content)
        file(WRITE "${QZDL_HEADER}" "${content}")
    endif ()

    return()
endif ()

option(QZDL_RELEASE "Build as a release: no git revision beside the version" OFF)

function(qzdl_git_revision target)
    set(dir "${CMAKE_BINARY_DIR}/generated")
    set(header "${dir}/qzdl_git_revision.h")

    set(command ${CMAKE_COMMAND}
            -DQZDL_HEADER=${header}
            -DQZDL_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DQZDL_RELEASE=${QZDL_RELEASE}
            -P ${CMAKE_CURRENT_FUNCTION_LIST_FILE})

    # Once now so the header is there to include, then again every build.
    file(MAKE_DIRECTORY "${dir}")
    execute_process(COMMAND ${command})

    add_custom_target(qzdl_revision
            BYPRODUCTS "${header}"
            COMMAND ${command}
            COMMENT "Reading the git revision")

    add_dependencies(${target} qzdl_revision)
    target_include_directories(${target} PRIVATE "${dir}")
endfunction()
