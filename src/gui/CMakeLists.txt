add_executable(qzdl_gui
        qzdl_gui.cpp
        # app: the application, and what the window shows
        app/App.cpp
        app/App.h
        app/Views.h
        # draw: the mark and the card art, over what the toolkit draws
        draw/Cards.h
        draw/Mark.cpp
        draw/Mark.h
        draw/Warp.cpp
        draw/Warp.h
        # state: the tree every page reads. Plain data, and it depends on nothing.
        state/State.cpp
        state/State.h
        # model: the config on one side, the state tree on the other. Nothing here draws.
        model/Bridge.h
        model/ConfigBridge.cpp
        model/ConfigBridge.h
        model/LibraryBridge.cpp
        model/LibraryBridge.h
        model/ListsBridge.cpp
        model/ListsBridge.h
        model/ProfileBridge.cpp
        model/ProfileBridge.h
        model/ProfilePanels.cpp
        model/ProfilePanels.h
        model/SettingsBridge.cpp
        model/SettingsBridge.h
        # services: the long-lived ones that own a thread, a process or the network
        services/Engines.cpp
        services/Engines.h
        services/Filters.cpp
        services/Filters.h
        services/IwadArt.cpp
        services/IwadArt.h
        services/RunLog.cpp
        services/RunLog.h
        services/Runs.cpp
        services/Runs.h
        # pages: one per page the user can be on
        pages/EnginesPage.cpp
        pages/EnginesPage.h
        pages/LibraryPage.cpp
        pages/LibraryPage.h
        pages/ProfilePage.cpp
        pages/ProfilePage.h
        pages/SettingsPage.cpp
        pages/SettingsPage.h
        # dialogs: the modal ones, over everything
        dialogs/AboutDialog.cpp
        dialogs/AboutDialog.h
        dialogs/CommandDialog.cpp
        dialogs/CommandDialog.h
        dialogs/CopyConfigDialog.cpp
        dialogs/CopyConfigDialog.h
        dialogs/EntryDialog.cpp
        dialogs/EntryDialog.h
        # components: what is on the window and is not a page. The three the window
        # builds once, Frame, TitleBar and LogDock, are its furniture rather than
        # anything reusable, and Reach is the view's handle on all of the above.
        components/AddonList.cpp
        components/AddonList.h
        components/AddonsPanel.cpp
        components/AddonsPanel.h
        components/CommandPanel.cpp
        components/CommandPanel.h
        components/EngineCard.h
        components/Frame.cpp
        components/Frame.h
        components/LibraryCard.cpp
        components/LibraryCard.h
        components/LogDock.cpp
        components/LogDock.h
        components/NetPanel.cpp
        components/NetPanel.h
        components/Parts.h
        components/ProfileChooser.cpp
        components/ProfileChooser.h
        components/ProfileHead.cpp
        components/ProfileHead.h
        components/Reach.h
        components/ReplayPanel.cpp
        components/ReplayPanel.h
        components/RunPanel.cpp
        components/RunPanel.h
        components/SavesPanel.cpp
        components/SavesPanel.h
        components/TitleBar.cpp
        components/TitleBar.h
        components/Tones.h
        ${SANITIZER_SOURCES})

set_target_properties(qzdl_gui PROPERTIES
        OUTPUT_NAME ZDL4
        WIN32_EXECUTABLE ON)

target_link_libraries(qzdl_gui PRIVATE
        qzdl_core
        qzdl_sanitizers
        ttk::http
        ttk::ui)

if (WIN32)
    target_sources(qzdl_gui PRIVATE
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/resource.h
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/qzdl.rc
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/compat.manifest)

    # WIN32_EXECUTABLE expects WinMain. Nothing defines one.
    if (MSVC)
        target_link_options(qzdl_gui PRIVATE /ENTRY:mainCRTStartup)
    endif ()
endif ()

if (SANITIZE)
    target_compile_definitions(qzdl_gui PRIVATE
            QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ZDL4")
endif ()

ttk_embed(qzdl_gui mark128 ${CMAKE_SOURCE_DIR}/assets/qzdl-128.png)
ttk_embed(qzdl_gui mark256 ${CMAKE_SOURCE_DIR}/assets/qzdl-256.png)

target_compile_options(qzdl_gui PRIVATE ${COMPILE_FLAGS})

qzdl_git_revision(qzdl_gui)

include(GNUInstallDirs)

# Where the bundle looks for the icon named in the plist.
if (APPLE)
    set_target_properties(qzdl_gui PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_BUNDLE_NAME ZDL4
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            MACOSX_BUNDLE_GUI_IDENTIFIER io.github.spacebub.qzdl
            MACOSX_BUNDLE_ICON_FILE qzdl.icns)

    target_sources(qzdl_gui PRIVATE ${CMAKE_SOURCE_DIR}/assets/qzdl.icns)

    set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/qzdl.icns
            PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif ()

install(TARGETS qzdl_gui
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The bundle and the Windows resources carry their own icon.
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
