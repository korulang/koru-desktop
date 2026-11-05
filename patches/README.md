# patches/

Cross-cutting donor patches — unikraft-core or library deltas that
more than one provider entry needs. Per-entry patches live in
`providers/<name>/patches/` (the koru-os-cypoe `patches/` +
`experiments/*/patches/` convention). Each patch file carries its
target SHA and upstream-submission status in its header.
