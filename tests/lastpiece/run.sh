#!/usr/bin/env bash
# Last Piece: rules, solver, card files, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/lastpiece
"${CXX:-c++}" -std=c++17 $FLAGS -O1 -I"$SRC" test_lastpiece.cpp "$SRC/lastpiece_pack.cpp" "$SRC/lastpiece_save.cpp" -o "$OUT/test_lastpiece"
"$OUT/test_lastpiece"
