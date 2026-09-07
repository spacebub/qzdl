find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Network
        Qml
        Quick
        QuickControls2)

qt_standard_project_setup(REQUIRES 6.9)

# Every module lands under its own URI path, which is what the tooling expects.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

# What the interface talks to. Everything below this line is Qt; everything
# it calls into is not.
qt_add_library(qzdl_backend STATIC)

qt_policy(SET QTP0004 NEW)
qt_add_qml_module(qzdl_backend
        URI Zdl
        VERSION 1.0
        SOURCES
        App.h
        App.cpp
        Browse.h
        Browse.cpp
        ConfigBridge.h
        ConfigBridge.cpp
        FileList.h
        FileList.cpp
        IwadArt.h
        IwadArt.cpp
        NameList.h
        NameList.cpp
        Notifier.h
        PathText.h
        ProfileList.h
        ProfileList.cpp
        RunLog.h
        RunLog.cpp
        Runs.h
        Runs.cpp
        Theme.h)

target_link_libraries(qzdl_backend
        PUBLIC
        qzdl_core
        Qt6::Network
        Qt6::Quick)

target_compile_options(qzdl_backend PRIVATE ${COMPILE_FLAGS})

# The pieces every view is drawn from.
qt_add_library(qzdl_components STATIC)

qt_add_qml_module(qzdl_components
        URI Zdl.Components
        VERSION 1.0
        QML_FILES
        components/ActionMenu.qml
        components/AppButton.qml
        components/Buzz.qml
        components/BuzzStack.qml
        components/Card.qml
        components/CardArt.qml
        components/Check.qml
        components/Crumb.qml
        components/EmptyState.qml
        components/Fact.qml
        components/Field.qml
        components/FileGlyph.qml
        components/Glyph.qml
        components/GlyphButton.qml
        components/Hint.qml
        components/LibraryCard.qml
        components/LogDock.qml
        components/PathField.qml
        components/PathLabel.qml
        components/Picker.qml
        components/Pill.qml
        components/RunPill.qml
        components/PlayBadge.qml
        components/SectionLabel.qml
        components/Segmented.qml
        components/Slot.qml
        components/Stepper.qml
        components/Surface.qml
        components/TitleBar.qml
        components/Toggle.qml
        components/Wash.qml)

target_link_libraries(qzdl_components
        PRIVATE
        qzdl_backend
        Qt6::Quick)

# The two pages, and the sheets that stand in for windows.
qt_add_library(qzdl_views STATIC)

qt_add_qml_module(qzdl_views
        URI Zdl.Views
        VERSION 1.0
        QML_FILES
        views/AboutSheet.qml
        views/BrowseView.qml
        views/CommandSheet.qml
        views/ConfirmSheet.qml
        views/EnginesView.qml
        views/EntrySheet.qml
        views/LibraryView.qml
        views/PickSheet.qml
        views/ProfileView.qml
        views/PromptSheet.qml
        views/SettingsView.qml)

target_link_libraries(qzdl_views
        PRIVATE
        qzdl_backend
        qzdl_components
        Qt6::Quick)

# The window itself.
qt_add_executable(qzdl_gui
        qzdl_gui.cpp
        ${SANITIZER_SOURCES})

qt_add_qml_module(qzdl_gui
        URI Zdl.App
        VERSION 1.0
        QML_FILES
        app/Main.qml)

# The launcher reads the icon off the desktop entry, the window off the binary,
# so the same file that gets installed is carried along inside it.
qt_add_resources(qzdl_gui "icon"
        PREFIX "/"
        BASE ${CMAKE_SOURCE_DIR}/assets
        FILES
        ${CMAKE_SOURCE_DIR}/assets/qzdl-128.png
        ${CMAKE_SOURCE_DIR}/assets/qzdl-256.png)

target_link_libraries(qzdl_gui
        PRIVATE
        qzdl_backend
        qzdl_components
        qzdl_views
        qzdl_sanitizers
        Qt6::Quick
        Qt6::QuickControls2)

target_compile_options(qzdl_gui PRIVATE ${COMPILE_FLAGS})

# Against a static Qt the QML modules are plugins that have to be linked in by
# name, which qmlimportscanner works out by reading the import statements. The
# finalizer qt_add_executable installs would reach this on its own; it is
# written out because a static build is something this project promises, and
# because calling it twice is a no-op.
qt_import_qml_plugins(qzdl_gui)

if (WIN32)
    target_sources(qzdl_gui
            PRIVATE
            WindowChrome.h
            WindowChrome.cpp
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/resource.h
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/qzdl.rc
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/compat.manifest)

    target_link_libraries(qzdl_gui PRIVATE dwmapi shell32)
endif ()

if (SANITIZE)
    target_compile_definitions(qzdl_gui PRIVATE
            QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ZDL")
endif ()

set_target_properties(qzdl_gui PROPERTIES
        OUTPUT_NAME ZDL
        WIN32_EXECUTABLE ON)

# -DDEPLOY=ON adds the install target. What it writes under CMAKE_INSTALL_PREFIX
# is the layout Qt's deployment tooling expects and reads back at run time: the
# binary in bin/, the Qt libraries in lib/, the plugins and the QML modules in
# the directories a generated qt.conf points at, so the tree runs on a machine
# with no Qt on it. GNUInstallDirs is what decides bin/ and lib/ here; Qt reads
# the same variables, so overriding one moves both.
if (DEPLOY)
    include(GNUInstallDirs)

    # Deployment on macOS is defined in terms of the bundle: the frameworks and
    # the plugins are copied inside it, and the generator refuses to run for a
    # loose executable. The icon travels in Resources, which is where the bundle
    # looks for the name written into the plist.
    if (APPLE)
        set_target_properties(qzdl_gui PROPERTIES
                MACOSX_BUNDLE ON
                MACOSX_BUNDLE_BUNDLE_NAME ZDL
                MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
                MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
                MACOSX_BUNDLE_GUI_IDENTIFIER io.github.spacebub.qzdl
                MACOSX_BUNDLE_ICON_FILE qzdl.icns)

        target_sources(qzdl_gui PRIVATE ${CMAKE_SOURCE_DIR}/assets/qzdl.icns)

        set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/qzdl.icns
                PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif ()

    # Everywhere else the Qt libraries land next to the binary or one directory
    # over, and an installed binary keeps none of the build tree's rpath, so it
    # is told where to look from where it stands.
    if (UNIX AND NOT APPLE)
        set_target_properties(qzdl_gui PROPERTIES
                INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif ()

    install(TARGETS qzdl_gui
            BUNDLE DESTINATION .
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # The desktop entry and the icons are what a launcher reads the app off; the
    # bundle and the Windows resources carry their own, so only the free desktops
    # need them installed beside the binary.
    if (UNIX AND NOT APPLE)
        install(FILES ${CMAKE_SOURCE_DIR}/assets/qzdl.desktop
                DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

        foreach (size IN ITEMS 32 64 128 256 512 1024)
            install(FILES ${CMAKE_SOURCE_DIR}/assets/qzdl-${size}.png
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
                    RENAME qzdl.png)
        endforeach ()

        install(FILES ${CMAKE_SOURCE_DIR}/assets/qzdl.svg
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    endif ()

    # Against a static Qt there is nothing left to copy: the libraries, the
    # plugins and the compiled QML are already inside the binary, and the
    # generator errors out rather than write a script that would do nothing. So
    # the install above is the whole of it, and the deployment step is only
    # asked for when it has work to do.
    #
    # Nothing here is translated, so the catalogues would be weight for no
    # reading: drop NO_TRANSLATIONS the day there is a .ts file to ship.
    if (QT6_IS_SHARED_LIBS_BUILD)
        qt_generate_deploy_qml_app_script(
                TARGET qzdl_gui
                OUTPUT_SCRIPT QZDL_DEPLOY_SCRIPT
                MACOS_BUNDLE_POST_BUILD
                NO_TRANSLATIONS)

        install(SCRIPT ${QZDL_DEPLOY_SCRIPT})
    endif ()
endif ()
