#!/usr/bin/env bash
# Lights Out: rules, solver and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/lightsout
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_lightsout.cpp "$SRC/lightsout_save.cpp" -o "$OUT/test_lightsout"
"$OUT/test_lightsout"
