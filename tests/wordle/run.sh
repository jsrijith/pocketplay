#!/usr/bin/env bash
# Wordle's rules and its small save, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/wordle
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I"$SRC" test_wordle.cpp "$SRC/wordle_rules.cpp" -o "$OUT/test_wordle"
"$OUT/test_wordle"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I"$SRC" test_wordle_save.cpp "$SRC/wordle_save.cpp" -o "$OUT/test_wordle_save"
"$OUT/test_wordle_save"
