cmake_minimum_required(VERSION 3.24)
project(qzdl
        DESCRIPTION "Launcher for ZDoom based Doom engine source ports"
        HOMEPAGE_URL "https://github.com/spacebub/qzdl"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(QT5 "Build with Qt5 instead of Qt6" OFF)
option(MSVC_STATIC "Enable static linking for msvc builds" ON)

# Must be set before FetchContent_MakeAvailable so that dependencies pick up the
# same runtime.  Warning flags are applied per-target in src/CMakeLists.txt so
# that -Werror doesn't apply to third party sources.
if(MSVC)
    if(MSVC_STATIC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_link_options("$<$<CONFIG:RELEASE>:/LTCG>")
endif()

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
