# Profile-guided optimisation

`qzdl_pgo` drives the interface through a few sittings the way a player does, so
an instrumented build can count what runs, and `tools/pgo.sh` builds ZDL4 on what
it counted. Built only with `QZDL_PGO_HARNESS=ON`. Linux and clang only: the
profile format is clang's, and the sittings launch shell scripts in place of the
source ports.

## Running

```
tools/pgo.sh --flags "-O3 -march=native -flto=full"
```

- Builds `build-pgo-train/` instrumented, runs `qzdl_pgo` in it, merges the raw
  profiles into `build-pgo/qzdl.profdata` and builds `build-pgo/bin/ZDL4` with it.
  Every fetched library is instrumented and optimised too: Blend2D, SDL, yyjson
  and tinytk.
- `--flags` are the compiler flags of the optimised build. Empty leaves the
  project's Release defaults. `--link` sets the linker flags, `-fuse-ld=lld` by
  default.
- A second round instrumented after inlining refines the first for the inlining the
  optimised build does. It measured better on every axis, so it is on by default
  and `--no-cs` skips it.
- `--bench` builds `qzdl_bench` in the optimised tree too, for comparing against
  a tree without the profile.
- `--profile-only` stops once the profile is merged. Any tree then takes it:
  `-DQZDL_PGO_USE=/abs/path/qzdl.profdata`, with the flags the profile was trained
  under. Ninja does not see a profile change, so a tree takes a new profile only
  after a clean build.
- The script fails when a sitting did not go as scripted, since the profile would
  then be of the wrong paths.

The window itself is not driven: SDL's event pump, the frame pacing and the wl_shm
upload run only with a desktop. The profile is merged sparse, so a function the
training never entered is left to the optimiser as unknown rather than taken for
cold. To count those paths as well, run the instrumented binary the training
built for a while, then merge again:

```
LLVM_PROFILE_FILE=build-pgo-train/raw/session-%p.profraw build-pgo-train/bin/ZDL4
tools/pgo.sh --reuse --flags "-O3 -march=native -flto=full"
```

`--reuse` skips the instrumented build and the training and merges whatever the
raw directory holds.

Driving the harness by hand:

```
cmake -S . -B build-pgo-check -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -DQZDL_PGO_HARNESS=ON
cmake --build build-pgo-check --target qzdl_pgo
build-pgo-check/bin/qzdl_pgo --list
build-pgo-check/bin/qzdl_pgo --sitting typical --step library-hover
```

## The options

| Option                 | Effect                                                                                       |
|------------------------|----------------------------------------------------------------------------------------------|
| `QZDL_PGO_HARNESS`     | Builds `qzdl_pgo` against the headless rig in `tests/support`                                |
| `QZDL_PGO_GENERATE`    | A directory. Instruments everything to write raw profiles into it                            |
| `QZDL_PGO_USE`         | A merged `.profdata`. Optimises everything with it                                           |
| `QZDL_PGO_CS_GENERATE` | A directory. With `QZDL_PGO_USE`, instruments again after inlining and writes into it        |

A function whose body changed since the profile was taken is reported in the
optimised build and left without its counts, not failed on: `main()` always is,
since the profile's own `main()` is the harness's. A profile is retrained when the
source moves.

## What a sitting is

Each sitting is a config shape, a window and a display rate. The first is the shape
a launcher's config settles into: a dozen profiles over as many games and eight
source ports, two of them DOS, with up to three add-ons each, in the window the app
opens with on a 144 Hz display. Then the same at 60 Hz, maximised on a 1440p display,
in the light theme, a collector's 64 profiles with 16 add-ons each, and a first run
with nothing configured.

Through each the steps go in the order a sitting does:

| Step              | What a player did                                                                    |
|-------------------|--------------------------------------------------------------------------------------|
| `startup`         | The session finds the user config, the settings apply, the shelf settles, title art arrives |
| `idle`            | Two seconds with nothing happening                                                   |
| `library-hover`   | The pointer across the cards, a pause on every third, a wander inside one            |
| `library-scroll`  | Wheel notches down the shelf and back                                                |
| `library-filter`  | Typing into the filter and erasing it                                                |
| `library-shelves` | The games shelf, a look over its cards, back to the profiles                        |
| `library-reorder` | A card carried two cells over and dropped, then carried back                         |
| `library-actions` | Duplicate, rename and delete from a card's menu                                      |
| `open-profile`    | A click on a card                                                                    |
| `profile-edit`    | Typing extra arguments, the skill, monsters and warp, every port in turn, the folds, other profiles |
| `profile-files`   | Add-ons added, toggled, reordered by their grip and removed                          |
| `dialogs`         | About, the command line, a confirmation, a prompt, a game edited, a config copied, the file picker, toasts |
| `launch`          | A port that prints and leaves, the dock reading it, play from a card, a game on its own, a port stopped, a DOS port under DOSBox |
| `engines`         | The ports shelf hovered and reordered, a port edited, the catalogue browsed          |
| `settings`        | Typing the arguments, the switches flipped, the start view, the buttons' tips        |
| `theme`           | The shade cycled round with a look over the shelf in each                            |
| `resize`          | The corner dragged in and back out                                                   |
| `persist`         | The autosave, then the write on the way out                                          |

After the sittings, `core` runs what surrounds them without a window: configs of
several shapes saved and loaded, a legacy config imported, `.zdl` files, the launch
line for every port, every add-on's maps, every game's title screen, hashes, a zip
unpacked, the catalogue and the release cache, and the output filter.

Each step checks that it did what it says, such as the drop having reordered the
shelf or the dock showing the port's lines with the escapes gone, and `qzdl_pgo`
exits non-zero when one did not. The report lists frames and pixels per step, so a
step that stopped reaching its paint path shows as a number that fell.

## The sandbox

`qzdl_pgo` runs inside `tests/support/Sandbox`: `HOME`, the XDG variables and
`PATH` point at a scratch tree, so no sitting reads the real config or finds a port
installed on the machine. `World` furnishes it: twelve games copied from the test
corpus, eight source ports that are shell scripts printing like a game (one that
waits to be stopped, one DOS program), a fake `dosbox` and a `woof` on `PATH` for
detection to find, and sixteen add-ons.
