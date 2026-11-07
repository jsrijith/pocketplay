#!/usr/bin/env bash
# Kakuro: every bank puzzle solved by run reasoning, rules, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/kakuro
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_kakuro.cpp "$SRC/kakuro_save.cpp" -o "$OUT/test_kakuro"
"$OUT/test_kakuro"
