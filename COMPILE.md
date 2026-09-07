# ZDL compiling instructions

## 1. License

Copyright (c) 2023-2026 spacebub  
Copyright (c) 2018-2019 Lcferrum  
Copyright (c) 2004-2012 ZDL Software Foundation

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

This program uses the miniz and yyjson libraries. See `AUTHORS` and the
associated `LICENSE` files for details.

## 2. What you need

ZDL is C++23. The interface is Slint, built from source at configure time and
linked in statically.

- CMake 3.25 or newer, and Ninja.
- A C++23 compiler: GCC 13, Clang 16 or Visual Studio 2022, or newer.
- Rust 1.92 or newer with cargo (rustup is the easy way). On Windows use the
  `x86_64-pc-windows-msvc` toolchain.
- git and a network connection for the first configure: yyjson and Slint are
  fetched as sources, and the Slint compiler as a prebuilt for your machine.
- Linux and macOS: libcurl and fontconfig development files, and pkg-config.
  Windows uses WinHTTP and needs nothing.

Sources: <https://github.com/spacebub/qzdl>. After compiling, see `README.md`
for using ZDL.

## 3. Building

```console
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/bin/ZDL` (`ZDL.exe` on Windows) and depends on nothing
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
                            more in it than ZDL needs
-DQZDL_ACCESSIBILITY=ON     compile Slint's accessibility bridge in
                            (about 0.8 MB)
-DSANITIZE=ON               address and undefined sanitizers; lsan.supp
                            says what is ignored
```

Offline or reproducible builds: check Slint out once and pass
`-DFETCHCONTENT_SOURCE_DIR_SLINT=<checkout> -DFETCHCONTENT_FULLY_DISCONNECTED=ON`
together with `-DQZDL_SLINT_COMPILER=<path>`. Cargo caches crates in
`CARGO_HOME`. `PRODUCTION-BUILD.md` explains every setting behind the build.

## 4. Installing

```console
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix {where the app should end up}
```

The binary lands in `bin`; on Linux the desktop entry and the icons land under
`share`, on macOS it is a `ZDL.app` bundle at the top of the prefix.

## 5. Static analysis

clang-tidy reads the compile database of a configured build:

```console
tools/lint.sh build
```

The script exits non-zero if it reports anything. The check set and its
opt-outs, each with a reason, are in `.clang-tidy`. Suppressions in the sources
are NOLINT comments and each says why.
