#!/bin/sh
# Runs clang-tidy over the project sources.
#
# Usage: tools/lint.sh [build-dir]
#
# The build directory just has to contain a compile_commands.json, which cmake
# writes out on its own (CMAKE_EXPORT_COMPILE_COMMANDS is set in CMakeLists.txt).
set -eu

BUILD_DIR=${1:-build}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "no compile_commands.json in $BUILD_DIR; run cmake first" >&2
    exit 1
fi

# Third party and generated sources are not ours to fix.
FILES=$(python3 -c '
import json, sys
root, db = sys.argv[1], sys.argv[2]
for entry in json.load(open(db)):
    path = entry["file"]
    if not path.startswith(root + "/src/"):
        continue
    if "/external/" in path:
        continue
    print(path)
' "$ROOT" "$BUILD_DIR/compile_commands.json")

# The check set and the per-check opt-outs live in .clang-tidy. Warnings are not
# errors there, so clang-tidy exits 0 with a page of them: the output is what says
# whether anything was found.
echo "== clang-tidy =="

REPORT=$(echo "$FILES" | xargs -P "$(nproc)" -I{} clang-tidy -p "$BUILD_DIR" --quiet "{}" 2>&1) \
    || { printf '%s\n' "$REPORT"; echo "clang-tidy failed to run" >&2; exit 1; }

printf '%s\n' "$REPORT"

if printf '%s\n' "$REPORT" | grep -qE '\[[a-z0-9-]+,?[a-z0-9-]*\]$'; then
    echo "clang-tidy reported the above" >&2
    exit 1
fi

exit 0
