include(FetchContent)

# Includes are written relative to src/.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(QZDL_TINYTK_VERSION v0.1.3)

qzdl_cache_source(tinytk https://github.com/spacebub/tinytk.git ${QZDL_TINYTK_VERSION})

FetchContent_Declare(tinytk
        GIT_REPOSITORY https://github.com/spacebub/tinytk.git
        GIT_TAG ${QZDL_TINYTK_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)

set(TTK_DOWNLOAD_CACHE "${QZDL_DOWNLOAD_CACHE}")

FetchContent_MakeAvailable(tinytk)

add_subdirectory(external)
add_subdirectory(core)
add_subdirectory(gui)
