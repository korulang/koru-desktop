#!/usr/bin/env bash
# board.sh — reconcile the catalog against reality. Fails loud when:
#   - an entry claims landed|naive-wrap|idiomatic with no evidence/
#     artifacts (self-report is not evidence)
#   - a claimed+ entry's `needs:` aren't all landed|waived
#   - entry.md frontmatter is missing required fields
# Prints the slot table either way.
set -uo pipefail
cd "$(dirname "$0")/.."

FAIL=0
status_of() { # $1=name -> status field
  awk '/^status:/{print $2; exit}' "providers/$1/entry.md" 2>/dev/null
}
needs_of() { # $1=name -> needs list (comma/space separated, [] = none)
  awk '/^needs:/{gsub(/[\[\]]/,""); print; exit}' "providers/$1/entry.md" 2>/dev/null \
    | tr ',' ' '
}

declare -A STATUS
for d in providers/*/; do
  n=$(basename "$d"); [ "$n" = "_template" ] && continue
  [ -f "$d/entry.md" ] || { echo "board: $n missing entry.md"; FAIL=1; continue; }
  s=$(status_of "$n"); STATUS[$n]="$s"
  for f in name iface arms lane home needs accept tier status evidence license provenance; do
    grep -q "^$f:" "$d/entry.md" || { echo "board: $n entry.md missing field: $f"; FAIL=1; }
  done
  case "$s" in
    landed|naive-wrap|idiomatic)
      if [ -z "$(find "$d/evidence" -type f 2>/dev/null | head -1)" ]; then
        echo "board: $n status=$s but evidence/ is empty — landed claims need artifacts"; FAIL=1
      fi ;;
    claimed)
      for dep in $(needs_of "$n"); do
        case "${STATUS[$dep]:-$(status_of "$dep")}" in
          landed|waived) ;;
          *) echo "board: $n claimed but needs $dep (${STATUS[$dep]:-missing})"; FAIL=1 ;;
        esac
      done ;;
  esac
done

printf '%-18s %-12s %-6s %s\n' SLOT STATUS TIER EVIDENCE
for n in "${!STATUS[@]}"; do
  ev=$(awk '/^evidence:/{print $2; exit}' "providers/$n/entry.md")
  tier=$(awk '/^tier:/{print $2; exit}' "providers/$n/entry.md")
  printf '%-18s %-12s %-6s %s\n' "$n" "${STATUS[$n]}" "$tier" "$ev"
done | sort

[ "$FAIL" = 0 ] && echo "board: clean" || { echo "board: FAILURES above" >&2; exit 1; }
