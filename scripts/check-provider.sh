#!/usr/bin/env bash
# check-provider.sh <name> — run a provider slot's acceptance oracle.
# Thin wrapper: the oracle lives in providers/<name>/accept.sh; this
# exists so the command shape matches scripts/check-*.sh convention.
set -euo pipefail
cd "$(dirname "$0")/.."

name="${1:?usage: check-provider.sh <name>}"
entry="providers/$name"
[ -d "$entry" ] || { echo "check-provider: no such provider: $name" >&2; exit 1; }
[ -x "$entry/accept.sh" ] || { echo "check-provider: $name has no executable accept.sh" >&2; exit 1; }

"$entry/accept.sh"
