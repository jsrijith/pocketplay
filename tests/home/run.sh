#!/usr/bin/env bash
# The home page's paging, checked on the PC without a panel.
set -euo pipefail
cd "$(dirname "$0")"
. ../flags.sh
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
SRC=../../src/shell
"${CXX:-c++}" -std=c++17 $FLAGS -I"$SRC" test_home_pages.cpp -o "$OUT/test_home_pages"
"$OUT/test_home_pages"
