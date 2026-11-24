#!/usr/bin/env bash
# Yacht Dice's rules, opponent and flow (CrossPlay's test_yahtzee.cpp, unchanged
# apart from its includes), and the save packing.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/yachtdice
"${CXX:-c++}" -std=c++17 $FLAGS -O2 -I"$SRC" test_yachtdice.cpp -o "$OUT/test_yachtdice"
"$OUT/test_yachtdice"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_yachtdice_save.cpp "$SRC/yachtdice_save.cpp" -o "$OUT/test_yachtdice_save"
"$OUT/test_yachtdice_save"
