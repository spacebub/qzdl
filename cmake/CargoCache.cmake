# Corrosion puts cargo's target directory inside the build tree, so every new
# build tree compiles Slint again. It is kept outside them and shared instead,
# the way fetched sources are. Set this empty to build in the tree as before.
if (QZDL_DOWNLOAD_CACHE)
    set(_qzdl_cargo_default "${QZDL_DOWNLOAD_CACHE}/cargo")
else ()
    set(_qzdl_cargo_default "")
endif ()

set(QZDL_CARGO_CACHE "${_qzdl_cargo_default}"
        CACHE PATH "Where the Rust build is kept, shared by every build tree")

unset(_qzdl_cargo_default)

# Corrosion has no option for the directory, so the one it builds in is made a
# link to the shared one. Cargo keys its artifacts by profile and flags, so
# trees that disagree sit beside each other there rather than rebuild.
function(qzdl_share_cargo_dir)
    set(link "${CMAKE_BINARY_DIR}/cargo")

    if (NOT QZDL_CARGO_CACHE OR EXISTS "${link}" OR IS_SYMLINK "${link}")
        return()
    endif ()

    file(MAKE_DIRECTORY "${QZDL_CARGO_CACHE}")
    file(CREATE_LINK "${QZDL_CARGO_CACHE}" "${link}" SYMBOLIC RESULT result)

    # Windows without developer mode is the case that fails; the build still
    # works, it just builds Rust in the tree as it did before.
    if (result EQUAL 0)
        message(STATUS "Sharing the Rust build in ${QZDL_CARGO_CACHE}")
    else ()
        message(STATUS "Rust builds in the build tree: ${result}")
    endif ()
endfunction()

# cbindgen writes Slint's headers from cargo's build script, into the build
# tree rather than the target directory. A cached Rust build does not rerun it,
# so they are shared alongside it or a reused cache leaves the tree without
# them. The key keeps builds that would generate different headers apart.
function(qzdl_share_slint_headers key)
    if (NOT QZDL_CARGO_CACHE OR NOT slint_BINARY_DIR)
        return()
    endif ()

    set(link "${slint_BINARY_DIR}/generated_include")

    if (EXISTS "${link}" OR IS_SYMLINK "${link}")
        return()
    endif ()

    set(shared "${QZDL_CARGO_CACHE}/generated_include-${key}")

    file(MAKE_DIRECTORY "${shared}")
    file(CREATE_LINK "${shared}" "${link}" SYMBOLIC RESULT result)

    if (NOT result EQUAL 0)
        message(STATUS "Slint's headers stay in the build tree: ${result}")
        return()
    endif ()

    # A cache carried over from before this was shared has the artifacts but
    # not the headers, and cargo will not rerun the script that writes them
    # while its fingerprint still holds. Dropping that fingerprint rebuilds
    # the one crate, once, and fills the shared directory in.
    if (EXISTS "${shared}/private/slint_internal.h")
        return()
    endif ()

    file(GLOB stale LIST_DIRECTORIES TRUE
            "${QZDL_CARGO_CACHE}/*/*/.fingerprint/slint-cpp-*"
            "${QZDL_CARGO_CACHE}/*/*/*/.fingerprint/slint-cpp-*")

    if (stale)
        message(STATUS "Slint's headers are missing: rebuilding slint-cpp once")
        file(REMOVE_RECURSE ${stale})
    endif ()
endfunction()
