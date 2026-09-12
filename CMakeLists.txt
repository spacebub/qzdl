cmake_minimum_required(VERSION 3.25)
project(qzdl
        VERSION 4.4.2
        DESCRIPTION "Launcher for Doom engine source ports"
        HOMEPAGE_URL "https://github.com/spacebub/qzdl"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(SANITIZE "Build with the address and undefined sanitizers" OFF)

add_compile_definitions(
        QZDL_VERSION="${PROJECT_VERSION}"
        QZDL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        QZDL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        QZDL_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # The /O1 below would warn D9025 against CMake's default /O2.
    string(REPLACE "/O2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    add_link_options("$<$<CONFIG:RELEASE>:/LTCG;/OPT:REF;/OPT:ICF;/INCREMENTAL:NO;/DEBUG:NONE>")

    set(COMPILE_FLAGS
            "/W4"
            "/MP"
            "/GS"
            "$<$<CONFIG:RELEASE>:/O1;/GL;/Gw;/Gy>")
else ()
    set(COMPILE_FLAGS
            "-Wall"
            "-Wextra"
            "-Werror"
            "-Wpedantic"
            "-fstack-protector-strong"
            "$<$<CONFIG:DEBUG>:-Og>"
            "$<$<CONFIG:RELEASE>:-Os;-ffunction-sections;-fdata-sections;-fvisibility=hidden>")

    if (APPLE)
        add_link_options("$<$<CONFIG:RELEASE>:-Wl,-dead_strip>")
    else ()
        list(APPEND COMPILE_FLAGS "$<$<CONFIG:RELEASE>:-fno-plt>")

        add_link_options("$<$<CONFIG:RELEASE>:-Wl,--gc-sections;-Wl,--as-needed;-Wl,-O1;-s;-static-libstdc++;-static-libgcc>")
    endif ()
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_library(qzdl_sanitizers INTERFACE)

if (SANITIZE)
    set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined)

    # -Og folds frames, which misattributes sanitizer stacks.
    target_compile_options(qzdl_sanitizers INTERFACE
            ${SANITIZER_FLAGS}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -g)

    target_link_options(qzdl_sanitizers INTERFACE ${SANITIZER_FLAGS})

    # Runtime options and suppressions are compiled in; ASAN_OPTIONS still overrides.
    # It defines symbols, so it is a source of the binary rather than part of the interface target.
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp SANITIZER_SUPPRESSIONS)
    configure_file(cmake/Sanitizers.cpp.in ${CMAKE_BINARY_DIR}/Sanitizers.cpp @ONLY)

    set(SANITIZER_SOURCES ${CMAKE_BINARY_DIR}/Sanitizers.cpp)
endif ()

# One self-contained binary; this keeps yyjson static.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(cmake/DownloadCache.cmake)
include(cmake/Embed.cmake)
include(cmake/GitRevision.cmake)

add_subdirectory(src)
