# Changelog

## v0.1.3 — 2026-10-25

- The EXP popup and F6 preview could remain suppressed despite successful helper initialization.
- Fix event-bitfield detection: persistent high flags such as `0x18` no longer count as an active cutscene by themselves. Low event blockers and existing gameplay guards remain enforced.
- The author confirmed F6 preview works after restarting with the corrected gameplay check. EXP-gain behavior was not separately retested for this fix.
- Close KH1 normally before updating; replace the previous copy, rebuild in OpenKH, and restart. Native helpers cannot be replaced with F1 reload.

## v0.1.2

- Minimal NEXT LEVEL label with EXP underneath.
- Align popup below Scan HP using the native left-HUD widescreen offset.
- Restore original notification title and number font settings.
- Preserve EXP-gain updates, F6 preview, fading, and normal level-up priority.
