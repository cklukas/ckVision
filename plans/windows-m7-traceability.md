<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# M7 Windows exit traceability — 2026-09-26

This is the M7 slice of WP-38's release traceability audit, against the literal
[M7 exit criteria](../ROADMAP.md#m7--windows-platform--full-matrix-2-pw-named-windows-host-available-acceptance-open).
It records the current executable evidence without promoting M7 or WP-38 to
Done. The named host is the PRO-BLADE-backed Windows 11 25H2 ARM64 VM, using
Windows Terminal Preview 1.25.1912.0 and the signed-in `cktest` desktop. Exact
build identities, host versions, commands, process records, and captures are in
[WP-37 evidence](windows-wp37-evidence.md) and
[WP-50 evidence](windows-wp50-evidence.md).

| Literal M7 obligation | Corrective WP and implementation | Executable and live evidence | Public reference | Result |
|---|---|---|---|---|
| Windows VT backend, VT-first input, resize, and clipboard | WP-37; `src/term/windows_terminal.cpp`, `src/term/input_decoder.cpp`, `src/term/windows_clock.cpp` | Windows hosted suites; live Preview keyboard, native Unicode clipboard export/import, held cell mouse, host and private-child resize; [WP-37 record](windows-wp37-evidence.md) | [Terminal host integration](../docs/terminal-host-integration.md) | **Done** for the named cell/keyboard/clipboard/resize routes |
| Gallery Sixel graphics in Windows Terminal | WP-32/WP-37; `examples/gallery`, presenter and raster scene | Live Preview Gallery Sixel occlusion/redraw; symbolic and decoded [Gallery goldens](../tests/golden/gallery_sixel.visual); [WP-37 record](windows-wp37-evidence.md) | [Graphics](../docs/graphics.md) | **Done** for the named Sixel route |
| Gallery pixel-mouse interaction in Windows Terminal | WP-37; `src/term/windows_terminal.cpp`, `src/term/input_decoder.cpp`, `examples/gallery/gallery_app.cpp` | The fresh signed-in WezTerm Windows Gallery opened File → Terminal Report through real mouse clicks. The report showed `Pixel mouse: yes`, a 10×22 px cell metric, and a dispatched event with both cell and pixel coordinates; [live Gallery record](windows-wp37-evidence.md#live-gallery-pixel-capability-report--2026-09-26). The native fixture also completed a pixel-bearing held drag. Windows Terminal Stable and Preview still report no mode 1016. | [Terminal host integration](../docs/terminal-host-integration.md), [Gallery](../docs/example-apps.md#gallery-examplesgallery) | **Gallery pixel interaction Done** on a capable Windows host; **Open** for literal Windows Terminal mode-1016 support |
| Three-platform suites, including PTY/ConPTY smokes | WP-37/WP-50; Windows private adapter `src/term/windows_terminal_subsession.cpp` | Recorded 177/177 macOS, 177/177 Linux, 164/164 Windows hosted suites; modern and inbox Windows ConPTY suites 21/21 each; supported sanitizers, Linux fuzz corpora, and pinned benchmarks are recorded in [WP-37 evidence](windows-wp37-evidence.md) | [Embedded terminal](../docs/embedded-terminal.md) | **Done** for recorded suites; final WP-38 audit remains Open |
| Byte-identical platform-independent goldens | WP-32/WP-38/WP-48/WP-50; `tools/docgen/generate_terminal_plane_goldens.cpp`, `tools/docgen/generate_editor_goldens.cpp`, `tests/check_generated_goldens.py` | 119 symbolic, cell, and RGBA outputs match byte for byte on macOS, Linux, and Windows ARM64; [current manifest audit](wp38-editor-terminal-appearance.md) and [earlier 67-file checkpoint](windows-wp37-evidence.md#three-host-two-child-golden-checkpoint--2026-09-25) | [Embedded terminal](../docs/embedded-terminal.md) | **Done** for the declared manifest |
| Plain-VT ConPTY without enhanced input | WP-37; `src/term/windows_terminal.cpp`, `src/term/input_decoder.cpp` | Native private-ConPTY no-enhanced-reply run verified legacy keyboard and SGR cell mouse, without pixel mouse, metrics, or Sixel; [WP-37 record](windows-wp37-evidence.md) | [Terminal capability profiles](../docs/terminal-profiles.md) | **Done** for the named degraded route |
| Legacy Console Host refusal with actionable diagnostic | WP-37; `src/term/windows_terminal.cpp` | Redirected-handle diagnostic passes; the VM's `ForceV2=0` legacy host did not start even for `cmd.exe`, so the real host refusal path was never reached; [legacy audit](windows-wp37-evidence.md#legacy-console-host-availability-audit--2026-09-25) | [Terminal host integration](../docs/terminal-host-integration.md) | **Open**: requires a usable legacy host |
| Embedded Windows child sessions required by M11 and consumed by M7 | WP-50; `src/term/windows_terminal_subsession.cpp`, `examples/terminal` | All seven named live scenarios [marked Done](embedded-terminal-delivery-target.md#wp-50-named-windows-scenario-status--2026-09-25); portable contained-scene goldens match three hosts; [WP-50 record](windows-wp50-evidence.md) | [Embedded terminal](../docs/embedded-terminal.md) | **Done** for WP-50; M7 retains its separate open host rows |

M7 remains **Open** while literal Windows Terminal pixel-mouse support and real
legacy-console refusal are unverified. WP-38 also requires the complete
M0–M11 appearance, example, reference, and release-gate audit; this M7 slice
is evidence for that work, not its completion.
