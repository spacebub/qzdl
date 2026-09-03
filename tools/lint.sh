#!/bin/sh
# Runs the static analysers over the project sources.
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

# Third party sources and everything generated under the build directory --
# moc output and the C++ the QML modules are compiled into -- are not ours to
# fix.
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

status=0

# The check set and the per-check opt-outs live in .clang-tidy.
echo "== clang-tidy =="
echo "$FILES" | xargs -P "$(nproc)" -I{} clang-tidy -p "$BUILD_DIR" --quiet "{}" || status=1

# clazy has no project config file, so its check set lives here instead.
#
# qstring-allocations is deliberately left off.  It fires on nearly every
# string literal in the tree, and what it flags are one-shot dialog titles,
# INI keys and command line fragments, where an allocation is noise next to
# the file and process work around it.
echo "== clazy =="
echo "$FILES" | xargs -P "$(nproc)" -I{} clazy-standalone \
    -p "$BUILD_DIR" \
    --ignore-dirs='.*/external/.*|.*_autogen/.*' \
    --checks='level0,level1,level2,no-qstring-allocations' \
    "{}" || status=1

exit $status
