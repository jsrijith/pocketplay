#!/usr/bin/env bash
# Connect Four's rules, opponent and flow (CrossPlay's test_connectfour.cpp,
# unchanged apart from its includes), and the save packing.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/connectfour
"${CXX:-c++}" -std=c++17 $FLAGS -O2 -I"$SRC" test_connectfour.cpp -o "$OUT/test_connectfour"
"$OUT/test_connectfour"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_connectfour_save.cpp "$SRC/connectfour_save.cpp" -o "$OUT/test_connectfour_save"
"$OUT/test_connectfour_save"
