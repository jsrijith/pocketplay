#!/usr/bin/env bash
# Queens: rules, every bank puzzle's one solution, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/queens
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_queens.cpp "$SRC/queens_save.cpp" -o "$OUT/test_queens"
"$OUT/test_queens"
