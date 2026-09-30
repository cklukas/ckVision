<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Tab presentations validation — 2026-10-01

D-117 and plans/widgets.md specify one TabControl with Underlined, Framed and
Compact presentations. Selection, focus, keyboard input and page ownership are
shared; geometry, drawing and pointer hit regions use the same layout. Behavior
was derived from ckVision specifications and its own executable evidence. No
external implementation source was used.

Coverage: 19 TabControl cases, Workbench switching and child containment, 120
appearance snapshots across four schemes, Unicode truncation, narrow bounds,
overflow, independent theme roles and exact-fit framed rails. Generated SVG
screenshots of all three variants were inspected. No interactive app was
launched; generated scenes do not establish a running application's window.

| Target | Build | Full CTest |
| --- | --- | --- |
| macOS Release | clean 149.733 s, 481 actions / 393 C++ compilations | 218/218, 16.87 s |
| Linux GCC Release | clean 191.233 s; final incremental 5 s | 218/218, 11.27 s |
| Linux Clang ASan/UBSan + fuzz | clean 303.573 s; final incremental 48.546 s | 228/228, 71.95 s |
| Linux GCC TSan | clean 192.343 s; final incremental 18.353 s | 217/217, 114.57 s |
| Windows VS2022 ARM64 Release | clean 791.039 s; final incremental 19.132 s | 204/204, 36.45 s |

Builds completed without warnings. Mandatory provenance checks and self-test,
unit/golden/script gates and benchmark budgets passed. Linux fuzz configuration
ran all ten corpus gates. Additional modern Windows ConPTY gate passed 22 cases.
Release benchmark smoke runs passed on macOS and Linux.

Comparable documented baselines: macOS full two-phase 280 s in
[theme-switch-harness-2026-09-27.md](theme-switch-harness-2026-09-27.md);
Linux clean 153–269 s in
[live-linux-examples-2026-09-27.md](live-linux-examples-2026-09-27.md);
Windows 522–1457 s in
[windows-rootdialog-2026-09-27.md](windows-rootdialog-2026-09-27.md).
No comparable sanitizer incremental baseline was available. Timing was not used
as freshness proof: final compiler logs prove all five changed C++ inputs were
compiled. Preserved archive mtimes initially skipped final inputs on remote
builds; those runs were rejected, inputs touched without byte changes, and all
current suites rerun. macOS final validation used a clean build without a
compiler launcher.

Current outputs retained:
- macOS: /Volumes/PRO-BLADE/tmp/ckvision-tabs-20260930-01/mac-release
- Linux: /home/klukas.guest/ckvision-tabs-20260930-01/release
- Windows: C:\Users\klukas\ckvision-tabs-20260930-01\release

Source snapshot: 3262 files, all local bytes verified; remote snapshots were
hash-verified after staging. Manifest SHA-256:
`c5410634315bd586b597e8f09c5623ace5689d57595aeafa1ce273873005b699`.
This evidence file is excluded from that pre-build snapshot.

Asset-map SHA-256 (sorted JSON mapping of generated docs and golden files):
`f5a00c69be2d7d15a0108352235160eae64db85f8f455e486f11c4b90af170d5`.

Output SHA-256:
- macOS libcvision.a: `ed065bea87ab74276ff041f773b791ccc6a5b2a6af00dbcfce4c2c08abb30432`
- macOS Workbench: `e29e7d75533f6332fa677be835717276ffa30ff39c4493cc8a830485791c87b5`
- Linux libcvision.a: `a7fa4c61c9ed35aed90dd0e5695f5f2b126869b69593fcabead592cf08feedbd`
- Linux Workbench: `30419fb36e0dc6058d738633816d4104732032bc343c8d0a80e82dd855b46d2e`
- Windows cvision.lib: `f53b49d20beaaabe75692983dcb57de0d9c9e87bf162fc7cd26e6c2b6a34df71`
- Windows cvision_tests.exe: `ae9dfcd70dbf235eb3dc73840478e6c03f477d10cea9de9bd4a8844ab5604309`

Cleanup removed nine obsolete Mac build trees, five Linux trees and four Windows
trees after project/path inventory. Final sanitizer build trees, staging archives and disposable generated copies
were removed; current Release outputs and their required sources are retained. Source/assets and another task's process were preserved.
