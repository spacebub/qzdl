include(FetchContent)

# --- SDL3: the window, the input and one framebuffer to draw into --------------

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

foreach (part AUDIO CAMERA JOYSTICK HAPTIC HIDAPI SENSOR POWER GPU DIALOG)
    set(SDL_${part} OFF CACHE BOOL "" FORCE)
endforeach ()

set(QZDL_SDL_VERSION release-3.2.24)

qzdl_cache_source(SDL3 https://github.com/libsdl-org/SDL.git ${QZDL_SDL_VERSION})

# SDL_RENDER and OpenGL stay on, and not by accident. Only the Windows and X11
# video drivers implement a framebuffer of their own; everywhere else
# SDL_GetWindowSurface falls back to SDL_CreateWindowTexture, which is the 2D
# render API over a GPU texture. Turning these off would trade a slower Wayland
# and macOS for a smaller binary, and would cost the fallback entirely.
FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${QZDL_SDL_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
        SYSTEM)

FetchContent_MakeAvailable(SDL3)

# --- asmjit: Blend2D's JIT backend, compiled into Blend2D itself ---------------

# Blend2D bundles asmjit rather than linking one: it configures it for its own use
# and, when it does, can avoid a dependency on the C++ standard library. Building
# asmjit as a project of its own here would produce a second, differently
# configured copy, so this populates the source and stops. A SOURCE_SUBDIR holding
# no CMakeLists.txt is the documented way to say so.
set(QZDL_ASMJIT_VERSION e24347521858616d2d070375ad0d52512e6e44ba)

qzdl_cache_source(asmjit https://github.com/asmjit/asmjit.git ${QZDL_ASMJIT_VERSION})

FetchContent_Declare(asmjit
        GIT_REPOSITORY https://github.com/asmjit/asmjit.git
        GIT_TAG ${QZDL_ASMJIT_VERSION}
        SOURCE_SUBDIR populate-only)

FetchContent_MakeAvailable(asmjit)

set(ASMJIT_DIR "${asmjit_SOURCE_DIR}" CACHE PATH "" FORCE)

# --- Blend2D: the rasteriser the interface is drawn with ------------------------

set(BLEND2D_STATIC ON CACHE BOOL "" FORCE)
set(BLEND2D_TEST OFF CACHE BOOL "" FORCE)
set(BLEND2D_NO_INSTALL ON CACHE BOOL "" FORCE)
set(BLEND2D_NO_JIT_LOGGING ON CACHE BOOL "" FORCE)

set(QZDL_BLEND2D_VERSION 58ca9460b4138af6e793184682e9324e0a8053b2)  # v0.21.3

qzdl_cache_source(blend2d https://github.com/blend2d/blend2d.git ${QZDL_BLEND2D_VERSION})

FetchContent_Declare(blend2d
        GIT_REPOSITORY https://github.com/blend2d/blend2d.git
        GIT_TAG ${QZDL_BLEND2D_VERSION}
        EXCLUDE_FROM_ALL
        SYSTEM)

FetchContent_MakeAvailable(blend2d)

# --- the interface -------------------------------------------------------------

add_executable(qzdl_gui
        qzdl_gui.cpp
        # app: the application and the window it runs in
        app/App.cpp
        app/App.h
        app/Shell.cpp
        app/Shell.h
        # draw: the rasteriser, and what is drawn with it
        draw/Anim.cpp
        draw/Anim.h
        draw/Chrome.h
        draw/Glyphs.cpp
        draw/Glyphs.h
        draw/Paint.cpp
        draw/Paint.h
        draw/Surface.cpp
        draw/Surface.h
        draw/Svg.cpp
        draw/Svg.h
        draw/Theme.cpp
        draw/Theme.h
        draw/Typeface.cpp
        draw/Typeface.h
        # toolkit: the widget layer. Nothing in it knows what ZDL4 is.
        toolkit/Event.h
        toolkit/Painter.cpp
        toolkit/Painter.h
        toolkit/Root.cpp
        toolkit/Root.h
        toolkit/Widget.cpp
        toolkit/Widget.h
        # toolkit/layout: what places other widgets
        toolkit/layout/Box.cpp
        toolkit/layout/Box.h
        toolkit/layout/Pair.cpp
        toolkit/layout/Pair.h
        toolkit/layout/Panel.cpp
        toolkit/layout/Panel.h
        toolkit/layout/Picture.cpp
        toolkit/layout/Picture.h
        toolkit/layout/Scroll.cpp
        toolkit/layout/Scroll.h
        toolkit/layout/Spacer.cpp
        toolkit/layout/Spacer.h
        # toolkit/controls: what the pointer operates
        toolkit/controls/Button.cpp
        toolkit/controls/Button.h
        toolkit/controls/Check.cpp
        toolkit/controls/Check.h
        toolkit/controls/Fact.cpp
        toolkit/controls/Fact.h
        toolkit/controls/Field.cpp
        toolkit/controls/Field.h
        toolkit/controls/GlyphButton.cpp
        toolkit/controls/GlyphButton.h
        toolkit/controls/Label.cpp
        toolkit/controls/Label.h
        toolkit/controls/Pill.cpp
        toolkit/controls/Pill.h
        toolkit/controls/Segmented.cpp
        toolkit/controls/Segmented.h
        toolkit/controls/Select.cpp
        toolkit/controls/Select.h
        toolkit/controls/Stepper.cpp
        toolkit/controls/Stepper.h
        toolkit/controls/TextBox.cpp
        toolkit/controls/TextBox.h
        toolkit/controls/Toggle.cpp
        toolkit/controls/Toggle.h
        # toolkit/overlays: what comes up over a page
        toolkit/overlays/Menu.cpp
        toolkit/overlays/Menu.h
        toolkit/overlays/Sheet.cpp
        toolkit/overlays/Sheet.h
        toolkit/overlays/Tips.cpp
        toolkit/overlays/Tips.h
        # util: leaf helpers, with nothing above them
        util/Clipboard.cpp
        util/Clipboard.h
        util/Desktop.cpp
        util/Desktop.h
        util/Format.cpp
        util/Format.h
        util/Http.cpp
        util/Http.h
        # model: what the interface is of. Nothing here draws.
        model/Bridge.h
        model/ConfigBridge.cpp
        model/ConfigBridge.h
        model/Engines.cpp
        model/Engines.h
        model/IwadArt.cpp
        model/IwadArt.h
        model/LibraryBridge.cpp
        model/LibraryBridge.h
        model/ListsBridge.cpp
        model/ListsBridge.h
        model/Notifier.cpp
        model/Notifier.h
        model/Picker.cpp
        model/Picker.h
        model/ProfileBridge.cpp
        model/ProfileBridge.h
        model/ProfilePanels.cpp
        model/ProfilePanels.h
        model/RunLog.cpp
        model/RunLog.h
        model/Runs.cpp
        model/Runs.h
        model/SettingsBridge.cpp
        model/SettingsBridge.h
        model/State.cpp
        model/State.h
        # pages: one per page the user can be on
        pages/Engines.cpp
        pages/Engines.h
        pages/Library.cpp
        pages/Library.h
        pages/Profile.cpp
        pages/Profile.h
        pages/Settings.cpp
        pages/Settings.h
        # components: everything else that is on screen
        components/Frame.cpp
        components/Frame.h
        components/LibraryCard.cpp
        components/LibraryCard.h
        components/LogDock.cpp
        components/LogDock.h
        components/sheets/AboutSheet.cpp
        components/sheets/AboutSheet.h
        components/sheets/CommandSheet.cpp
        components/sheets/CommandSheet.h
        components/sheets/ConfirmSheet.cpp
        components/sheets/ConfirmSheet.h
        components/sheets/CopyConfigSheet.cpp
        components/sheets/CopyConfigSheet.h
        components/sheets/EntrySheet.cpp
        components/sheets/EntrySheet.h
        components/sheets/PickSheet.cpp
        components/sheets/PickSheet.h
        components/sheets/PromptSheet.cpp
        components/sheets/PromptSheet.h
        components/sheets/SheetLayer.cpp
        components/sheets/SheetLayer.h
        components/Toasts.cpp
        components/Toasts.h
        components/TitleBar.cpp
        components/TitleBar.h
        ${SANITIZER_SOURCES})

set_target_properties(qzdl_gui PROPERTIES
        OUTPUT_NAME ZDL4
        WIN32_EXECUTABLE ON)

target_link_libraries(qzdl_gui PRIVATE
        qzdl_core
        qzdl_sanitizers
        blend2d::blend2d
        SDL3::SDL3-static)

if (WIN32)
    target_sources(qzdl_gui PRIVATE draw/Chrome_Win32.cpp)

    target_link_libraries(qzdl_gui PRIVATE winhttp dwmapi shell32 user32)

    target_sources(qzdl_gui PRIVATE
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/resource.h
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/qzdl.rc
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/compat.manifest)

    # WIN32_EXECUTABLE expects WinMain; nothing defines one.
    if (MSVC)
        target_link_options(qzdl_gui PRIVATE /ENTRY:mainCRTStartup)
    endif ()
else ()
    target_sources(qzdl_gui PRIVATE draw/Chrome_Stub.cpp)

    find_package(CURL REQUIRED)

    target_link_libraries(qzdl_gui PRIVATE CURL::libcurl)
endif ()

if (SANITIZE)
    target_compile_definitions(qzdl_gui PRIVATE
            QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ZDL4")
endif ()

target_compile_definitions(qzdl_gui PRIVATE ZDL_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

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
