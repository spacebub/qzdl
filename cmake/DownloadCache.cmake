# Fetched sources, shared across build trees. Empty fetches into the tree.
set(QZDL_DOWNLOAD_CACHE "${CMAKE_SOURCE_DIR}/.download-cache"
        CACHE PATH "Where fetched sources are kept, shared by every build tree")

# Clones once into the cache and points FetchContent at it through FETCHCONTENT_SOURCE_DIR_<NAME>.
function(qzdl_cache_source name repo tag)
    string(TOUPPER ${name} upper)
    string(TOLOWER ${name} lower)

    if (NOT QZDL_DOWNLOAD_CACHE OR FETCHCONTENT_SOURCE_DIR_${upper})
        return()
    endif ()

    set(dir "${QZDL_DOWNLOAD_CACHE}/${lower}-${tag}")

    # Written after the clone, so an interrupted one is not taken for finished.
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
