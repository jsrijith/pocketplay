#!/usr/bin/env bash
# Peg Solitaire: rules, whole games, flow and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/pegsolitaire
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_pegsolitaire.cpp "$SRC/pegsolitaire_save.cpp" -o "$OUT/test_pegsolitaire"
"$OUT/test_pegsolitaire"
