#!/usr/bin/env bash
# Draws a game's screens on the PC as PNGs. See README.md.
#
#   tools/preview/run.sh FOLDER FUNCTION OUT_DIR COMMAND...
#   tools/preview/run.sh dungeon dungeonGame /tmp/shots shot:menu tap:240,758 shot:board
set -euo pipefail
if [ "$#" -lt 4 ]; then
  echo "usage: $0 FOLDER FUNCTION OUT_DIR COMMAND..." >&2
  exit 2
fi
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$ROOT/tools/preview"
FOLDER="$1"
FUNCTION="$2"
OUT_DIR="$3"
shift 3

mkdir -p "$OUT_DIR"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT

"${CXX:-c++}" -std=c++17 -Wall -Wextra -Wno-missing-field-initializers -g -fsanitize=address,undefined \
  -DPREVIEW_GAME="$FUNCTION" -I"$HERE/stub" -I"$ROOT/src" -I"$ROOT/src/games/$FOLDER" \
  "$HERE/preview.cpp" "$HERE/storage_stub.cpp" "$HERE/net_stub.cpp" \
  "$ROOT/src/core/graphics.cpp" "$ROOT/src/core/text.cpp" "$ROOT"/src/ui/*.cpp \
  "$ROOT"/src/games/"$FOLDER"/*.cpp -o "$BUILD/preview"

(cd "$OUT_DIR" && "$BUILD/preview" . "$@")
python3 -I "$HERE/topng.py" "$OUT_DIR"/*.pgm
rm -f "$OUT_DIR"/*.pgm
ls "$OUT_DIR"/*.png
