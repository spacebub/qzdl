cmake_minimum_required(VERSION 3.25)
project(qzdl
        VERSION 4.3.2
        DESCRIPTION "Launcher for Doom engine source ports"
        HOMEPAGE_URL "https://github.com/spacebub/qzdl"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(SANITIZE "Build with the address and undefined sanitizers" OFF)

# The Win32 version resource builds its numeric FILEVERSION out of the parts,
# so nothing carries a version of its own.
add_compile_definitions(
        QZDL_VERSION="${PROJECT_VERSION}"
        QZDL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        QZDL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        QZDL_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if (MSVC)
    # One CRT for everything, statically; the Rust side is told the same in
    # src/slint-gui, so the binary carries no VCRUNTIME/MSVCP DLLs.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # A release is built for size, so COMPILE_FLAGS below asks for /O1: the last
    # of the two is the one cl obeys. CMake puts its own /O2 in ahead of it, which
    # changes nothing but warns D9025 on every file, so it is taken back out here.
    # C only ever gets the one flag, and keeps it: miniz and yyjson are built with
    # CMAKE_C_FLAGS_RELEASE alone.
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

    # Release: drop what nothing reaches, strip, and carry the C++ runtime
    # inside rather than depend on the build machine's version of it.
    if (APPLE)
        add_link_options("$<$<CONFIG:RELEASE>:-Wl,-dead_strip>")
    else ()
        list(APPEND COMPILE_FLAGS "$<$<CONFIG:RELEASE>:-fno-plt>")

        add_link_options("$<$<CONFIG:RELEASE>:-Wl,--gc-sections;-Wl,--as-needed;-Wl,-O1;-s;-static-libstdc++;-static-libgcc>")
    endif ()
endif ()

# C++ side only: the Rust objects arrive already LTO'd by cargo.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_library(qzdl_sanitizers INTERFACE)

if (SANITIZE)
    set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined)

    # -Og folds enough frames together to make a sanitizer stack point at the
    # wrong function, so an instrumented build is compiled to be read instead.
    target_compile_options(qzdl_sanitizers INTERFACE
            ${SANITIZER_FLAGS}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -g)

    target_link_options(qzdl_sanitizers INTERFACE ${SANITIZER_FLAGS})

    # The runtimes read where to write and what to ignore off hooks in the
    # binary, so all of it travels with the build and there are no environment
    # variables to remember. ASAN_OPTIONS and the rest still override it.
    #
    # It defines symbols, so it belongs to the binaries rather than to the
    # interface above: carried by that, every library that picked the flags
    # up would carry a definition of them too.
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp SANITIZER_SUPPRESSIONS)
    configure_file(cmake/Sanitizers.cpp.in ${CMAKE_BINARY_DIR}/Sanitizers.cpp @ONLY)

    set(SANITIZER_SOURCES ${CMAKE_BINARY_DIR}/Sanitizers.cpp)
endif ()

# ZDL ships as one self-contained binary, so nothing it pulls in is allowed to
# become a shared object. yyjson picks its own kind off this, and a build
# configured with -DBUILD_SHARED_LIBS=ON would otherwise quietly grow a .so to
# carry around.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(cmake/DownloadCache.cmake)
include(cmake/CargoCache.cmake)
include(cmake/GitRevision.cmake)

qzdl_share_cargo_dir()

add_subdirectory(src)
