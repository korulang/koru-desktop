# session/

Compositor/shell/panel composition — the non-driver half of the desktop.
Lands here once provider slots start filling: which compositor binds
(native `comp.zig` path vs Hyprland oracle vs nested wlroots), which
shell, which panel, and the first `import oshun` app as the acceptance
consumer.

Empty by design — session work is commissioned after the provider
catalog proves itself on `net-virtio-lwip`.
