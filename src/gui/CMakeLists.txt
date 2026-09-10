set(QZDL_SLINT_VERSION "v1.17.1")
set(QZDL_SLINT_UNITS 8)

# download: the prebuilt slint-compiler off the GitHub release. source: build it. <path>: your own.
set(QZDL_SLINT_COMPILER "download" CACHE STRING "download | source | <path to slint-compiler>")

# Development only: the SDK library is shared and pulls the interpreter, LinuxKMS, libinput, libgbm and glib along.
option(QZDL_SLINT_PACKAGE "Use an installed Slint SDK (find_package) instead of building Slint" OFF)
option(QZDL_ACCESSIBILITY "Compile Slint's accessibility bridge in" OFF)

if (QZDL_SLINT_PACKAGE)
    find_package(Slint REQUIRED)
else ()
    set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_TESTING OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_ACCESSIBILITY ${QZDL_ACCESSIBILITY} CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_FEMTOVG ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)

    if (NOT QZDL_SLINT_COMPILER STREQUAL "source")
        set(SLINT_FEATURE_COMPILER OFF CACHE BOOL "" FORCE)

        if (QZDL_SLINT_COMPILER STREQUAL "download")
            if (CMAKE_HOST_WIN32)
                set(_slint_host "Windows-${CMAKE_HOST_SYSTEM_PROCESSOR}")
                set(_slint_exe ".exe")
            else ()
                set(_slint_host "${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
                set(_slint_exe "")
            endif ()

            if (QZDL_DOWNLOAD_CACHE)
                set(_slint_dir "${QZDL_DOWNLOAD_CACHE}/slint-compiler-${QZDL_SLINT_VERSION}-${_slint_host}")
            else ()
                set(_slint_dir "${CMAKE_BINARY_DIR}/slint-prebuilt")
            endif ()

            set(_slint_url "https://github.com/slint-ui/slint/releases/download/${QZDL_SLINT_VERSION}/slint-compiler-${_slint_host}.tar.gz")

            if (NOT EXISTS "${_slint_dir}/slint-compiler${_slint_exe}")
                message(STATUS "Fetching the Slint compiler from ${_slint_url}")
                file(DOWNLOAD "${_slint_url}" "${_slint_dir}/slint-compiler.tar.gz" STATUS _slint_status)
                list(GET _slint_status 0 _slint_code)

                if (NOT _slint_code EQUAL 0)
                    message(FATAL_ERROR "Could not fetch the Slint compiler: ${_slint_status}")
                endif ()

                file(ARCHIVE_EXTRACT INPUT "${_slint_dir}/slint-compiler.tar.gz" DESTINATION "${_slint_dir}")
                file(REMOVE "${_slint_dir}/slint-compiler.tar.gz")
            endif ()

            set(SLINT_COMPILER "${_slint_dir}/slint-compiler${_slint_exe}" CACHE STRING "" FORCE)
        else ()
            set(SLINT_COMPILER "${QZDL_SLINT_COMPILER}" CACHE STRING "" FORCE)
        endif ()
    endif ()

    include(FetchContent)

    qzdl_cache_source(Slint https://github.com/slint-ui/slint.git ${QZDL_SLINT_VERSION})

    # Slint fetches Corrosion itself; its tag is read out so that clone is cached too.
    if (FETCHCONTENT_SOURCE_DIR_SLINT)
        file(READ "${FETCHCONTENT_SOURCE_DIR_SLINT}/api/cpp/CMakeLists.txt" _slint_cpp)

        if (_slint_cpp MATCHES "corrosion-rs/corrosion.git[ \t\r\n]+GIT_TAG[ \t]+([^ \t\r\n]+)")
            qzdl_cache_source(Corrosion https://github.com/corrosion-rs/corrosion.git ${CMAKE_MATCH_1})
        endif ()

        unset(_slint_cpp)
    endif ()

    FetchContent_Declare(Slint
            GIT_REPOSITORY https://github.com/slint-ui/slint.git
            GIT_TAG ${QZDL_SLINT_VERSION}
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR api/cpp
            SYSTEM)

    FetchContent_MakeAvailable(Slint)

    qzdl_share_slint_headers("${QZDL_SLINT_VERSION}-a11y${QZDL_ACCESSIBILITY}")

    # jemalloc's build script trips over a CMake Makefile's MAKEFLAGS (tikv/jemallocator#92).
    if (TARGET slint-compiler)
        corrosion_set_env_vars(slint-compiler "--unset=MAKEFLAGS")
    endif ()

    # With a prebuilt compiler Slint defines neither the target nor the macro.
    if (SLINT_COMPILER AND NOT TARGET Slint::slint-compiler)
        add_executable(Slint::slint-compiler IMPORTED GLOBAL)
        set_target_properties(Slint::slint-compiler PROPERTIES IMPORTED_LOCATION "${SLINT_COMPILER}")
        include("${slint_SOURCE_DIR}/api/cpp/cmake/SlintMacro.cmake")
    endif ()

    corrosion_set_env_vars(slint_cpp
            CARGO_PROFILE_RELEASE_OPT_LEVEL=s
            CARGO_PROFILE_RELEASE_LTO=fat
            CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1
            CARGO_PROFILE_RELEASE_DEBUG=0)

    # Matches CMAKE_MSVC_RUNTIME_LIBRARY in the root.
    if (MSVC)
        corrosion_add_target_rustflags(slint_cpp -Ctarget-feature=+crt-static)
    endif ()
endif ()

add_executable(qzdl_gui
        Convert.cpp
        Convert.h
        Desktop.cpp
        Desktop.h
        Http.cpp
        Http.h
        Models.h
        qzdl_gui.cpp
        app/App.cpp
        app/App.h
        bridge/Bridge.h
        bridge/ConfigBridge.cpp
        bridge/ConfigBridge.h
        bridge/LibraryBridge.cpp
        bridge/LibraryBridge.h
        bridge/ListsBridge.cpp
        bridge/ListsBridge.h
        bridge/ProfileBridge.cpp
        bridge/ProfileBridge.h
        bridge/ProfilePanels.cpp
        bridge/ProfilePanels.h
        bridge/SettingsBridge.cpp
        bridge/SettingsBridge.h
        components/IwadArt.cpp
        components/IwadArt.h
        components/Notifier.cpp
        components/Notifier.h
        components/RunLog.cpp
        components/RunLog.h
        components/Runs.cpp
        components/Runs.h
        views/Engines.cpp
        views/Engines.h
        views/Picker.cpp
        views/Picker.h
        ${SANITIZER_SOURCES})

# Units keep the generated code from being one enormous file.
slint_target_sources(qzdl_gui ui/app/main.slint
        NAMESPACE ui
        COMPILATION_UNITS ${QZDL_SLINT_UNITS})

# The window icon is read off the binary.
set_property(TARGET qzdl_gui PROPERTY SLINT_EMBED_RESOURCES embed-files)

# Generated code: not our warnings.
foreach (unit RANGE 1 ${QZDL_SLINT_UNITS})
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/slint_generated_main_${unit}.cpp
            PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
endforeach ()

# main.h is generated here; SYSTEM keeps it out of -Wpedantic.
target_include_directories(qzdl_gui SYSTEM PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(qzdl_gui
        PRIVATE
        qzdl_core
        qzdl_sanitizers
        Slint::Slint)

if (WIN32)
    target_link_libraries(qzdl_gui PRIVATE winhttp)

    target_sources(qzdl_gui
            PRIVATE
            app/WindowChrome.h
            app/WindowChrome.cpp
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/resource.h
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/qzdl.rc
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/compat.manifest)

    # femtovg and the winit IME need opengl32 and imm32 without asking for them at link time.
    target_link_libraries(qzdl_gui PRIVATE dwmapi shell32 opengl32 imm32)

    # WIN32_EXECUTABLE expects WinMain; nothing defines one.
    if (MSVC)
        target_link_options(qzdl_gui PRIVATE /ENTRY:mainCRTStartup)
    endif ()
else ()
    find_package(CURL REQUIRED)

    target_link_libraries(qzdl_gui PRIVATE CURL::libcurl)

    # The one library a static Slint still needs that Corrosion does not add.
    if (UNIX AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)

        target_link_libraries(qzdl_gui PRIVATE PkgConfig::FONTCONFIG)
    endif ()
endif ()

if (SANITIZE)
    target_compile_definitions(qzdl_gui PRIVATE
            QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ZDL4")
endif ()

set_target_properties(qzdl_gui PROPERTIES
        OUTPUT_NAME ZDL4
        WIN32_EXECUTABLE ON)

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
