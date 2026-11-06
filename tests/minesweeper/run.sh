#!/usr/bin/env bash
# Minesweeper's rules and flow (CrossPlay's test_minesweeper.cpp, unchanged apart
# from its includes), and the save packing.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/minesweeper
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_minesweeper.cpp -o "$OUT/test_minesweeper"
"$OUT/test_minesweeper"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_minesweeper_save.cpp "$SRC/minesweeper_save.cpp" -o "$OUT/test_minesweeper_save"
"$OUT/test_minesweeper_save"
