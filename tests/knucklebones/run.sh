#!/usr/bin/env bash
# Knucklebones' rules, opponent and flow (CrossPlay's test_knucklebones.cpp,
# unchanged apart from its includes), and the save packing.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/knucklebones
"${CXX:-c++}" -std=c++17 $FLAGS -O2 -I"$SRC" test_knucklebones.cpp -o "$OUT/test_knucklebones"
"$OUT/test_knucklebones"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_knucklebones_save.cpp "$SRC/knucklebones_save.cpp" -o "$OUT/test_knucklebones_save"
"$OUT/test_knucklebones_save"
