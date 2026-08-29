set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (QT5)
    find_package(Qt5
            REQUIRED COMPONENTS
            Core
            Gui
            Widgets)
else ()
    find_package(Qt6
            REQUIRED COMPONENTS
            Core
            Gui
            Widgets)
endif ()

add_executable(qzdl
        # Third party, vendored
        external/miniz/miniz.h
        external/miniz/miniz.c

        # Entry point
        main.cpp

        # Core
        core/ZDLNullDevice.h
        core/zdlcommon.h

        # Configuration
        config/ZDLConfigModel.cpp
        config/ZDLConfigModel.h
        config/ZDLConfiguration.cpp
        config/ZDLConfiguration.h
        config/ZDLConfigurationManager.cpp
        config/ZDLConfigurationManager.h
        config/ZDLIniImport.cpp
        config/ZDLIniImport.h
        config/ZDLJson.cpp
        config/ZDLJson.h
        config/ZDLProfile.cpp
        config/ZDLProfile.h

        # Legacy INI format, read for migration and .zdl files
        config/ini/zdlconf.cpp
        config/ini/zdlconf.hpp
        config/ini/zdlline.cpp
        config/ini/zdlline.hpp
        config/ini/zdlsection.cpp
        config/ini/zdlsection.hpp

        # Doom data files
        wad/ZDLFileInfo.cpp
        wad/ZDLFileInfo.h
        wad/ZDLMapFile.cpp
        wad/ZDLMapFile.h
        wad/ZLibDir.cpp
        wad/ZLibDir.h
        wad/ZLibPK3.cpp
        wad/ZLibPK3.h
        wad/libwad.cpp
        wad/libwad.h

        # User interface
        ui/ZDLFilePane.cpp
        ui/ZDLFilePane.h
        ui/ZDLInputWidgets.cpp
        ui/ZDLInputWidgets.h
        ui/ZDLInterface.cpp
        ui/ZDLInterface.h
        ui/ZDLMainWindow.cpp
        ui/ZDLMainWindow.h
        ui/ZDLMultiPane.cpp
        ui/ZDLMultiPane.h
        ui/ZDLQSplitter.cpp
        ui/ZDLQSplitter.h
        ui/ZDLSettingsPane.cpp
        ui/ZDLSettingsPane.h
        ui/ZDLSettingsTab.cpp
        ui/ZDLSettingsTab.h
        ui/ZDLWidget.cpp
        ui/ZDLWidget.h

        # Dialogs
        ui/dialogs/ZDLAboutDialog.cpp
        ui/dialogs/ZDLAboutDialog.h
        ui/dialogs/ZDLImportDialog.cpp
        ui/dialogs/ZDLImportDialog.h
        ui/dialogs/ZDLNameInput.cpp
        ui/dialogs/ZDLNameInput.h

        # List widgets
        ui/lists/ZDLFileList.cpp
        ui/lists/ZDLFileList.h
        ui/lists/ZDLFileListable.cpp
        ui/lists/ZDLFileListable.h
        ui/lists/ZDLIWadList.cpp
        ui/lists/ZDLIWadList.h
        ui/lists/ZDLListWidget.cpp
        ui/lists/ZDLListWidget.h
        ui/lists/ZDLListable.cpp
        ui/lists/ZDLListable.h
        ui/lists/ZDLNameListable.cpp
        ui/lists/ZDLNameListable.h
        ui/lists/ZDLSourcePortList.cpp
        ui/lists/ZDLSourcePortList.h)

target_link_libraries(qzdl
        PRIVATE
        Qt::Core
        Qt::Gui
        Qt::Widgets
        yyjson)

# Kept target-scoped so that -Werror / /W4 never reach third party sources.
if(MSVC)
    target_compile_options(qzdl
            PRIVATE
            "/W4" "/MP" "/GS" "$<$<CONFIG:RELEASE>:/O1;/GL;/Gw;/Gy>")
else()
    target_compile_options(qzdl
            PRIVATE
            "-Wall" "-Wextra" "-Werror" "-fstack-protector-strong" "$<$<CONFIG:DEBUG>:-Og>")
endif()

target_include_directories(qzdl
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        external/miniz
        xpm)

if (WIN32)
    target_sources(qzdl
            PRIVATE
            resources/Win32/resource.h
            resources/Win32/qzdl.rc
            platform/win32/SimpleWFA.h
            platform/win32/SimpleWFA.cpp
            platform/win32/ZDLFileAssociations.h
            platform/win32/ZDLFileAssociations.cpp)
endif ()

set_target_properties(qzdl
        PROPERTIES
        OUTPUT_NAME ZDL)