cmake_minimum_required(VERSION 3.25)
project(qzdl
        VERSION 4.4.16
        DESCRIPTION "Launcher for Doom engine source ports"
        HOMEPAGE_URL "https://github.com/spacebub/qzdl"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(SANITIZE "Build with the address and undefined sanitizers" OFF)
option(QZDL_RELEASE "Build as a release: no git revision beside the version" OFF)
option(QZDL_BENCHMARKS "Build the benchmark suite in bench/" OFF)
option(QZDL_TESTS "Build the test suite in tests/" OFF)
option(QZDL_PGO_HARNESS "Build qzdl_pgo, the program that trains a profile for QZDL_PGO_USE" OFF)
set(QZDL_PGO_GENERATE "" CACHE PATH "Instrument everything to write raw profiles into this directory")
set(QZDL_PGO_USE "" CACHE FILEPATH "Optimise everything with this merged profile, see tools/pgo.sh")
set(QZDL_PGO_CS_GENERATE "" CACHE PATH
        "With QZDL_PGO_USE: instrument again after inlining, writing raw profiles into this directory")

add_compile_definitions(
        QZDL_VERSION="${PROJECT_VERSION}"
        QZDL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        QZDL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        QZDL_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_link_options("$<$<CONFIG:RELEASE>:/LTCG;/OPT:REF;/OPT:ICF;/INCREMENTAL:NO;/DEBUG:NONE>")

    set(COMPILE_FLAGS
            "/W4"
            "/WX"
            "/MP"
            "/GS"
            "$<$<CONFIG:RELEASE>:/O2;/GL;/Gw;/Gy>")
else ()
    set(COMPILE_FLAGS
            "-Wall"
            "-Wextra"
            "-Werror"
            "-Wpedantic"
            "-fstack-protector-strong"
            "$<$<CONFIG:RELEASE>:-ffunction-sections;-fdata-sections;-fvisibility=hidden>")

    # These land after CMAKE_CXX_FLAGS on the command line, where the last one wins, so the
    # default steps aside for a level the user sets there.
    if (NOT CMAKE_CXX_FLAGS MATCHES "(^| )-O")
        list(APPEND COMPILE_FLAGS "$<$<CONFIG:DEBUG>:-Og>" "$<$<CONFIG:RELEASE>:-O2>")
    endif ()

    list(APPEND COMPILE_FLAGS "$<$<CONFIG:RELEASE>:-fno-plt>")

    add_link_options("$<$<CONFIG:RELEASE>:-Wl,--gc-sections;-Wl,--as-needed;-Wl,-O1;-s;-static-libstdc++;-static-libgcc>")
endif ()

# Its -flto=thin would land after one the user sets in CMAKE_CXX_FLAGS.
if (NOT CMAKE_CXX_FLAGS MATCHES "-f(no-)?lto")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif ()

# Before src/, so the fetched libraries are instrumented and optimised with the rest.
if (QZDL_PGO_GENERATE OR QZDL_PGO_USE)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Profile-guided optimisation here needs clang, see pgo/README.md")
    endif ()

    if (QZDL_PGO_GENERATE AND QZDL_PGO_USE)
        message(FATAL_ERROR "QZDL_PGO_GENERATE and QZDL_PGO_USE exclude each other; a second round is QZDL_PGO_CS_GENERATE")
    endif ()

    if (QZDL_PGO_GENERATE)
        # Atomic: the art reader and the log readers count on threads of their own.
        add_compile_options(-fprofile-generate=${QZDL_PGO_GENERATE} -fprofile-update=atomic)
        add_link_options(-fprofile-generate=${QZDL_PGO_GENERATE})
    endif ()

    if (QZDL_PGO_USE)
        # A function the training never reached, or one edited since, is a warning here and
        # not an error. The optimiser reports both under backend-plugin.
        add_compile_options(-fprofile-use=${QZDL_PGO_USE}
                -Wno-error=backend-plugin
                -Wno-error=profile-instr-unprofiled
                -Wno-error=profile-instr-out-of-date)
        add_link_options(-fprofile-use=${QZDL_PGO_USE})

        if (QZDL_PGO_CS_GENERATE)
            add_compile_options(-fcs-profile-generate=${QZDL_PGO_CS_GENERATE})
            add_link_options(-fcs-profile-generate=${QZDL_PGO_CS_GENERATE})
        endif ()
    endif ()
endif ()

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

    # Runtime options and suppressions are compiled in. ASAN_OPTIONS still overrides.
    # It defines symbols, so it is a source of the binary rather than part of the interface target.
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp SANITIZER_SUPPRESSIONS)
    configure_file(cmake/Sanitizers.cpp.in ${CMAKE_BINARY_DIR}/Sanitizers.cpp @ONLY)

    set(SANITIZER_SOURCES ${CMAKE_BINARY_DIR}/Sanitizers.cpp)
endif ()

# One self-contained binary. This keeps yyjson static.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(QZDL_TINYTK_VERSION v1.8.0)

include(cmake/GitRevision.cmake)

add_subdirectory(src)

if (QZDL_BENCHMARKS)
    add_subdirectory(bench)
endif ()

# The training program drives the same headless rig the tests do.
if (QZDL_TESTS OR QZDL_PGO_HARNESS)
    if (QZDL_TESTS)
        enable_testing()
    endif ()

    add_subdirectory(tests)
endif ()

if (QZDL_PGO_HARNESS)
    add_subdirectory(pgo)
endif ()
