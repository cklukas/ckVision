<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Widget presentation implementation and platform evidence — 2026-10-02

D-119 and [the thirteen-family plan](../widget-presentations.md) define the full
scope. All thirteen families are implemented through instance-owned settings
and shared interaction/geometry. Quiet defaults retain color-led fields and
unbracketed hotkeys. Alternatives add deliberate padding, grouping or containment.
Focus remains independent of selection/default emphasis and visible in Mono.
No dependency, global mutable facility or external implementation was introduced.
Behavior derives from ckVision's specification and its executable evidence.

| Family | Implemented presentation options |
| --- | --- |
| InputLine, ComboBox, SearchBox | Flat, Padded, Underlined; shared caret/content/accessory geometry |
| Progress | Solid, Block, Segmented; optional percentage |
| NotificationCenter | Lines, Banners, Framed; severity identity and explicit dismissal |
| PropertyInspector | Plain, Divided, Sectioned; owned groups, optional banding, aligned editor |
| Slider | Line, Block, ProminentThumb; optional value |
| CheckGroup, RadioGroup | Classic, BoxedRows, Buttons; selection independent of focus |
| SpinBox | Compact, Separate, Stacked; unbracketed field and explicit controls |
| Button | Classic, Flat, Padded, Outlined; default/focus separation |
| StatusLine | Plain, Grouped; accented hotkey before caption |
| BreadcrumbBar | Plain, Padded, Connected; overflow retained |
| Splitter | Line, CentralGrip, Gutter; shared drag geometry |
| ListView, Table | Optional row bands and table column dividers |
| Wizard | Compact, Bands, StepRail; real navigation buttons and validated focus flow |

Public headers and [the widget reference](../../docs/widget-gallery.md) document
configuration, geometry and behavior. The appearance catalog pins 452 states
for these seventeen widget types across four schemes: 1,808 dumps. Behavioral
cases exercise changed pointer regions, inert gaps, keyboard activation,
press cancellation, disabled/invalid/read-only states, tiny bounds and focus
transitions. Hot-path cases check zero steady allocations where preparation or
painting changed. Additional Tabs/ToolBar refinement removes decorative brackets
while preserving command and page behavior. Generated scene images were reviewed;
no claim is made about a running user-facing application window.

| Configuration | Clean build | Compiled inputs | Full suite |
| --- | --- | --- | --- |
| macOS Release | 153.586 s | 393 C++ inputs | 218/218 |
| Linux GCC Release | 160.785 s | 393 C++ inputs | 218/218 |
| Linux Clang ASan/UBSan + fuzz | 187.236 s | 405 C++ inputs | 228/228 |
| Linux GCC TSan | 173.944 s | 392 C++ inputs | 217/217 |
| Windows VS2022 ARM64 Release | 621.487 s | 369 primary objects | 204/204 |

All compiler lanes are warning-free. Ten fuzz corpus gates and all 22 modern
Windows ConPTY cases passed. Full suites include provenance and its self-test,
golden/visual/script checks and deterministic benchmark budgets. Release smoke
benchmarks passed on all hosts. Detailed timestamps, source identity, object
counts and executable hashes are in the adjacent
[platform record](widget-final-platform-gates-2026-10-02.json).

The clean runtime build snapshot contains 4,654 files and has archive SHA-256
`a6697c01b8ba8214f526e18a0cc74159a8a06bc8e43b94e6fc92cdb8aa0883ba`.
The later TSan deadline/documentation adjustment changed no C++ input or asset;
metadata rebuilds performed no C++ compilation. The final TSan suite passed
under its documented 300-second deadline; other configurations retain 120 seconds.
[The measurement record](widget-sanitizer-matrix-2026-10-02.md) explains why.
Final evidence/plan updates are documentation only.

The [performance diagnostic](widget-performance-diagnostic-2026-10-02.json)
recorded 10,000 PTY operations with overall p99 1.100208 ms, below 2 ms. All
1,000 theme transitions were below 5 ms; maximum was 1.426583 ms. Both harnesses
exited 0. The Mac was under other work and macOS service load; these results
are diagnostic evidence of the absolute limits, not an accepted idle-host
baseline or a 5% baseline-regression comparison. No accepted timing baseline
existed before this feature. The separate baseline procedure remains open.

Superseded Release builds and sanitizer trees were removed after preserving
records. Current verified Release outputs remain on each host. Verification VMs
were stopped; no project application remains running. Temporary scene captures
and test intermediates are disposable after the final documentation checks.
No commit was made for this feature.

## Remaining acceptance evidence

The feature scope, documentation, platform/sanitizer suites and deterministic
budgets are verified. Qualified pinned-host timing evidence remains unproven:
the current reference hardware is under another test workload and macOS service
load. The diagnostic results do not satisfy the idle-host baseline procedure.
The goal awaits that evidence. Three consecutive goal turns confirmed the same
host-load constraint; [the blocker record](widget-timing-blocker-2026-10-02.json)
captures the final recheck. No unrelated process was stopped.
