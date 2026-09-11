# ZDL4 compiling instructions

## 1. What you need

ZDL4 is C++23. The interface is drawn with Blend2D into a window SDL owns;
both are fetched at configure time and linked in statically.

- CMake 3.25 or newer, and Ninja.
- A C++23 compiler: GCC 14, Clang 16 or Visual Studio 2022, or newer.
  Clang reads GCC's libstdc++ on Linux, so it needs GCC 14's as well:
  `std::ranges::to` is not in 13's.
- Linux : libcurl development files, and the X11 and Wayland ones. SDL builds
  without whichever of those two is missing at configure time and says nothing
  about it later.

## 2. Building

```console
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/bin/ZDL4` (`ZDL4.exe` on Windows) and depends on nothing
that is not part of the system. Release is the configuration to ship: it is
what the size and link settings are written for. A `Debug` configuration
builds Blend2D unoptimised as well, and a rasteriser built that way is slow to
run, so for day-to-day work keep a Release tree beside it.

Options:

```
-DSANITIZE=ON               address and undefined sanitizers; lsan.supp
                            says what is ignored
-DQZDL_DOWNLOAD_CACHE=...   where the fetched sources are kept, shared by
                            every build tree (default: .download-cache in
                            the repository); empty fetches into the build
                            tree instead
```

SDL, Blend2D, asmjit and yyjson land in `.download-cache` the first time they
are needed, and every later build tree is pointed at what is already there, so a
new one costs no download. Delete the directory to start over. Blend2D and
asmjit publish no tags, so both are pinned by commit and fetched rather than
cloned; the cache handles either.

Blend2D bundles asmjit rather than linking one, so asmjit is populated and not
built: it is compiled into Blend2D, configured the way Blend2D wants it.

Offline or reproducible builds: fill the cache once, or check the sources out
yourself and pass `-DFETCHCONTENT_SOURCE_DIR_SDL3=<checkout>` and the same for
`BLEND2D` and `ASMJIT`; `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` then holds every
fetch to what is on disk. `PRODUCTION-BUILD.md` explains every setting behind
the build.

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
