# yyjson is core's alone, so it is fetched here and not at the root.
include(FetchContent)

set(QZDL_YYJSON_VERSION 0.12.0)

set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_MISC OFF CACHE BOOL "" FORCE)
set(YYJSON_BUILD_FUZZER OFF CACHE BOOL "" FORCE)
set(YYJSON_INSTALL OFF CACHE BOOL "" FORCE)

qzdl_cache_source(yyjson https://github.com/ibireme/yyjson.git ${QZDL_YYJSON_VERSION})

FetchContent_Declare(yyjson
        GIT_REPOSITORY https://github.com/ibireme/yyjson.git
        GIT_TAG ${QZDL_YYJSON_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
        SYSTEM)

FetchContent_MakeAvailable(yyjson)

# Everything ZDL knows how to do, and nothing about how it is shown. Nothing of
# Slint reaches in here: the interface is one consumer of this library, and a
# test or a command line front end would be another.
add_library(qzdl_core STATIC
        Archive.h
        Archive.cpp
        Artwork.h
        Artwork.cpp
        Catalog.h
        Catalog.cpp
        Config.h
        Config.cpp
        Detect.h
        Detect.cpp
        Env.h
        Env.cpp
        FileInfo.h
        FileInfo.cpp
        Import.h
        Import.cpp
        Ini.h
        Ini.cpp
        Json.h
        Json.cpp
        Launcher.h
        Launcher.cpp
        LibDir.h
        LibDir.cpp
        LibPk3.h
        LibPk3.cpp
        MapFile.h
        MapFile.cpp
        Md5.h
        Paths.h
        Paths.cpp
        Process.h
        Process.cpp
        Profile.h
        Profile.cpp
        Releases.h
        Releases.cpp
        Session.h
        Session.cpp
        Text.h
        Text.cpp
        Wad.h
        Wad.cpp)

target_link_libraries(qzdl_core
        PUBLIC
        yyjson
        PRIVATE
        miniz
        stb_image
        chocobo1)

target_compile_options(qzdl_core PRIVATE ${COMPILE_FLAGS})
