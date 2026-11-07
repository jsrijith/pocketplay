#!/usr/bin/env bash
# Suguru: rules, every bank puzzle's one solution, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/suguru
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_suguru.cpp "$SRC/suguru_save.cpp" -o "$OUT/test_suguru"
"$OUT/test_suguru"
