<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Advanced progress delivery validation

Validated on 2026-10-03 against D-120 and the progress reference documentation.
Published Python-library documentation informed appearance requirements; no
outside implementation source was used.

## Delivered surface

Smooth eighth-cell meters, Sweep/Bounce/Pulse activity, built-in and custom
indicators, Unicode/ASCII and animated/static policies, a hierarchical task
model, responsive compact/detailed rows, timing estimates, scoped animation
ownership and bounded worker delivery are implemented. Workbench uses Smooth;
SysInfo uses overall/current-operation tasks. Progress Lab exposes presentation,
measurement, lifecycle, aggregation, columns, frames, timing and row-range
controls. Its keyboard and narrow-terminal resize paths have executable tests.

The guide and widget gallery contain compiled code examples and captured
figures. The appearance catalog adds 43 states across four schemes (172 dumps).
The screenshot manifest covers 225 figures, including 31 new progress figures.
Browser inspection confirmed the guide and gallery figures render.

## Gates

| Configuration | Result |
|---|---|
| macOS Release, clean, warnings as errors | 222 CTests passed |
| macOS ASan/UBSan | 222 CTests passed |
| Linux Release, warnings as errors | 222 CTests passed |
| Linux ASan/UBSan and fuzz corpus | 233 CTests passed |
| Linux TSan | 221 CTests passed |
| Windows ARM64 Release, clean, warnings as errors | 208 CTests passed |
| Modern Windows ConPTY | 22 cases passed |
| Progress hierarchy/frame/clipping fuzz run | 10,000 runs; no finding |

These include provenance and checker self-test, layering, license, documentation,
compiled snippets, screenshot freshness, appearance goldens and example hygiene.
Prepared painting allocates zero memory for every meter style, both row layouts
and indicator phases. Existing deterministic work-budget gates and benchmark
smoke passed. Timing diagnostics are not a new pinned p99 baseline acceptance.

## Freshness and retained outputs

The final Mac clean build ran 130.27 seconds versus a recent comparable 153.59
seconds. All 403 project compilation objects were written inside its build
interval; no compiler cache launcher was configured. Source and figure hashes
were unchanged through validation. The generated install-consumer test fixture
was checked separately from project compilation objects.

Windows was rebuilt clean after detecting stale consumers caused by preserved
archive timestamps. The clean rebuild took 580.23 seconds versus the earlier
829.20-second run. Subsequent source transfer used arrival timestamps, verified
all source/figure hashes, and rebuilt changed inputs before the final tests.
The Linux initial clean builds were slower while three compiler lanes ran
concurrently; fresh directories, compile logs and input hashes established
freshness independently of elapsed time.

The canonical C/C++/CMake input fingerprint is
`16b21e9b8cb339ae0857ae29e202eedbf175ce830643df99d529b963c52e71e6`.
Logs, source/asset manifests, executable hashes, build intervals, CMake caches
and test reports are retained beneath
`/Volumes/PRO-BLADE/tmp/ckvision-advanced-progress-lMKqiP/evidence`.
Superseded build trees were removed after preserving evidence.

Current runnable outputs:

- Mac: `/Volumes/PRO-BLADE/tmp/ckvision-advanced-progress-lMKqiP/current-release/examples/ckvision_progress`
- Linux: `/home/klukas.guest/ckvision-progress-lMKqiP/release/examples/ckvision_progress`
- Windows: `C:\Users\klukas\ckvision-progress-lMKqiP\release\examples\Release\ckvision_progress.exe`

The local browser guide is served at `http://127.0.0.1:8766/progress.html`.
