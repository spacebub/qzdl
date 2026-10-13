# Tests

GoogleTest suite over `src/core` and `src/gui`, in the shape of tinytk's. Built
only with `QZDL_TESTS=ON`.

## Building

```
cmake -S . -B build-tests -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld \
    -DQZDL_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests -j $(nproc)
```

- `Debug` is the configuration to test: `Release` links every program with
  interprocedural optimisation, which is minutes per binary for nothing.
- A matching C and C++ compiler, as for any tree. `SANITIZE=ON` works here as
  it does for the app.
- `qzdl_gui`'s sources are read off the target and compiled once into
  `qzdl_test_gui`, so a file added to the interface is covered without an edit
  here.

## Running

```
ctest --test-dir build-tests -R 'core_config\.'
./build-tests/bin/qzdl_test_core_config --gtest_filter='Session.*'
```

- One program per area, declared with `qzdl_test(name files...)` in the area's
  `CMakeLists.txt`. Every `TEST` in it is a ctest entry of its own, so `ctest -j`
  runs them side by side, each in its own process and its own sandbox.
- A case that paints text skips itself when no font is installed.

## Layout

| Directory         | Covers                                                                          |
|-------------------|---------------------------------------------------------------------------------|
| `support/`        | The harness: the sandbox, a windowless canvas, a clock to drive by hand, a headless `App`, and the fixtures |
| `core/config/`    | `Config`, `Profile`, `Schema`, `Session`, `ConfigPaths`, `Import`                |
| `core/util/`      | `Ansi`, `Ini`, `Md5`                                                            |
| `core/launch/`    | `Arguments`, `Command`, `Dialect`, `Dos`, `DosFiles`, `Launcher`, `Netgame`, `Storage` |
| `core/ports/`     | `Archive`, `Catalog`, `Detect`, `Install`, `Releases`                            |
| `core/wad/`       | `Wad`, `LibPk3`, `LibDir`, `MapFile`, `FileInfo`, `Artwork`                      |
| `gui/state/`      | The state tree                                                                  |
| `gui/model/`      | The bridges: config in, state out                                               |
| `gui/draw/`       | `Mark`, `Warp`, the card art                                                    |
| `gui/services/`   | `Engines`, `Filters`, `IwadArt`, `RunLog`, `Runs`                               |
| `gui/components/` | What is on the window and is not a page                                          |
| `gui/pages/`      | The four pages, synced from the state tree                                      |
| `gui/dialogs/`    | The modal dialogs                                                               |

### The harness

- `support/Sandbox` points `HOME`, the XDG variables and `PATH` at a scratch
  tree before anything reads them, so no case touches a real config or finds a
  port installed on the machine. `scratch(name)` is an empty directory for a
  case that writes.
- `support/Canvas` is a window's pixels with no window: an offscreen `Surface`
  through the same `compose` step `Shell::draw` uses. `frame()` settles and
  paints what is damaged, `full()` paints the lot, `frameAt(now)` and `run(n)`
  move the clock for tweens. `at(x, y)` and `pixels()` read it back.
  `mount()` puts one widget alone in the page, `paintOnce()` paints one with
  nothing above it. `Painted` is the fixture for a case that needs a font.
- `support/Shell` is the frame loop's `Clock` and the window's controls with no
  window: `advance(seconds)` fires the timers that fall due and runs what was
  posted from another thread, `drain()` runs only the posted. Every service
  that debounces or schedules is driven through it.
- `support/Rig` is `App` without the window: the same services, widget tree,
  wiring and `sync()`, against a `Canvas` and a `Shell`. Building one empties
  the `Session` config and the `State` tree, both singletons. What a view asks
  the window for, a confirmation, a prompt, an entry dialog, is recorded with
  its callback rather than shown, so a case answers it. `turn()` is one turn of
  the loop, `ready()` waits for the threads and runs what fell due. `Rigged` is
  the fixture.
- `support/Corpus` writes real game files into the sandbox: an IWAD with a
  320x200 `TITLEPIC`, a stored-method PK3, small PWADs, a legacy `.zdl` with
  12 games, 12 ports and 64 files. The writers are its own: a case that needs a
  malformed file writes the bytes itself.
  `support/Fixtures` builds configs of a given shape, deterministically, and
  `install()` puts one in the `Session`.
