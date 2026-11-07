#!/usr/bin/env bash
# Symbol Dice: rolls, turns and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/symboldice
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_symboldice.cpp "$SRC/symboldice_save.cpp" -o "$OUT/test_symboldice"
"$OUT/test_symboldice"
