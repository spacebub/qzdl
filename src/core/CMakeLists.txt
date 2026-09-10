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

add_library(qzdl_core STATIC
        # config
        config/Config.cpp
        config/Config.h
        config/Import.cpp
        config/Import.h
        config/Profile.cpp
        config/Profile.h
        config/Session.cpp
        config/Session.h

        # launch
        launch/Arguments.cpp
        launch/Arguments.h
        launch/Command.cpp
        launch/Command.h
        launch/Dialect.cpp
        launch/Dialect.h
        launch/Dos.cpp
        launch/Dos.h
        launch/DosFiles.cpp
        launch/DosFiles.h
        launch/Launcher.cpp
        launch/Launcher.h
        launch/Netgame.cpp
        launch/Netgame.h
        launch/Storage.cpp
        launch/Storage.h

        # ports
        ports/Archive.cpp
        ports/Archive.h
        ports/Catalog.cpp
        ports/Catalog.h
        ports/Detect.cpp
        ports/Detect.h
        ports/Releases.cpp
        ports/Releases.h

        # system
        system/Env.cpp
        system/Env.h
        system/Paths.cpp
        system/Paths.h
        system/Process.cpp
        system/Process.h

        # util
        util/Ini.cpp
        util/Ini.h
        util/Json.cpp
        util/Json.h
        util/Md5.h
        util/Text.cpp
        util/Text.h

        # wad
        wad/Artwork.cpp
        wad/Artwork.h
        wad/FileInfo.cpp
        wad/FileInfo.h
        wad/LibDir.cpp
        wad/LibDir.h
        wad/LibPk3.cpp
        wad/LibPk3.h
        wad/MapFile.cpp
        wad/MapFile.h
        wad/Wad.cpp
        wad/Wad.h)

target_link_libraries(qzdl_core
        PUBLIC
        yyjson
        PRIVATE
        miniz
        stb_image
        chocobo1)

target_compile_options(qzdl_core PRIVATE ${COMPILE_FLAGS})
