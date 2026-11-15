add_library(qzdl_core STATIC
        # config
        config/Config.cpp
        config/Config.h
        config/ConfigPaths.cpp
        config/ConfigPaths.h
        config/Import.cpp
        config/Import.h
        config/Profile.cpp
        config/Profile.h
        config/Schema.h
        config/Session.cpp
        config/Session.h

        # launch
        launch/Arguments.cpp
        launch/Arguments.h
        launch/Command.cpp
        launch/Command.h
        launch/Dialect.cpp
        launch/Dialect.h
        launch/dialects/Boom.cpp
        launch/dialects/Chocolate.cpp
        launch/dialects/Classic.cpp
        launch/dialects/Dialects.h
        launch/dialects/Helion.cpp
        launch/dialects/Woof.cpp
        launch/dialects/Zdoom.cpp
        launch/Dos.cpp
        launch/Dos.h
        launch/DosFiles.cpp
        launch/DosFiles.h
        launch/Flags.cpp
        launch/Flags.h
        launch/Launcher.cpp
        launch/Launcher.h
        launch/Netgame.cpp
        launch/Netgame.h
        launch/PortLine.h
        launch/PortSaves.cpp
        launch/PortSaves.h
        launch/Rules.cpp
        launch/Rules.h
        launch/Storage.cpp
        launch/Storage.h

        # ports
        ports/Archive.cpp
        ports/Archive.h
        ports/Catalog.cpp
        ports/Catalog.h
        ports/Detect.cpp
        ports/Detect.h
        ports/Install.cpp
        ports/Install.h
        ports/Releases.cpp
        ports/Releases.h

        # util
        util/Ansi.cpp
        util/Ansi.h
        util/FileStamp.cpp
        util/FileStamp.h
        util/Ini.cpp
        util/Ini.h
        util/Md5.h
        util/Sha256.h
        util/Sparse.h

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
        ttk::json
        PRIVATE
        qzdl_sanitizers
        miniz
        stb_image
        chocobo1)

if (WIN32)
    target_sources(qzdl_core PRIVATE
            launch/Association.cpp
            launch/Association.h
            launch/Shortcut.cpp
            launch/Shortcut.h)

    target_link_libraries(qzdl_core PRIVATE shell32 ole32 uuid advapi32)
endif ()

target_compile_options(qzdl_core PRIVATE ${COMPILE_FLAGS})
