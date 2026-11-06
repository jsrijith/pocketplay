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

# The suites run at the same time, as many as there are cores, because the slowest
# ones (CrossPlay's own tests under the sanitizers) take most of a minute each. Each
# one writes to its own log, printed in order once all are done, so the output reads
# as if they ran one after another. Each run.sh builds in its own temporary folder.
logs="$(mktemp -d)"
trap 'rm -rf "$logs"' EXIT
max_jobs="$(nproc 2>/dev/null || echo 4)"
runs=()
for run in tests/*/run.sh; do
  [ -e "$run" ] || continue
  index=${#runs[@]}
  runs+=("$run")
  while [ "$(jobs -rp | wc -l)" -ge "$max_jobs" ]; do
    wait -n
  done
  # The status file is written only once the suite has finished, so a suite that
  # was killed leaves none and counts as failed.
  (
    bash "$run" >"$logs/$index.log" 2>&1
    echo $? >"$logs/$index.status"
  ) &
done
wait

for index in "${!runs[@]}"; do
  suites=$((suites + 1))
  cat "$logs/$index.log"
  if [ "$(cat "$logs/$index.status" 2>/dev/null)" != "0" ]; then
    fail "${runs[$index]}"
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
