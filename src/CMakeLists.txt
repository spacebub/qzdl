include(FetchContent)

# Includes are written relative to src/.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

FetchContent_Declare(tinytk
        GIT_REPOSITORY https://github.com/spacebub/tinytk.git
        GIT_TAG ${QZDL_TINYTK_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)

set(TTK_NOTIFY OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(tinytk)

# tinytk compiles its Release code with /GL whatever the consumer wants, and one /GL object
# makes every link it reaches link-time optimised. The later /GL- wins.
if (MSVC AND NOT QZDL_LTO)
    foreach (each ttk_system ttk_json ttk_http ttk_notify ttk_ui)
        if (TARGET ${each})
            target_compile_options(${each} PRIVATE "$<$<CONFIG:RELEASE>:/GL->")
        endif ()
    endforeach ()
endif ()

add_subdirectory(external)
add_subdirectory(core)
add_subdirectory(gui)
