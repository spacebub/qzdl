# Vendored third party. The warning flags the rest of the tree is built with
# never reach in here: these are not our sources to fix.
add_library(miniz STATIC
        miniz/miniz.h
        miniz/miniz.c)

target_include_directories(miniz SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniz)
