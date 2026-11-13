#!/usr/bin/env bash
# Chess Puzzles: rules, every puzzle solved by taps, flow and save packing, checked on the PC
# without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/chesspuzzles
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_chesspuzzles.cpp "$SRC/chesspuzzles_save.cpp" -o "$OUT/test_chesspuzzles"
"$OUT/test_chesspuzzles"
