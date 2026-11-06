#!/usr/bin/env bash
# Connections' rules, puzzle pack, JSON import, results, save packing and tile fit,
# checked on the PC. test_connections_fit walks every board in archive.idx/.dat (a
# copy of CrossPlay's host-tests/tilefit fixture) through the real Jersey metrics.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/games/connections
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_connections.cpp "$SRC/connections_rules.cpp" -o "$OUT/test_connections"
"$OUT/test_connections"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_pack.cpp "$SRC/connections_rules.cpp" "$SRC/connections_pack.cpp" \
  "$SRC/connections_results.cpp" "$SRC/connections_import.cpp" -o "$OUT/test_pack"
"$OUT/test_pack"
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_connections_save.cpp "$SRC/connections_rules.cpp" "$SRC/connections_save.cpp" -o "$OUT/test_connections_save"
"$OUT/test_connections_save"
# The fit test measures with the real fonts, so it links core/text and core/graphics,
# compiled against the preview tool's stand-ins for Arduino and the panel. The font
# headers leave optional fields unset, as tools/preview does.
STUB=../../tools/preview/stub
CORE=../../src/core
"${CXX:-c++}" -std=c++17 $FLAGS -Wno-missing-field-initializers -I"$SRC" -I../../src -I"$STUB" test_connections_fit.cpp "$SRC/connections_fit.cpp" \
  "$SRC/connections_rules.cpp" "$SRC/connections_pack.cpp" "$CORE/text.cpp" "$CORE/graphics.cpp" -o "$OUT/test_connections_fit"
"$OUT/test_connections_fit" .
