#!/usr/bin/env bash
# PocketBGG: the line under each name, how the list splits into pages, and the config file,
# URL encoding and BGG's user answer, and the collection parser on a real answer cut down to
# five games (sample_collection.xml), and saving the collection, checked on the PC.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/apps/pocketbgg
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_pocketbgg.cpp "$SRC/pocketbgg_list.cpp" -o "$OUT/test_pocketbgg"
"$OUT/test_pocketbgg"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_config.cpp "$SRC/pocketbgg_config.cpp" -o "$OUT/test_config"
"$OUT/test_config"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_collection.cpp "$SRC/pocketbgg_collection.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_collection"
"$OUT/test_collection"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_save.cpp "$SRC/pocketbgg_save.cpp" "$SRC/pocketbgg_collection.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_save"
"$OUT/test_save"
