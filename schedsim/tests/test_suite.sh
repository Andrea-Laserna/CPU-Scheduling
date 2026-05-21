#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT_DIR/schedsim"
WORKLOAD="$ROOT_DIR/tests/workload1.txt"

fail_count=0

assert_avg_tt() {
  local algo="$1"
  local expected="$2"
  local extra_args="$3"

  local output
  output=$("$BIN" --algorithm="$algo" $extra_args --input="$WORKLOAD")
  local actual
  actual=$(printf "%s\n" "$output" | awk -F':' '/Average Turnaround Time/ {gsub(/ /, "", $2); print $2}')

  if [[ "$actual" != "$expected" ]]; then
    echo "FAIL: $algo expected Avg TT $expected, got $actual"
    fail_count=$((fail_count + 1))
  else
    echo "PASS: $algo Avg TT $actual"
  fi
}

echo "Running scheduler tests..."

assert_avg_tt "FCFS" "515.00" ""
assert_avg_tt "SJF" "461.00" ""
assert_avg_tt "STCF" "393.00" ""
assert_avg_tt "RR" "651.00" "--quantum=30"

if [[ $fail_count -ne 0 ]]; then
  echo "Tests failed: $fail_count"
  exit 1
fi

echo "All tests passed."
