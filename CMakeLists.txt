cmake_minimum_required(VERSION 3.24)
project(qzdl
        VERSION 3.2.1
        DESCRIPTION "Launcher for ZDoom based Doom engine source ports"
        HOMEPAGE_URL "https://github.com/spacebub/qzdl"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(GUI "Build graphical interface" ON)

option(SANITIZE "Build with the address and undefined sanitizers" OFF)

option(MSVC_STATIC "Enable static linking for msvc builds" ON)

add_compile_definitions(QZDL_VERSION="${PROJECT_VERSION}")

if (MSVC)
    # Must be set before anything is added so that every target picks up the
    # same runtime.
    if (MSVC_STATIC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif ()

    add_link_options("$<$<CONFIG:RELEASE>:/LTCG>")

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
            "$<$<CONFIG:DEBUG>:-Og>")
endif ()

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
# carry around. Qt is found rather than built here, so its own kind is whatever
# the Qt being linked against was built as.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(FetchContent)
set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_MISC OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_FUZZER OFF CACHE BOOL "" FORCE)
set(YYJSON_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(yyjson
        GIT_REPOSITORY https://github.com/ibireme/yyjson.git
        GIT_TAG 0.12.0
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
        SYSTEM)

FetchContent_MakeAvailable(yyjson)

add_subdirectory(src)
