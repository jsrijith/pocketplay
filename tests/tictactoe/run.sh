#!/usr/bin/env bash
# Tic Tac Toe's rules, its computer player, its save and when it asks before a new game, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/tictactoe
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_tictactoe.cpp -o "$OUT/test_tictactoe"
"$OUT/test_tictactoe"
"${CXX:-c++}" -std=c++17 -O1 $FLAGS -I"$SRC" test_tictactoe_brain.cpp -o "$OUT/test_tictactoe_brain"
"$OUT/test_tictactoe_brain"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_tictactoe_save.cpp "$SRC/tictactoe_save.cpp" -o "$OUT/test_tictactoe_save"
"$OUT/test_tictactoe_save"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_tictactoe_flow.cpp -o "$OUT/test_tictactoe_flow"
"$OUT/test_tictactoe_flow"
