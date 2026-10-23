# The training program for profile-guided optimisation: the interface driven through
# a few sittings against the headless rig in tests/support. See README.md here.
add_executable(qzdl_pgo
        Main.cpp
        Drive.cpp
        Drive.h
        Steps.cpp
        Steps.h
        World.cpp
        World.h
        ${SANITIZER_SOURCES})

target_include_directories(qzdl_pgo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(qzdl_pgo PRIVATE qzdl_test_support qzdl_sanitizers)

if (SANITIZE)
    target_compile_definitions(qzdl_pgo PRIVATE QZDL_SANITIZER_LOG="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qzdl_pgo")
endif ()

target_compile_options(qzdl_pgo PRIVATE ${COMPILE_FLAGS})
