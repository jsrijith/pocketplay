#!/usr/bin/env bash
# Sokoban: every level, rules, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/sokoban
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_sokoban.cpp "$SRC/sokoban_save.cpp" -o "$OUT/test_sokoban"
"$OUT/test_sokoban"
