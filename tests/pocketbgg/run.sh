#!/usr/bin/env bash
# PocketBGG: the line under each name and how the list splits into pages, checked on the PC.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/apps/pocketbgg
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_pocketbgg.cpp "$SRC/pocketbgg_list.cpp" -o "$OUT/test_pocketbgg"
"$OUT/test_pocketbgg"
