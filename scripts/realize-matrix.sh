#!/usr/bin/env bash
# realize-matrix.sh — the scoreboard. Runs `koruc realize matrix` over
# every Koru program under providers/*/src (or programs declared in
# entry.md `programs:`), collecting arm-matrix.txt + rows.json per entry
# into evidence/matrix/<name>/.
#
# The output IS the gap catalog: each `refused:` line names a provider
# the arm can't bind; each `bound:` line names one it can.
#
# Usage: KORUC=/path/to/koruc bash scripts/realize-matrix.sh [arm ...]
#   default arms: unikraft/linuxu-x86_64 hosted koru-native
set -euo pipefail
cd "$(dirname "$0")/.."

KORUC="${KORUC:-$HOME/src/koru/zig-out/bin/koruc}"
[ -x "$KORUC" ] || KORUC="$(command -v koruc || true)"
[ -n "${KORUC:-}" ] && [ -x "$KORUC" ] || { echo "realize-matrix: koruc not found (set KORUC=)" >&2; exit 1; }

ARMS=("$@")
[ ${#ARMS[@]} -gt 0 ] || ARMS=(unikraft/linuxu-x86_64 hosted koru-native)

found=0
for entry in providers/*/; do
  name=$(basename "$entry")
  [ "$name" = "_template" ] && continue
  for prog in "$entry"src/**/*.k "$entry"src/*.k "$entry"programs/*.k; do
    [ -f "$prog" ] || continue
    found=1
    scratch=$(mktemp -d)
    cp "$prog" "$scratch/"
    (cd "$scratch" && for arm in "${ARMS[@]}"; do
       "$KORUC" "$(basename "$prog")" realize gen --arm "$arm" \
         >"realize-$arm.txt" 2>&1 || true
     done)
    mkdir -p "providers/$name/evidence/matrix"
    cp "$scratch"/realize-*.txt "providers/$name/evidence/matrix/" 2>/dev/null || true
    rm -rf "$scratch"
    echo "realize-matrix: $name <- $prog"
  done
done

[ "$found" = 1 ] || { echo "realize-matrix: no .k programs under providers/*/src|programs yet" >&2; exit 1; }
echo "realize-matrix: done — artifacts under providers/*/evidence/matrix/"
