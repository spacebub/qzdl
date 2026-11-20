qzdl_test(core_launch_command Arguments.cpp Command.cpp Dialect.cpp Flags.cpp Netgame.cpp Options.cpp Launch.h)
qzdl_test(core_launch_dos Dos.cpp DosFiles.cpp Launch.h)
qzdl_test(core_launch_run Launcher.cpp PortSaves.cpp Storage.cpp Launch.h)

qzdl_test_spawns(core_launch_command)
qzdl_test_spawns(core_launch_dos)
qzdl_test_spawns(core_launch_run)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qzdl_test(core_launch_flatpak Flatpak.cpp Launch.h)
endif ()
