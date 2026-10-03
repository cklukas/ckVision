<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Portable lifecycle candidate verification — 2026-10-03

Scope: committed base `6e568df325ff39dd827911745dd92031a660b51f`
plus the portable lifecycle/selective-snapshot delta and version 0.1.9.
These are candidate-byte gates, not assertions about an untested commit.
All runs were performed by the ckmux-windows Codex session. Unrelated
advanced-progress work in the shared checkout was excluded.

| Host/configuration | Result |
|---|---|
| macOS Apple Clang, Release, warnings as errors | CTest 219/219, exit 0; benchmark smoke exit 0 |
| Debian 13 ARM64, GCC 13.3, Release, strict warnings + FORTIFY | CTest 219/219, exit 0; benchmark smoke exit 0 |
| Debian 13 ARM64, Clang 19, Debug ASan/UBSan, fuzzers enabled | CTest 229/229, exit 0, including corpus runs; benchmark smoke exit 0 |
| Debian 13 ARM64, GCC 13.3, Debug TSan | CTest 218/218, exit 0, 529.06 s |
| Windows 11 25H2 ARM64, MSVC 2022, Release /W4 /WX | CTest 205/205, exit 0; modern ConPTY 24 cases, exit 0; benchmark smoke exit 0 |

The native Windows lifecycle case uses the portable base interface, keeps
a stubborn root/descendant alive during graceful handling, exercises a
responsive peer, then proves both OS process handles signal after job
escalation. A compiled no-op `request_kill()` mutation produced four failed
checks and test exit 1. Restoring the adapter and rebuilding preceded the
final full native gate. POSIX lifecycle and nonempty history/raster cases
exercise the same portable interface; selected snapshots retain the grid.
Provenance checks and their self-tests passed in the full inventories.

## Freshness and exact outputs

The 4,716 tracked source/asset fingerprints were verified in the native
guest against manifest SHA256
`ce27208bc88f30edaeb423efbbdddea8b06ebffefe8c53573cf95e4a99148d72`.
The tracked delta SHA256 is
`56d8d149c03f19cb0612a99d9041096ce45b645aefb70664d0122fe18a239640`;
planning/evidence documents are not compiled inputs.

Windows clean build: 2026-10-03T12:37:40.9253019Z to
12:52:21.1046320Z, 880.1705997 s, 371 freshly compiled objects. The recent
same-project ARM64 Release baseline was 621.487051 s / 369 objects in
`evidence/widget-final-platform-gates-2026-10-02.json`. The slower build was
checked against actual compile actions and fresh input hashes, rather than
accepted on elapsed time. Final restored incremental build: 13:11:36.5986507Z
to 13:11:44.5796760Z, 7.9745765 s, both changed test translation units rebuilt.
Output: `C:\Users\klukas\ckmux-windows-20261003-01\release\tests\Release\cvision_tests.exe`,
SHA256 `cfcc86ffac9b3f95dd117c8447e074416eeaa1c22c21c4ccac49eed5e08f1f77`.

macOS verified output:
`/Volumes/PRO-BLADE/tmp/ckmux-windows-20261003-01/mac-ckvision/tests/cvision_tests`,
SHA256 `75f0dbcac29c0ebb3a297f8053afa757d4b4e6267b384ee75dbfcb720977c0aa`.
Linux outputs are in `/home/klukas.guest/ckmux-windows-20261003-01/ckvision-{release,asan}`.
Debug sanitizer timings are not optimized performance baselines.

TSan build: 2026-10-03T13:20:18Z to 13:21:59Z, 100.504 s, 483 Ninja
actions, no warnings. Output: `/home/klukas.guest/ckmux-windows-20261003-01/ckvision-tsan`.
The 218-test inventory intentionally omits the incompatible allocation-probe
gate present in Release; it is not a claim of Release's identical inventory.

Instrument repairs: AppleDouble archive entries were removed from task-owned
guest sources; a corrected transfer excluded them. Preserved archive mtimes
initially let Windows reuse the old version-test object: changed test inputs
were made newer, compile actions checked, and the full native run repeated.
Frozen per-run Bash drivers and explicit PowerShell native exit capture
prevent wrapper failures or expected mutation stderr from masquerading as
test outcomes. No live user-facing application was launched or visually
certified by these gates.
