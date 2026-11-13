#!/usr/bin/env bash
# Think Alike's question parser, deck and save packing, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/thinkalike
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" -DQUESTIONS_FILE="\"$SRC/questions.txt\"" test_thinkalike.cpp \
  "$SRC/thinkalike_questions.cpp" "$SRC/thinkalike_save.cpp" -o "$OUT/test_thinkalike"
"$OUT/test_thinkalike"
