#!/usr/bin/env bash
# Authority Tracker: scores, the end at 0, taking back a tap, and save packing, checked on
# the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/authoritytracker
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_authoritytracker.cpp "$SRC/authoritytracker_save.cpp" -o "$OUT/test_authoritytracker"
"$OUT/test_authoritytracker"
