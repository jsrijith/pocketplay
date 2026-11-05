#!/usr/bin/env bash
# Tic Tac Toe's rules, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/tictactoe
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I"$SRC" test_tictactoe.cpp -o "$OUT/test_tictactoe"
"$OUT/test_tictactoe"
