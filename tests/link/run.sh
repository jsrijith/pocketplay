#!/usr/bin/env bash
# The multiplayer link's protocol and session (core/link/), checked on the PC over
# FakeLink.h, a link that drops, delays, duplicates and reorders packets on purpose.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src
LINK=$SRC/core/link
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" $LINK/protocol.cpp $LINK/session.cpp test_link.cpp -o "$OUT/test_link"
"$OUT/test_link"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_name.cpp -o "$OUT/test_name"
"$OUT/test_name"

# Play<State>, what a game uses, over the same bad link. radio_stub.cpp stands in for
# the ESP-NOW radio, which only builds for the device.
PLAY="$LINK/protocol.cpp $LINK/session.cpp $LINK/play.cpp radio_stub.cpp"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" $PLAY test_play.cpp -o "$OUT/test_play"
"$OUT/test_play"

# The end of a match: the final board held, then ANOTHER GAME?, with Connect Four's
# rules, which are CrossPlay's unchanged.
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" $PLAY test_endgame.cpp -o "$OUT/test_endgame"
"$OUT/test_endgame"

# Match: pairing, the final board held, another game, leaving, and the goodbye before
# sleep, as every game gets them.
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" $PLAY $LINK/match.cpp test_match.cpp -o "$OUT/test_match"
"$OUT/test_match"
