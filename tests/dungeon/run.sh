#!/usr/bin/env bash
# D&Diagrams' rules, puzzle bank, guide and save packing, checked on the PC.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/dungeon
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_dungeon.cpp "$SRC/dungeon_rules.cpp" "$SRC/dungeon_guide.cpp" -o "$OUT/test_dungeon"
"$OUT/test_dungeon"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_dungeon_save.cpp "$SRC/dungeon_rules.cpp" "$SRC/dungeon_save.cpp" -o "$OUT/test_dungeon_save"
"$OUT/test_dungeon_save"
