include(FetchContent)

# --- Google Benchmark ----------------------------------------------------------

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
set(BENCHMARK_USE_BUNDLED_GTEST OFF CACHE BOOL "" FORCE)

set(QZDL_BENCHMARK_VERSION v1.9.5)

qzdl_cache_source(benchmark https://github.com/google/benchmark.git ${QZDL_BENCHMARK_VERSION})

FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG ${QZDL_BENCHMARK_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
        SYSTEM)

FetchContent_MakeAvailable(benchmark)

# --- the interface's sources, compiled a second time ---------------------------

# qzdl_gui is an executable, so there is no library to link: its source list is
# read off the target, less what is generated, platform-specific or holds main().
get_target_property(QZDL_GUI_SOURCES qzdl_gui SOURCES)
get_target_property(QZDL_GUI_DIR qzdl_gui SOURCE_DIR)

set(QZDL_GUI_REUSED "")

foreach (source IN LISTS QZDL_GUI_SOURCES)
    if (NOT source MATCHES "\\.cpp$")
        continue()
    endif ()

    if (IS_ABSOLUTE "${source}" OR source STREQUAL "qzdl_gui.cpp")
        continue()
    endif ()

    list(APPEND QZDL_GUI_REUSED "${QZDL_GUI_DIR}/${source}")
endforeach ()

list(LENGTH QZDL_GUI_REUSED QZDL_GUI_REUSED_COUNT)

if (QZDL_GUI_REUSED_COUNT LESS 40)
    message(FATAL_ERROR
            "Only ${QZDL_GUI_REUSED_COUNT} sources were read off qzdl_gui; the target has changed shape.")
endif ()

# --- the benchmarks ------------------------------------------------------------

add_executable(qzdl_bench
        Main.cpp

        support/Canvas.cpp
        support/Canvas.h
        support/Corpus.cpp
        support/Corpus.h
        support/Fixtures.cpp
        support/Fixtures.h
        support/Rig.cpp
        support/Rig.h
        support/Sandbox.cpp
        support/Sandbox.h

        core/Config_bench.cpp
        core/Ini_bench.cpp
        core/Json_bench.cpp
        core/Launch_bench.cpp
        core/Text_bench.cpp
        core/Wad_bench.cpp

        gui/Anim_bench.cpp
        gui/Bridges_bench.cpp
        gui/Card_bench.cpp
        gui/Controls_bench.cpp
        gui/Glyphs_bench.cpp
        gui/Layout_bench.cpp
        gui/Painter_bench.cpp
        gui/State_bench.cpp
        gui/Typeface_bench.cpp

        workflows/Frame_bench.cpp
        workflows/Library_bench.cpp
        workflows/Profile_bench.cpp
        workflows/Settings_bench.cpp
        workflows/Startup_bench.cpp

        ${QZDL_GUI_REUSED}
        ${SANITIZER_SOURCES})

target_include_directories(qzdl_bench PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/generated)

# App.cpp includes the header qzdl_git_revision writes for qzdl_gui.
add_dependencies(qzdl_bench qzdl_revision)

target_link_libraries(qzdl_bench PRIVATE
        qzdl_core
        qzdl_sanitizers
        benchmark::benchmark
        blend2d::blend2d
        SDL3::SDL3-static)

if (WIN32)
    target_link_libraries(qzdl_bench PRIVATE winhttp dwmapi shell32 user32)
else ()
    find_package(CURL REQUIRED)

    target_link_libraries(qzdl_bench PRIVATE CURL::libcurl)
endif ()

if (SANITIZE)
    target_compile_definitions(qzdl_bench PRIVATE
            QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qzdl_bench")
endif ()

qzdl_embed(qzdl_bench mark128 ${CMAKE_SOURCE_DIR}/assets/qzdl-128.png)
qzdl_embed(qzdl_bench mark256 ${CMAKE_SOURCE_DIR}/assets/qzdl-256.png)

target_compile_options(qzdl_bench PRIVATE ${COMPILE_FLAGS})
