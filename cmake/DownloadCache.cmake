# FetchContent clones into the build tree, so every new build tree downloads
# the same sources again. They are kept here instead, outside any build tree,
# and reused. Set this empty to fetch into the build tree as before.
set(QZDL_DOWNLOAD_CACHE "${CMAKE_SOURCE_DIR}/.download-cache"
        CACHE PATH "Where fetched sources are kept, shared by every build tree")

# Clone <repo> at <tag> into the cache once, then point FetchContent at it:
# FETCHCONTENT_SOURCE_DIR_<NAME> is what turns its download off. Does nothing
# if the cache is off or the caller already named a source directory.
function(qzdl_cache_source name repo tag)
    string(TOUPPER ${name} upper)
    string(TOLOWER ${name} lower)

    if (NOT QZDL_DOWNLOAD_CACHE OR FETCHCONTENT_SOURCE_DIR_${upper})
        return()
    endif ()

    set(dir "${QZDL_DOWNLOAD_CACHE}/${lower}-${tag}")

    # The marker is written after the clone, so one that was interrupted is
    # not taken for a finished checkout.
    if (NOT EXISTS "${dir}/.cached")
        find_package(Git REQUIRED)
        message(STATUS "Caching ${name} ${tag} in ${dir}")

        file(REMOVE_RECURSE "${dir}")
        execute_process(
                COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${tag}
                --recurse-submodules --shallow-submodules ${repo} ${dir}
                RESULT_VARIABLE result)

        if (NOT result EQUAL 0)
            file(REMOVE_RECURSE "${dir}")
            message(FATAL_ERROR "Could not clone ${name} ${tag} from ${repo}: ${result}")
        endif ()

        file(TOUCH "${dir}/.cached")
    endif ()

    set(FETCHCONTENT_SOURCE_DIR_${upper} "${dir}" PARENT_SCOPE)
endfunction()
