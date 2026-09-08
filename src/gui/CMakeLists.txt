set(QZDL_SLINT_VERSION "v1.17.1")
set(QZDL_SLINT_UNITS 8)

# download: the prebuilt slint-compiler for this host, off the GitHub
#           release. No Rust build of the compiler, no jemalloc, no make.
# source:   build it from the fetched sources.
# <path>:   a slint-compiler executable you provide.
set(QZDL_SLINT_COMPILER "download" CACHE STRING "download | source | <path to slint-compiler>")

# An installed Slint SDK instead, for development builds. Never for a
# release: the SDK library is shared, carries the interpreter and the
# LinuxKMS backend, and pulls libinput, libgbm and glib along.
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

    # Slint fetches Corrosion itself, into the build tree. The tag it asks for
    # is read back out of its CMakeLists so that clone is cached as well; if
    # the line ever reads differently, Slint fetches it as it always has.
    if (FETCHCONTENT_SOURCE_DIR_SLINT)
        file(READ "${FETCHCONTENT_SOURCE_DIR_SLINT}/api/cpp/CMakeLists.txt" _slint_cpp)

        if (_slint_cpp MATCHES "corrosion-rs/corrosion.git[ \t\r\n]+GIT_TAG[ \t]+([^ \t\r\n]+)")
            qzdl_cache_source(Corrosion https://github.com/corrosion-rs/corrosion.git ${CMAKE_MATCH_1})
        endif ()

        unset(_slint_cpp)
    endif ()

    # SYSTEM: Slint's headers are not ours, so neither are their warnings.
    FetchContent_Declare(Slint
            GIT_REPOSITORY https://github.com/slint-ui/slint.git
            GIT_TAG ${QZDL_SLINT_VERSION}
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR api/cpp
            SYSTEM)

    FetchContent_MakeAvailable(Slint)

    qzdl_share_slint_headers("${QZDL_SLINT_VERSION}-a11y${QZDL_ACCESSIBILITY}")

    # jemalloc's build script trips over the MAKEFLAGS a CMake-generated
    # Makefile hands it (tikv/jemallocator#92).
    if (TARGET slint-compiler)
        corrosion_set_env_vars(slint-compiler "--unset=MAKEFLAGS")
    endif ()

    # A prebuilt compiler: Slint's FetchContent path defines neither the
    # target nor the macro in that case.
    if (SLINT_COMPILER AND NOT TARGET Slint::slint-compiler)
        add_executable(Slint::slint-compiler IMPORTED GLOBAL)
        set_target_properties(Slint::slint-compiler PROPERTIES IMPORTED_LOCATION "${SLINT_COMPILER}")
        include("${slint_SOURCE_DIR}/api/cpp/cmake/SlintMacro.cmake")
    endif ()

    # Size on the Rust side. Cargo reads these as release profile settings;
    # panic=abort is already Slint's own.
    corrosion_set_env_vars(slint_cpp
            CARGO_PROFILE_RELEASE_OPT_LEVEL=s
            CARGO_PROFILE_RELEASE_LTO=fat
            CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1
            CARGO_PROFILE_RELEASE_DEBUG=0)

    # The static CRT, matching CMAKE_MSVC_RUNTIME_LIBRARY in the root.
    if (MSVC)
        corrosion_add_target_rustflags(slint_cpp -Ctarget-feature=+crt-static)
    endif ()
endif ()

add_executable(qzdl_gui
        App.h
        App.cpp
        Engines.h
        Engines.cpp
        ConfigBridge.h
        ConfigBridge.cpp
        Convert.h
        Convert.cpp
        Desktop.h
        Desktop.cpp
        Http.h
        Http.cpp
        IwadArt.h
        IwadArt.cpp
        Notifier.h
        Notifier.cpp
        Picker.h
        Picker.cpp
        RunLog.h
        RunLog.cpp
        Runs.h
        Runs.cpp
        qzdl_gui.cpp
        ${SANITIZER_SOURCES})

# The whole interface hangs off the one window, which the compiler turns into
# one header. The units keep the generated code from being one enormous file.
slint_target_sources(qzdl_gui ui/app/main.slint
        NAMESPACE ui
        COMPILATION_UNITS ${QZDL_SLINT_UNITS})

# The launcher reads the icon off the desktop entry, the window off the binary,
# so the same file that gets installed travels along inside it.
set_property(TARGET qzdl_gui PROPERTY SLINT_EMBED_RESOURCES embed-files)

# Generated by Slint and compiled with our flags: not our warnings.
foreach (unit RANGE 1 ${QZDL_SLINT_UNITS})
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/slint_generated_main_${unit}.cpp
            PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
endforeach ()

# main.h is generated into this directory; as a system directory its contents
# stop counting against -Wpedantic in the units that include it.
target_include_directories(qzdl_gui SYSTEM PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(qzdl_gui
        PRIVATE
        qzdl_core
        qzdl_sanitizers
        Slint::Slint)

# Fetching a source port is the one thing Slint has no answer for. Windows has
# WinHTTP in the box; everywhere else it is libcurl.
if (WIN32)
    target_link_libraries(qzdl_gui PRIVATE winhttp)

    target_sources(qzdl_gui
            PRIVATE
            WindowChrome.h
            WindowChrome.cpp
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/resource.h
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/qzdl.rc
            ${CMAKE_SOURCE_DIR}/src/resources/Win32/compat.manifest)

    # opengl32 and imm32 back femtovg and the winit IME on the Rust side, which
    # asks for neither at link time.
    target_link_libraries(qzdl_gui PRIVATE dwmapi shell32 opengl32 imm32)

    # WIN32_EXECUTABLE puts the binary in the windows subsystem, whose entry point
    # is WinMain. Nothing here defines one, so the CRT is pointed at main instead.
    if (MSVC)
        target_link_options(qzdl_gui PRIVATE /ENTRY:mainCRTStartup)
    endif ()
else ()
    find_package(CURL REQUIRED)

    target_link_libraries(qzdl_gui PRIVATE CURL::libcurl)

    # The one library a static Slint still needs at link time, and which
    # Corrosion does not add for it.
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

# Slint is linked into the binary, so nothing is copied beside it: what goes
# under CMAKE_INSTALL_PREFIX is the executable and, on the free desktops, what
# a launcher reads the app off. GNUInstallDirs is what decides bin/ here.
include(GNUInstallDirs)

# The icon travels in Resources, which is where the bundle looks for the
# name written into the plist.
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

# The bundle and the Windows resources carry their own icon, so only the
# free desktops need the entry and the images installed beside the binary.
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
