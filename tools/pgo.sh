#!/usr/bin/bash
# Trains a profile on qzdl_pgo and builds ZDL4 with it.
#
# Usage: tools/pgo.sh [--flags "<cflags>"] [--link "<ldflags>"] [--out <file>]
#                     [--tree <dir>] [--train <dir>] [--repeat N] [--no-cs] [--bench]
#                     [--profile-only] [--reuse]
#
#   --flags    Compiler flags of the optimised build, such as "-O3 -march=native -flto=full".
#              Empty leaves the project's Release defaults.
#   --link     Linker flags. Defaults to -fuse-ld=lld.
#   --out      Where the merged profile lands. Defaults to <tree>/qzdl.profdata.
#   --tree     The optimised tree. Defaults to build-pgo.
#   --train    The instrumented tree. Defaults to build-pgo-train.
#   --repeat   Runs of the harness in the instrumented build. Defaults to 1.
#   --no-cs    Skip the second round, the one instrumented after inlining. It refines
#              the first and measured better on every axis, so it is on by default.
#   --bench    Build qzdl_bench in the optimised tree as well.
#   --profile-only
#              Stop once the profile is merged.
#   --reuse    Skip the instrumented build and the training: merge what the raw
#              directory already holds, such as the harness's profiles plus a real
#              session of the instrumented ZDL4, and build on that.
#
# CC and CXX pick the compiler, which has to be clang. LLVM_PROFDATA names the
# llvm-profdata of that clang when it is not the one on PATH.
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

FLAGS=""
LINK="-fuse-ld=lld"
OUT=""
TREE="$ROOT/build-pgo"
TRAIN="$ROOT/build-pgo-train"
REPEAT=1
CS=1
BENCH=0
PROFILE_ONLY=0
REUSE=0

while [ $# -gt 0 ]; do
    case "$1" in
        --flags) FLAGS=$2; shift 2 ;;
        --link) LINK=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        --tree) TREE=$2; shift 2 ;;
        --train) TRAIN=$2; shift 2 ;;
        --repeat) REPEAT=$2; shift 2 ;;
        --cs) CS=1; shift ;;
        --no-cs) CS=0; shift ;;
        --bench) BENCH=1; shift ;;
        --profile-only) PROFILE_ONLY=1; shift ;;
        --reuse) REUSE=1; shift ;;
        -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done

CC=${CC:-clang}
CXX=${CXX:-clang++}

case "$($CXX --version 2>/dev/null | head -1)" in
    *clang*) ;;
    *) echo "$CXX is not clang; set CC and CXX" >&2; exit 1 ;;
esac

PROFDATA=${LLVM_PROFDATA:-$($CXX -print-prog-name=llvm-profdata)}

if ! command -v "$PROFDATA" >/dev/null 2>&1; then
    echo "no llvm-profdata beside $CXX. Set LLVM_PROFDATA" >&2
    exit 1
fi

case "$TREE" in /*) ;; *) TREE="$ROOT/$TREE" ;; esac
case "$TRAIN" in /*) ;; *) TRAIN="$ROOT/$TRAIN" ;; esac

[ -n "$OUT" ] || OUT="$TREE/qzdl.profdata"
case "$OUT" in /*) ;; *) OUT="$ROOT/$OUT" ;; esac

RAW="$TRAIN/raw"
JOBS=$(nproc 2>/dev/null || echo 4)

say() {
    printf '\n== %s ==\n' "$1"
}

# Configures a tree with the common settings plus whatever follows.
configure() {
    dir=$1
    shift

    cmake -S "$ROOT" -B "$dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_EXE_LINKER_FLAGS="$LINK" \
        "$@"
}

# Runs the harness in `dir`, writing raw profiles into `raw`.
train() {
    dir=$1
    raw=$2

    rm -rf "$raw"
    mkdir -p "$raw"

    # A sitting that did not go as scripted would profile the wrong paths.
    LLVM_PROFILE_FILE="$raw/qzdl-%p.profraw" "$dir/bin/qzdl_pgo" --repeat "$REPEAT"
}

if [ "$REUSE" -eq 0 ]; then
    # Instrumented under the same flags, ZDL4 included for a session by hand.
    say "instrumented build in $TRAIN"
    configure "$TRAIN" \
        -DCMAKE_C_FLAGS="$FLAGS" \
        -DCMAKE_CXX_FLAGS="$FLAGS" \
        -DQZDL_PGO_HARNESS=ON \
        -DQZDL_PGO_GENERATE="$RAW"
    cmake --build "$TRAIN" --target qzdl_pgo qzdl_gui -j "$JOBS"

    say "training"
    train "$TRAIN" "$RAW"
elif ! ls "$RAW"/*.profraw >/dev/null 2>&1; then
    echo "nothing to reuse in $RAW" >&2
    exit 1
fi

# Sparse: a function with every counter at zero is left out rather than taken for
# cold. The window shell, SDL's event pump and the wl_shm upload never run headless.
say "merging into $OUT"
mkdir -p "$(dirname -- "$OUT")"
"$PROFDATA" merge --sparse -o "$OUT" "$RAW"/*.profraw
"$PROFDATA" show --topn=12 "$OUT"

cat <<HINT

The window itself is not driven headless. To count a real session as well, run the
instrumented binary for a while and merge again:
    LLVM_PROFILE_FILE="$RAW/session-%p.profraw" "$TRAIN/bin/ZDL4"
    tools/pgo.sh --reuse $([ -n "$FLAGS" ] && printf -- '--flags "%s" ' "$FLAGS")--out "$OUT"
HINT

if [ "$CS" -eq 1 ]; then
    # The second round counts what the first left inlined, so it builds like the
    # optimised tree does.
    say "context-sensitive build in $TRAIN-cs"
    configure "$TRAIN-cs" \
        -DCMAKE_C_FLAGS="$FLAGS" \
        -DCMAKE_CXX_FLAGS="$FLAGS" \
        -DQZDL_PGO_HARNESS=ON \
        -DQZDL_PGO_USE="$OUT" \
        -DQZDL_PGO_CS_GENERATE="$TRAIN-cs/raw"
    cmake --build "$TRAIN-cs" --target qzdl_pgo -j "$JOBS"

    say "training again"
    train "$TRAIN-cs" "$TRAIN-cs/raw"

    say "merging both rounds into $OUT"
    "$PROFDATA" merge --sparse -o "$OUT.cs" "$OUT" "$TRAIN-cs"/raw/*.profraw
    mv "$OUT.cs" "$OUT"
    "$PROFDATA" show "$OUT" | head -8
fi

if [ "$PROFILE_ONLY" -eq 1 ]; then
    echo "profile: $OUT"
    exit 0
fi

say "optimised build in $TREE"
configure "$TREE" \
    -DCMAKE_C_FLAGS="$FLAGS" \
    -DCMAKE_CXX_FLAGS="$FLAGS" \
    -DQZDL_PGO_USE="$OUT" \
    -DQZDL_BENCHMARKS="$([ "$BENCH" -eq 1 ] && echo ON || echo OFF)"

# Ninja does not see the profile change, and every object depends on it.
cmake --build "$TREE" --target clean

LOG="$TREE/pgo-build.log"
TARGETS=(qzdl_gui)

[ "$BENCH" -eq 0 ] || TARGETS+=(qzdl_bench)

if ! cmake --build "$TREE" --target "${TARGETS[@]}" -j "$JOBS" 2>&1 | tee "$LOG"; then
    echo "the optimised build failed, see $LOG" >&2
    exit 1
fi

# Functions the profile has nothing for, and functions changed since the training.
# A few of each is normal: main() is never driven, and the profile's own main() is
# the harness's.
UNPROFILED=$(grep -c 'no profile data available' "$LOG" || true)
STALE=$(grep -c 'hash mismatch' "$LOG" || true)

say "done"
echo "profile:    $OUT"
echo "binary:     $TREE/bin/ZDL4 ($(wc -c < "$TREE/bin/ZDL4") bytes)"
echo "warnings:   $UNPROFILED unprofiled, $STALE changed since the profile"
