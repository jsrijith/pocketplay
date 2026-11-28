#!/usr/bin/env bash
# Tic Tac Toe's rules, its computer player, its save, its confirm and its play between two devices, checked on the PC without a panel.
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

# Nearby: whole matches between two devices over tests/link/FakeLink.h, with the link's
# real code and the stand-in radio from tests/link.
LINK=../../src/core/link
"${CXX:-c++}" -std=c++17 $FLAGS -O1 -I"$SRC" -I../../src -I../link test_tictactoe_link.cpp \
  $LINK/protocol.cpp $LINK/session.cpp $LINK/play.cpp $LINK/match.cpp ../link/radio_stub.cpp -o "$OUT/test_tictactoe_link"
"$OUT/test_tictactoe_link"
