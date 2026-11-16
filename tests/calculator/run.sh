#!/usr/bin/env bash
# Calculator: typing, pocket-style sums, percent, errors and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/apps/calculator
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_calculator.cpp "$SRC/calculator_save.cpp" -o "$OUT/test_calculator"
"$OUT/test_calculator"
