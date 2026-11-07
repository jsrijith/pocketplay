#!/usr/bin/env bash
# Text adventures: the Z-machine playing the built-in Zorks, and the game screen's pieces, checked
# on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/textadv
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_zmachine.cpp "$SRC/zmachine.cpp" -o "$OUT/test_zmachine"
"$OUT/test_zmachine"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_textadv_text.cpp "$SRC/zmachine.cpp" -o "$OUT/test_textadv_text"
"$OUT/test_textadv_text"
