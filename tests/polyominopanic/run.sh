#!/usr/bin/env bash
# Polyomino Panic: the shapes, both decks and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/polyominopanic
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_polyominopanic.cpp "$SRC/polyominopanic_save.cpp" -o "$OUT/test_polyominopanic"
"$OUT/test_polyominopanic"
