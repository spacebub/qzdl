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

        # A project that publishes no tags is pinned by commit, and --branch takes
        # only a name, so that case is fetched rather than cloned.
        string(LENGTH "${tag}" length)

        if (length EQUAL 40 AND tag MATCHES "^[0-9a-f]+$")
            file(MAKE_DIRECTORY "${dir}")

            execute_process(COMMAND ${GIT_EXECUTABLE} init --quiet ${dir}
                    RESULT_VARIABLE result)

            if (result EQUAL 0)
                execute_process(
                        COMMAND ${GIT_EXECUTABLE} -C ${dir} remote add origin ${repo}
                        RESULT_VARIABLE result)
            endif ()

            if (result EQUAL 0)
                execute_process(
                        COMMAND ${GIT_EXECUTABLE} -C ${dir} fetch --depth 1 origin ${tag}
                        RESULT_VARIABLE result)
            endif ()

            if (result EQUAL 0)
                execute_process(
                        COMMAND ${GIT_EXECUTABLE} -C ${dir} checkout --quiet FETCH_HEAD
                        RESULT_VARIABLE result)
            endif ()

            if (result EQUAL 0)
                execute_process(
                        COMMAND ${GIT_EXECUTABLE} -C ${dir} submodule update --init --recursive
                        --depth 1
                        RESULT_VARIABLE result)
            endif ()
        else ()
            execute_process(
                    COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${tag}
                    --recurse-submodules --shallow-submodules ${repo} ${dir}
                    RESULT_VARIABLE result)
        endif ()

        if (NOT result EQUAL 0)
            file(REMOVE_RECURSE "${dir}")
            message(FATAL_ERROR "Could not fetch ${name} ${tag} from ${repo}: ${result}")
        endif ()

        file(TOUCH "${dir}/.cached")
    endif ()

    set(FETCHCONTENT_SOURCE_DIR_${upper} "${dir}" PARENT_SCOPE)
endfunction()
