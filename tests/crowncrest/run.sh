#!/usr/bin/env bash
# Crown & Crest: fair dice, the face art and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/crowncrest
"${CXX:-c++}" -std=c++17 $FLAGS -O1 -I"$SRC" test_crowncrest.cpp "$SRC/crowncrest_save.cpp" -o "$OUT/test_crowncrest"
"$OUT/test_crowncrest"
