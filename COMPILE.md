# ZDL compiling instructions

## 1. License

Copyright (c) 2023 spacebub  
Copyright (c) 2018-2019 Lcferrum  
Copyright (c) 2004-2012 ZDL Software Foundation

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

This program uses SimpleWFA, miniz and yyjson libraries. See AUTHORS and
associated LICENSE files for details.

## 2. General

ZDL is written in C++23 and its interface is written in Qt Quick. To be able
to compile ZDL you should install the Qt 6.9 or newer SDK, including
QtDeclarative, and CMake 3.24 or newer. ZDL also uses SimpleWFA and
miniz libraries which are statically linked and included with the sources.
miniz is the zip reader, and is what a PK3 is read through.
yyjson is fetched at configure time by CMake, so the first configure needs
git and a network connection. Sources can be downloaded from official GitHub
page:

<https://github.com/spacebub/qzdl>

After compiling, see README file for instructions on using ZDL.

## 3. General Compilation

qzdl uses CMake to generate the required project files. Built binaries will be
placed in a "bin" folder in the directory configured with CMake.

Two options are worth knowing about:

```
-DGUI=OFF             builds core on its own, without Qt
-DSANITIZE=ON         builds with the address and undefined
                      sanitizers. Where to write and what to ignore
                      travel inside the binary, so there are no
                      environment variables to remember; edit
                      lsan.supp to change what is ignored
```

### 3.1.1 Compilation on Windows

Create a build directory in the repository root, enter it then configure with CMake:

```console
mkdir build
cd build
cmake .. -G "{Your VS version}" -DCMAKE_PREFIX_PATH="{Path to qt installation}"
```

To know which version to pass to `-G` you can consult:

<https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#visual-studio-generators>

After the build is complete and you have a `qzdl.exe` file, assuming you have
the correct Qt directory registered in your PATH:

```console
windeployqt --release .\qzdl.exe
```

This will copy over the required Dlls.

### 3.1.2 Static Compilation on Windows

For a static windows build you need to link with static builds of qtbase and
qtdeclarative. The easiest way to get both is through vcpkg. The CMake flag
`MSVC_STATIC` is provided for convenient setup. The following is an example for
x64 builds:

**VCPKG:**

```console
git clone https://github.com/Microsoft/vcpkg.git  (in a suitable dir)
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install qtbase:x64-windows-static
.\vcpkg install qtdeclarative:x64-windows-static
```

**BUILD:**

```console
enter the qzdl repository root where you cloned the project
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" \
    -A x64 \
    -DMSVC_STATIC=ON \
    -DCMAKE_TOOLCHAIN_FILE="{vcpkg directory from previous step}\scripts\buildsystems\vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

For 32bit builds you can set the CMake platform to Win32 with `-A Win32`.
The 32bit triplet is `x86-windows-static`.

Against a static Qt, each QML module is a plugin that has to be linked in by
name. CMake works out which ones by running qmlimportscanner over the import
statements at configure time, so nothing has to be listed by hand; a QML file
that is missing from `src/gui/CMakeLists.txt` is invisible to it, which is one
more reason the build fails loudly rather than at run time.

Everything else ZDL links is static already and stays that way: miniz is
compiled from the vendored sources, and yyjson is built by CMake at configure
time. The root CMakeLists pins `BUILD_SHARED_LIBS` off so that a build
configured with it on cannot quietly turn yyjson into a shared object.

### 3.2.1 Compile for Linux

CMake by default will generate the necessary makefile:

```console
mkdir build
cd build
cmake ..
make -j$(nproc --all)
```

### 3.2.2 Static Compilation on Linux

You will have to manually build static versions of qtbase and qtdeclarative
and then link to them during compilation. The notes on QML plugins and on the
other dependencies in 3.1.2 apply here too.

## 4. Static analysis

The sources are kept clean under clang-tidy and clazy. Both read the compile
database that CMake writes into the build directory, so configure a build
first, then:

```console
tools/lint.sh build
```

The script exits non-zero if either tool reports anything.

clang-tidy's check set and its per-check opt-outs live in `.clang-tidy` at the
top of the tree, each opt-out carrying the reason it does not fit this
codebase. clazy has no project config file, so its check set is in the script
instead: levels 0 through 2, minus qstring-allocations.

Two things worth knowing before adding checks:

- `readability-redundant-access-specifiers` is not moc-aware. Its automatic
  fix deletes Qt "slots:" and "signals:" sections, which breaks the build.
- clazy stops emitting after a number of diagnostics per file, so leaving a
  noisy check on can hide real findings behind it.

Suppressions in the sources are NOLINT comments and each says why.
