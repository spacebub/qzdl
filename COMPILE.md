# ZDL4 compiling instructions

## 1. Requirements

ZDL4 is written in C++23. The interface is built on tinytk, which draws with Blend2D into
a window SDL owns. All three are fetched at configure time and linked in
statically.

- CMake 4.2 or newer, and Ninja.
- Linux: Clang 22 or newer, lld, libstdc++ from GCC 15 or newer, pkg-config,
  and the development files of libcurl, X11 with its Xext, Xcursor, Xi,
  Xfixes, Xrandr, Xrender and Xss extensions, xkbcommon, Wayland and EGL.
- Windows: Visual Studio 2022.

Ubuntu:

```console
apt install cmake ninja-build pkg-config g++ libcurl4-openssl-dev libx11-dev libxext-dev libxcursor-dev libxi-dev libxfixes-dev libxrandr-dev libxrender-dev libxss-dev libxkbcommon-dev libwayland-dev libwayland-bin libegl1-mesa-dev
```

Ubuntu's clang is older than 22, and before 26.04 so is its g++. apt.llvm.org
has `clang-23` and `lld-23`, and ppa:ubuntu-toolchain-r/test has `g++-15`.

Fedora:

```console
dnf install clang lld cmake ninja-build pkgconf-pkg-config libstdc++-devel libcurl-devel libX11-devel libXext-devel libXcursor-devel libXi-devel libXfixes-devel libXrandr-devel libXrender-devel libXScrnSaver-devel libxkbcommon-devel wayland-devel mesa-libEGL-devel
```

Arch:

```console
pacman -S clang lld cmake ninja pkgconf gcc curl libx11 libxext libxcursor libxi libxfixes libxrandr libxrender libxss libxkbcommon wayland libglvnd
```

## 2. Building

Linux:

```console
CC=clang CXX=clang++ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
cmake --build build
```

Windows, from the x64 developer prompt:

```console
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is `build/bin/ZDL4` (`ZDL4.exe` on Windows) and depends on nothing
that is not part of the system. Release is the configuration to ship: it is
what the size and link settings are written for, and it links with
interprocedural optimisation. `Debug` builds Blend2D unoptimised as well.

Options:

```
-DQZDL_TESTS=ON             the test suite, see tests/README.md
-DQZDL_BENCHMARKS=ON        the benchmark suite, see bench/README.md
-DSANITIZE=ON               address and undefined sanitizers, with lsan.supp
                            saying what is ignored
-DTTK_DOWNLOAD_CACHE=...    where the fetched sources are kept, shared by
                            every build tree (default: .download-cache in
                            the repository). Empty fetches into the build
                            tree instead
-DQZDL_RELEASE=ON           no git revision beside the version
```

SDL, Blend2D, asmjit and yyjson land in `.download-cache` the first time they
are needed, and every later build tree is pointed at what is already there.
tinytk supplies that cache, so it is the one download each new tree makes.
Delete the directory to start over.

Blend2D bundles asmjit rather than linking one, so asmjit is populated and not
built. It is compiled into Blend2D, configured the way Blend2D wants it.

To build against a local tinytk checkout instead of the pinned tag, pass
`-DFETCHCONTENT_SOURCE_DIR_TINYTK={path to it}`.

## 3. Profile-guided optimisation

A build tuned to the machine it is built on:

```console
cmake --workflow --preset pgo
```

The preset configures `build-pgo/` as Release with `-O3 -march=native` and full
LTO, trains a profile at configure time by building an instrumented tree under it
and driving the interface through `qzdl_pgo`, then builds `build-pgo/bin/ZDL4` on
the profile. A few minutes the first time, a plain build after that. Linux and
clang only. `tools/pgo.sh` does the same for any flags and tree, and
`pgo/README.md` has the options and what the training covers.

Another tree: the configure preset takes `-B`, the workflow does not.

```console
cmake --preset pgo -B build-fast
cmake --build build-fast --target qzdl_gui
```

A `CMakeUserPresets.json` preset that inherits `pgo` with its own `binaryDir`
keeps the one-line form.

## 4. Installing

```console
cmake --install build --prefix {where the app should end up}
```

The binary lands in `bin`, the desktop entry and the icons under `share`.

The Windows installer:

```console
iscc /DAppVersion={version} packaging\qzdl.iss
```

## 5. Static analysis

clang-tidy reads the compile database of a configured build:

```console
tools/lint.sh build
```

The script exits non-zero if it reports anything. The check set and its
opt-outs, each with a reason, are in `.clang-tidy`. Suppressions in the sources
are NOLINT comments and each says why.
