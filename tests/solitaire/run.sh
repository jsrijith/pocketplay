#!/usr/bin/env bash
# Solitaire's rules (CrossPlay's test_solitaire.cpp, unchanged apart from its
# include), and the save packing, undo history across a save, and record.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/solitaire
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_solitaire.cpp "$SRC/solitaire_rules.cpp" -o "$OUT/test_solitaire"
"$OUT/test_solitaire"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_solitaire_save.cpp "$SRC/solitaire_rules.cpp" "$SRC/solitaire_save.cpp" \
  "$SRC/solitaire_record.cpp" -o "$OUT/test_solitaire_save"
"$OUT/test_solitaire_save"
