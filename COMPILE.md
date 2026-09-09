# ZDL4 compiling instructions

## 1. What you need

ZDL4 is C++23. The interface is Slint, built from source at configure time and
linked in statically.

- CMake 3.25 or newer, and Ninja.
- A C++23 compiler: GCC 14, Clang 16 or Visual Studio 2022, or newer.
  Clang reads GCC's libstdc++ on Linux, so it needs GCC 14's as well:
  `std::ranges::to` is not in 13's.
- Rust 1.92 or newer with cargo (rustup is the easy way). On Windows use the
  `x86_64-pc-windows-msvc` toolchain.
- Linux : libcurl and fontconfig development files, and pkg-config.

## 2. Building

```console
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/bin/ZDL4` (`ZDL4.exe` on Windows) and depends on nothing
that is not part of the system. Release is the configuration to ship: it is
what the size and link settings are written for. A `Debug` configuration
builds Slint unoptimised as well, which is slow to build and to run; for
day-to-day work either keep a Release tree beside it or use an installed Slint
SDK with `-DQZDL_SLINT_PACKAGE=ON` and `-DCMAKE_PREFIX_PATH` pointing at it.

Options:

```
-DQZDL_SLINT_COMPILER=...   download (default): the prebuilt slint-compiler
                            off the GitHub release; source: build it from
                            the fetched sources; or a path to one you have
-DQZDL_SLINT_PACKAGE=ON     use an installed Slint SDK. Development only:
                            it links Slint as a shared library with far
                            more in it than ZDL4 needs
-DQZDL_ACCESSIBILITY=ON     compile Slint's accessibility bridge in
                            (about 0.8 MB)
-DSANITIZE=ON               address and undefined sanitizers; lsan.supp
                            says what is ignored
-DQZDL_DOWNLOAD_CACHE=...   where the fetched sources are kept, shared by
                            every build tree (default: .download-cache in
                            the repository); empty fetches into the build
                            tree instead
-DQZDL_CARGO_CACHE=...      where the Rust build is kept, shared by every
                            build tree (default: cargo inside the download
                            cache); empty builds in the build tree instead
```

Slint, yyjson, Corrosion and the prebuilt Slint compiler land in
`.download-cache` the first time they are needed, and every later build tree
is pointed at what is already there, so a new one costs no download. Delete
the directory to start over.

The Rust build is shared the same way, in `.download-cache/cargo`, so a new
build tree reuses the crates Slint was already built from rather than
compiling them again. Slint's generated headers are written by cargo's build
script and are kept there beside it, since a reused build does not rerun it.

Deleting a build tree and configuring it again at the same path costs no Rust
build at all. A tree at a different path rebuilds the one `slint-cpp` crate,
because the directory it writes those headers to is part of what cargo keys
on, so one tree kept around still beats alternating between two.

Offline or reproducible builds: fill the cache once, or check Slint out
yourself and pass `-DFETCHCONTENT_SOURCE_DIR_SLINT=<checkout>` and
`-DQZDL_SLINT_COMPILER=<path>`; `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` then
holds every fetch to what is on disk. Cargo caches crates in `CARGO_HOME`.
`PRODUCTION-BUILD.md` explains every setting behind the build.

## 3. Installing

```console
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix {where the app should end up}
```

The binary lands in `bin`; on Linux the desktop entry and the icons land under
`share`, on macOS it is a `ZDL4.app` bundle at the top of the prefix.

## 4. Static analysis

clang-tidy reads the compile database of a configured build:

```console
tools/lint.sh build
```

The script exits non-zero if it reports anything. The check set and its
opt-outs, each with a reason, are in `.clang-tidy`. Suppressions in the sources
are NOLINT comments and each says why.
