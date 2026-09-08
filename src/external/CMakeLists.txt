add_library(miniz STATIC
        miniz/miniz.h
        miniz/miniz.c)

# Only what a title screen is ever stored as; the rest of what stb reads is
# compiled out. PUBLIC for the same reason as miniz below: these change what
# the header declares.
add_library(stb_image STATIC
        stb/stb_image.h
        stb/stb_image.c)

target_compile_definitions(stb_image PUBLIC
        STBI_NO_STDIO
        STBI_NO_BMP
        STBI_NO_PSD
        STBI_NO_TGA
        STBI_NO_HDR
        STBI_NO_PIC
        STBI_NO_PNM
        STBI_NO_LINEAR)

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
