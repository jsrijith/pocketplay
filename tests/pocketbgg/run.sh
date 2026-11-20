#!/usr/bin/env bash
# PocketBGG: the line under each name, how the list splits into pages, and the config file,
# URL encoding and BGG's user answer, and the collection parser on a real answer cut down to
# five games (sample_collection.xml), saving the collection, the filters, and
# the plays read for last-played dates (sample_plays.xml), thumbnails and the covers saved, checked on the PC.
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
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_collection.cpp "$SRC/pocketbgg_collection.cpp" "$SRC/pocketbgg_xml.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_collection"
"$OUT/test_collection"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_save.cpp "$SRC/pocketbgg_save.cpp" "$SRC/pocketbgg_collection.cpp" "$SRC/pocketbgg_xml.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_save"
"$OUT/test_save"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_filter.cpp "$SRC/pocketbgg_filter.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_filter"
"$OUT/test_filter"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_plays.cpp "$SRC/pocketbgg_plays.cpp" "$SRC/pocketbgg_xml.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_plays"
"$OUT/test_plays"
# Thumbnails: stb_image decodes, the SDK's tone mapper dithers. The images come from make_images.py.
TONE=../../freeink-sdk/libs/assets/ImageToneMapper
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" -I"$TONE/include" test_image.cpp "$SRC/pocketbgg_image.cpp" "$SRC/pocketbgg_stb.cpp" "$TONE/src/ImageToneMapper.cpp" -o "$OUT/test_image"
"$OUT/test_image"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_thumbs.cpp "$SRC/pocketbgg_thumbs.cpp" "$SRC/pocketbgg_list.cpp" -o "$OUT/test_thumbs"
"$OUT/test_thumbs"
