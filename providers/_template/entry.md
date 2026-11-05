---
name: <slot-name>
iface: koruos/<iface>        # the provider row this slot must bind
arms: [unikraft/linuxu-x86_64] # arms it binds on; more arms = more binds
lane: donor-c                # compose | donor-c | silo | koru-lift | native-zig | upstream-koruos
home: this-repo              # this-repo | upstream-koruos | unikraft-fork | koru-libs
donor: unikraft-fork         # which tree the code lands in (if any)
needs: []                    # catalog entries that must be landed/waived first
accept: accept.sh            # the falsifying oracle — must be able to say no
qemu: -device <...>          # the device model this acceptance runs against
tier: qemu                   # qemu | hw — hw = real hardware, later rung
status: open                 # open | claimed | naive-wrap | idiomatic | landed | waived
evidence: D                  # V | M | C | D — grade of current claims
---

# <slot-name>

One paragraph: what the slot is, what exists today (with evidence class),
what the gap actually is. Cite pins (`origins.lock` SHAs), upstream
blockers (koru-os-cypoe BLOCKERS.md rows), and prior art.

## Acceptance

What `accept.sh` proves, step by step, and what host-side artifact
decides it. Name the independent oracle (e2fsck, screendump diff, host
`cmp`, libinput event stream — never guest self-report alone).

## Notes

Licensing, firmware blobs, known upstream defects, ordering constraints,
things measured vs assumed. `unmeasured` is a complete sentence.
