# Cargo's target directory, shared across build trees. Empty builds in the tree.
if (QZDL_DOWNLOAD_CACHE)
    set(_qzdl_cargo_default "${QZDL_DOWNLOAD_CACHE}/cargo")
else ()
    set(_qzdl_cargo_default "")
endif ()

set(QZDL_CARGO_CACHE "${_qzdl_cargo_default}"
        CACHE PATH "Where the Rust build is kept, shared by every build tree")

unset(_qzdl_cargo_default)

# Corrosion has no option for it, so the build tree's directory is a link to the shared one.
function(qzdl_share_cargo_dir)
    set(link "${CMAKE_BINARY_DIR}/cargo")

    if (NOT QZDL_CARGO_CACHE OR EXISTS "${link}" OR IS_SYMLINK "${link}")
        return()
    endif ()

    file(MAKE_DIRECTORY "${QZDL_CARGO_CACHE}")
    file(CREATE_LINK "${QZDL_CARGO_CACHE}" "${link}" SYMBOLIC RESULT result)

    # Symlinks fail on Windows without developer mode; the build then stays in the tree.
    if (result EQUAL 0)
        message(STATUS "Sharing the Rust build in ${QZDL_CARGO_CACHE}")
    else ()
        message(STATUS "Rust builds in the build tree: ${result}")
    endif ()
endfunction()

# cbindgen writes Slint's headers into the build tree and a cached build does not
# rerun it, so they are shared too. The key separates builds with different headers.
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

    # An older cache has the artifacts but no headers; dropping the fingerprint rebuilds that one crate.
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
