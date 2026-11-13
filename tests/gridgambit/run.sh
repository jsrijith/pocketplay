#!/usr/bin/env bash
# Grid Gambit: rules, generator, whole games, flow and save packing, checked on the PC without
# a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/gridgambit
"${CXX:-c++}" -std=c++17 $FLAGS -O1 -I"$SRC" test_gridgambit.cpp "$SRC/gridgambit_save.cpp" -o "$OUT/test_gridgambit"
"$OUT/test_gridgambit"
