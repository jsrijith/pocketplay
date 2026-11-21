#!/usr/bin/env bash
# Karuba Tiles: the tile table, whole rounds, fairness, rejection and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/karubatiles
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_karubatiles.cpp "$SRC/karubatiles_save.cpp" -o "$OUT/test_karubatiles"
"$OUT/test_karubatiles"
