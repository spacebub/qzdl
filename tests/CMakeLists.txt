include(FetchContent)
include(GoogleTest)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

set(QZDL_GOOGLETEST_VERSION v1.17.0)

ttk_cache_source(googletest https://github.com/google/googletest.git ${QZDL_GOOGLETEST_VERSION})

FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG ${QZDL_GOOGLETEST_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
        SYSTEM)

FetchContent_MakeAvailable(googletest)

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

if (QZDL_GUI_REUSED_COUNT LESS 30)
    message(FATAL_ERROR
            "Only ${QZDL_GUI_REUSED_COUNT} sources were read off qzdl_gui; the target has changed shape.")
endif ()

# The interface compiled once, for every test program to link.
add_library(qzdl_test_gui STATIC ${QZDL_GUI_REUSED})

target_include_directories(qzdl_test_gui PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated)

# App.cpp includes the header qzdl_revision writes for qzdl_gui.
add_dependencies(qzdl_test_gui qzdl_revision)

target_link_libraries(qzdl_test_gui
        PUBLIC
        qzdl_core
        ttk::http
        ttk::ui
        PRIVATE
        qzdl_sanitizers)

target_compile_definitions(qzdl_test_gui PRIVATE QZDL_TINYTK_VERSION="${QZDL_TINYTK_VERSION}")

ttk_embed(qzdl_test_gui mark128 ${CMAKE_SOURCE_DIR}/assets/qzdl-128.png)
ttk_embed(qzdl_test_gui mark256 ${CMAKE_SOURCE_DIR}/assets/qzdl-256.png)
ttk_embed(qzdl_test_gui authors ${CMAKE_SOURCE_DIR}/AUTHORS.md)

target_compile_options(qzdl_test_gui PRIVATE ${COMPILE_FLAGS})

add_library(qzdl_test_support STATIC
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
        support/Shell.cpp
        support/Shell.h)

target_include_directories(qzdl_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(qzdl_test_support
        PUBLIC
        qzdl_test_gui
        GTest::gtest
        PRIVATE
        qzdl_sanitizers)

target_compile_definitions(qzdl_test_support PRIVATE QZDL_TINYTK_VERSION="${QZDL_TINYTK_VERSION}")

target_compile_options(qzdl_test_support PRIVATE ${COMPILE_FLAGS})

# The support library has no main(), so qzdl_pgo can link it. Nothing below here is
# built for the training program.
if (NOT QZDL_TESTS)
    return()
endif ()

# One program per area. Each TEST in it is registered with ctest on its own, so
# `ctest -j` runs them in parallel, each in a sandbox of its own.
function(qzdl_test name)
    add_executable(qzdl_test_${name}
            ${ARGN}
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/support/Main.cpp
            ${SANITIZER_SOURCES})
    target_link_libraries(qzdl_test_${name} PRIVATE qzdl_test_support qzdl_sanitizers)
    target_compile_options(qzdl_test_${name} PRIVATE ${COMPILE_FLAGS})

    if (SANITIZE)
        target_compile_definitions(qzdl_test_${name} PRIVATE
                QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qzdl_test_${name}")
    endif ()

    gtest_discover_tests(qzdl_test_${name} TEST_PREFIX "${name}." DISCOVERY_MODE PRE_TEST)
endfunction()

qzdl_test(harness support/Harness.cpp)

add_subdirectory(core/config)
add_subdirectory(core/util)
add_subdirectory(core/launch)
add_subdirectory(core/ports)
add_subdirectory(core/wad)
add_subdirectory(gui/state)
add_subdirectory(gui/model)
add_subdirectory(gui/draw)
add_subdirectory(gui/services)
add_subdirectory(gui/components)
add_subdirectory(gui/pages)
add_subdirectory(gui/dialogs)
