#!/usr/bin/env bash
# Sudoku's rules and game (CrossPlay's test_sudoku.cpp, unchanged apart from its
# includes), and the save packing.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/sudoku
# -O2 as CrossPlay builds it: the generator's soak is slow unoptimised under the sanitizers.
"${CXX:-c++}" -std=c++17 $FLAGS -O2 -I"$SRC" test_sudoku.cpp "$SRC/sudoku_rules.cpp" -o "$OUT/test_sudoku"
"$OUT/test_sudoku"
"${CXX:-c++}" -std=c++17 $FLAGS -O2 -I"$SRC" test_sudoku_save.cpp "$SRC/sudoku_rules.cpp" "$SRC/sudoku_save.cpp" -o "$OUT/test_sudoku_save"
"$OUT/test_sudoku_save"
