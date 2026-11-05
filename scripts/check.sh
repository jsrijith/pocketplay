#!/usr/bin/env bash
# Runs every PC test. platformio.ini runs this before each firmware build.
# Read the last line: only "CHECK-VERDICT: green" is a pass, and no verdict line
# means the run was killed or crashed.
set -uo pipefail
cd "$(dirname "$0")/.."

failed=0
suites=0

fail() {
  echo "FAIL  $1"
  failed=1
}

# A test file that no run.sh mentions would never run, and would look like coverage.
for test_file in tests/*/test_*.cpp; do
  [ -e "$test_file" ] || continue
  folder="$(dirname "$test_file")"
  if [ ! -f "$folder/run.sh" ]; then
    fail "$test_file: $folder has no run.sh"
  elif ! grep -qF "$(basename "$test_file")" "$folder/run.sh"; then
    fail "$test_file: not run by $folder/run.sh"
  fi
done

for run in tests/*/run.sh; do
  [ -e "$run" ] || continue
  suites=$((suites + 1))
  if ! bash "$run"; then
    fail "$run"
  fi
done

if [ "$suites" -eq 0 ]; then
  fail "no tests found"
fi

if [ "$failed" -ne 0 ]; then
  echo "CHECK-VERDICT: failed"
  exit 1
fi
echo "CHECK-VERDICT: green"
