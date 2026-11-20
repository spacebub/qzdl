qzdl_test(core_launch_command Arguments.cpp Command.cpp Dialect.cpp Flags.cpp Netgame.cpp Options.cpp Launch.h)
qzdl_test(core_launch_dos Dos.cpp DosFiles.cpp Launch.h)
qzdl_test(core_launch_run Launcher.cpp PortSaves.cpp Storage.cpp Launch.h)

# Windows has no sh to script a stand-in port with, so it gets a program of its own. Windowed, as
# a port started without its output captured gets a console of its own.
if (WIN32)
    add_executable(qzdl_test_recorder WIN32 Recorder.cpp)
    target_link_options(qzdl_test_recorder PRIVATE /ENTRY:mainCRTStartup)

    foreach (each core_launch_command core_launch_dos core_launch_run)
        add_dependencies(qzdl_test_${each} qzdl_test_recorder)
        target_compile_definitions(qzdl_test_${each} PRIVATE QZDL_RECORDER="$<TARGET_FILE:qzdl_test_recorder>")
    endforeach ()
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qzdl_test(core_launch_flatpak Flatpak.cpp Launch.h)
endif ()
