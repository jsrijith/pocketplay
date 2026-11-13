#!/usr/bin/env bash
# Island Maps: every island's grid, stepping, the random pick and the save, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/islandmaps
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_islandmaps.cpp "$SRC/islandmaps_save.cpp" -o "$OUT/test_islandmaps"
"$OUT/test_islandmaps"
