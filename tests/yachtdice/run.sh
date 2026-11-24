#!/usr/bin/env bash
# Yacht Dice's rules, opponent and flow (CrossPlay's test_yahtzee.cpp, unchanged
# apart from its includes), and the save packing.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/yachtdice
"${CXX:-c++}" -std=c++17 $FLAGS -O2 -I"$SRC" test_yachtdice.cpp -o "$OUT/test_yachtdice"
"$OUT/test_yachtdice"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_yachtdice_save.cpp "$SRC/yachtdice_save.cpp" -o "$OUT/test_yachtdice_save"
"$OUT/test_yachtdice_save"

# Nearby: whole games between two devices over tests/link/FakeLink.h, with the link's
# real code and the stand-in radio from tests/link.
LINK=../../src/core/link
"${CXX:-c++}" -std=c++17 $FLAGS -O1 -I"$SRC" -I../../src -I../link test_yachtdice_link.cpp \
  $LINK/protocol.cpp $LINK/session.cpp $LINK/play.cpp $LINK/match.cpp ../link/radio_stub.cpp -o "$OUT/test_yachtdice_link"
"$OUT/test_yachtdice_link"
