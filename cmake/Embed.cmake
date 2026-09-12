# Compiles a file in as `Embedded::<symbol>` and `Embedded::<symbol>Size`, so
# nothing has to be found on disk at runtime.
function(qzdl_embed target symbol input)
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/embed/${symbol}.cpp")

    add_custom_command(
            OUTPUT ${generated}
            COMMAND ${CMAKE_COMMAND}
            -DQZDL_EMBED_INPUT=${input}
            -DQZDL_EMBED_OUTPUT=${generated}
            -DQZDL_EMBED_SYMBOL=${symbol}
            -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
            DEPENDS ${input} ${CMAKE_SOURCE_DIR}/cmake/EmbedFile.cmake
            COMMENT "Embedding ${symbol}"
            VERBATIM)

    target_sources(${target} PRIVATE ${generated})
endfunction()
