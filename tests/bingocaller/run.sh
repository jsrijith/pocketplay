#!/usr/bin/env bash
# Bingo Caller: whole games, fairness, rejection, letters and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/bingocaller
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_bingocaller.cpp "$SRC/bingocaller_save.cpp" -o "$OUT/test_bingocaller"
"$OUT/test_bingocaller"
