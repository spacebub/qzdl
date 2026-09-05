add_library(miniz STATIC
        miniz/miniz.h
        miniz/miniz.c)

add_library(chocobo1 INTERFACE
        chocobo1/Md5.h)

target_include_directories(miniz SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/miniz)
target_include_directories(chocobo1 SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/chocobo1)

# ZDL only ever reads a PK3, so the compressor and the zlib compatibility shim
# are cut out. PUBLIC because these change what miniz.h declares, and the
# library and its callers have to agree on that.
target_compile_definitions(miniz PUBLIC
        MINIZ_NO_DEFLATE_APIS
        MINIZ_NO_ZLIB_APIS)
