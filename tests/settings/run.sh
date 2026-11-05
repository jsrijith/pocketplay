#!/usr/bin/env bash
# How settings turn into bytes, including files saved by earlier firmware.
set -euo pipefail
cd "$(dirname "$0")"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I../../src test_settings.cpp ../../src/settings/state.cpp -o "$OUT/test_settings"
"$OUT/test_settings"
