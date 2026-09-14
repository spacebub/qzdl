# Benchmarks

Google Benchmark suite over `src/core` and `src/gui`. Built only with
`QZDL_BENCHMARKS=ON`.

## Building

```
cmake -S . -B build-bench -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld -flto=full" \
    -DQZDL_BENCHMARKS=ON
cmake --build build-bench --target qzdl_bench
./build-bench/bin/qzdl_bench
```

- `Release` only; a `Debug` build measures `-Og`.
- A matching C and C++ compiler plus `lld`: `Release` turns interprocedural
  optimisation on, and a mixed pair leaves bitcode neither linker can read.
- `qzdl_gui`'s sources are read off the target and compiled a second time, so a
  file added to the interface is benchmarked without an edit here.

## Running

```
./build-bench/bin/qzdl_bench --benchmark_filter='Card_|Frame_'
./build-bench/bin/qzdl_bench --benchmark_min_time=2s
./build-bench/bin/qzdl_bench --benchmark_out=before.json --benchmark_out_format=json
```

Comparing two commits, with the tool Google Benchmark ships (needs scipy):

```
python3 .download-cache/benchmark-v1.9.5/tools/compare.py benchmarks before.json after.json
```

- Compare whole runs. Every file drives the same `Rig`, `State` and process-wide
  caches, so a filtered run is a different measurement from a full one.
- Pin the clock before trusting a small difference; the runner warns when CPU
  scaling is on.
- `bench/isolation.sh <scratch dir>` reports how far a benchmark drifts between
  running alone and in the suite. A new benchmark that drifts more than the rest
  is holding state the one before it left.

## Layout

| Directory    | Covers                                                                                                            |
|--------------|-------------------------------------------------------------------------------------------------------------------|
| `support/`   | The harness: a windowless canvas, a headless `App`, and the fixtures both read                                    |
| `core/`      | `Text`, `Ini`, `Json`, `Config`, `Import`, the launch layer, the WAD and PK3 readers                              |
| `gui/`       | `Typeface`, `Painter`, the glyph and paint primitives, the controls, the layouts, `LibraryCard`, the state tree, the bridges |
| `workflows/` | Startup, the library, the profile and engines pages, and the per-frame budget                                     |

### The harness

- `support/Sandbox` points `HOME` and the XDG variables at a scratch tree before
  anything reads them; no benchmark touches a real config.
- `support/Canvas` is a window's pixels with no window: the same XRGB32 target
  and damage loop as `Shell::draw`, minus SDL. `frame()` settles, advances and
  paints the damaged rectangles; `full()` paints the lot.
- `support/Rig` is `App` without the window: the same services, widget tree and
  `sync()`, against a `Canvas`. Its `Shell` is constructed but never started.
  `bench::shared()` is the one rig for the process, since `State` and `Session`
  are singletons.
- `Rig::forget()` resets the shared rig; call it before installing a fixture.
  `Rig::ready()` waits for what the fixture built, title screens included; call
  it after `sync()`. `Canvas::bare()` does the same for the control benchmarks,
  and `mount()` calls it already.
- `support/Corpus` writes real game files into the sandbox: an IWAD with a
  320x200 `TITLEPIC`, a stored-method PK3, a legacy `.zdl`. `support/Fixtures`
  builds configs of a given shape, deterministically.

## Reading the numbers

- The budget for `gui/` and `workflows/` is one frame, 1000 ms over the refresh
  rate. `Frame_idle` and `Frame_fullRepaint` bound the range; a control's
  `_paint` is its share.
- Sizes are swept where the cost depends on one: 8/64/512 profiles and 0/16/512
  add-ons stand for a small, a large and an unreasonable config. A time that is
  flat across a sweep means the work is cached or culled.
