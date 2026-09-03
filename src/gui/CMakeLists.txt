find_package(Qt6 REQUIRED COMPONENTS
        Core
        Gui
        Qml
        Quick
        QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

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
        ConfigBridge.h
        ConfigBridge.cpp
        FileList.h
        FileList.cpp
        NameList.h
        NameList.cpp
        Notifier.h
        Theme.h)

target_link_libraries(qzdl_backend
        PUBLIC
        qzdl_core
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
        components/Check.qml
        components/Collection.qml
        components/Crumb.qml
        components/EmptyState.qml
        components/Fact.qml
        components/Field.qml
        components/FileGlyph.qml
        components/Glyph.qml
        components/GlyphButton.qml
        components/Hint.qml
        components/PathField.qml
        components/PathLabel.qml
        components/Picker.qml
        components/Pill.qml
        components/SectionLabel.qml
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
        views/CommandSheet.qml
        views/ConfirmSheet.qml
        views/EntrySheet.qml
        views/LaunchView.qml
        views/PickSheet.qml
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
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/resource.h
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/qzdl.rc)
endif ()

if (SANITIZE)
    target_compile_definitions(qzdl_gui PRIVATE
            QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ZDL")
endif ()

set_target_properties(qzdl_gui PROPERTIES OUTPUT_NAME ZDL)
