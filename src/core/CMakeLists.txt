# Everything ZDL knows how to do, and nothing about how it is shown. No Qt
# reaches in here: the interface is one consumer of this library, and a test or
# a command line front end would be another.
add_library(qzdl_core STATIC
        Config.h
        Config.cpp
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
        Md5.cpp
        Paths.h
        Paths.cpp
        Process.h
        Process.cpp
        Profile.h
        Profile.cpp
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
        miniz)

target_compile_options(qzdl_core PRIVATE ${COMPILE_FLAGS})
