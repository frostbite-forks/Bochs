#!/bin/sh
# Run Bochs benchmark and print elapsed time.
# Usage: run-bench.sh /path/to/bochs [bochsrc]

BOCHS="${1:-./bochs}"
BOCHSRC="${2:-cpu/jit/bench/bochsrc.txt}"

if test ! -x "$BOCHS"; then
  echo "Bochs binary not found: $BOCHS" >&2
  exit 1
fi

echo "Running benchmark with $BOCHS"
echo "bochsrc: $BOCHSRC"
(
  time "$BOCHS" -f "$BOCHSRC" -q
) 2>&1

echo "See log for IPS if --enable-show-ips was used at build time."
