#!/usr/bin/env bash
# Word Ladder: every stored ladder, days, play, hints, rows and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/wordladder
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_wordladder.cpp "$SRC/wordladder_save.cpp" -o "$OUT/test_wordladder"
"$OUT/test_wordladder"
