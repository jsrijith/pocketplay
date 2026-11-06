#!/usr/bin/env bash
# One Clue's deck, word list and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/oneclue
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_oneclue.cpp "$SRC/oneclue_save.cpp" -o "$OUT/test_oneclue"
"$OUT/test_oneclue"
