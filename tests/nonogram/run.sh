#!/usr/bin/env bash
# Nonograms: every bank puzzle line-solvable, rules, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/nonogram
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_nonogram.cpp "$SRC/nonogram_save.cpp" -o "$OUT/test_nonogram"
"$OUT/test_nonogram"
