#!/usr/bin/env bash
# Dice Roller: fair faces, rejection, totals and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/diceroller
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_diceroller.cpp "$SRC/diceroller_save.cpp" -o "$OUT/test_diceroller"
"$OUT/test_diceroller"
