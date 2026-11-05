#!/usr/bin/env bash
# TEMPLATE — copy into providers/<name>/accept.sh and make it real.
#
# Contract:
#   exit 0  = the slot's acceptance passed; artifacts must exist under
#             ./evidence/ (this script or its callees write them)
#   exit !=0 = failed or unimplemented — either way it must be able to
#             say NO. A check that can only confirm is not a gate.
#
# Rules (PROVIDERS_CHALLENGE.md):
#   - Prefer host-side verification: e2fsck, screendump pixel diff,
#     host cmp/dd, serial log greps for device events the guest cannot
#     fake. Guest self-report alone never passes.
#   - Name the QEMU device model(s) you rely on — see entry.md `qemu:`.
#   - Write artifacts under ./evidence/<run-name>/ before exiting 0.
set -euo pipefail
cd "$(dirname "$0")"

echo "accept.sh: unimplemented template — copy and make it real" >&2
exit 1
