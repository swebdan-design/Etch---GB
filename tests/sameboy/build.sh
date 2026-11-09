#!/bin/sh
# Build the optional SameBoy-based headless test harness.
# Usage: tests/sameboy/build.sh /path/to/SameBoy   (already built with `make lib bootroms`)
set -e
SB="$1"
OUT="$(dirname "$0")/bin/sb_harness"
mkdir -p "$(dirname "$OUT")"
cc -O2 -I"$SB/Core" -o "$OUT" "$(dirname "$0")/harness.c" "$SB/build/lib/libsameboy.a" -lm
cp "$SB/build/bin/BootROMs/dmg_boot.bin" "$(dirname "$OUT")/dmg_boot.bin"
echo "built $OUT"
