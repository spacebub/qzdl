# Every include in the tree is written from here, so "core/Config.h" reads the
# same whichever directory it is written in.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(external)
add_subdirectory(core)
add_subdirectory(gui)
