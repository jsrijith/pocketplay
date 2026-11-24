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
