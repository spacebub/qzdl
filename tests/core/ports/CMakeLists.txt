qzdl_test(core_ports Catalog.cpp Detect.cpp Releases.cpp Zip.h)
qzdl_test(core_ports_install Archive.cpp Install.cpp Zip.h)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qzdl_test(core_ports_flatpak Flatpak.cpp)
endif ()
