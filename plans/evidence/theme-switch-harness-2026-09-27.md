<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Full-frame theme-switch harness — 2026-09-27

This records the first implementation and verification of the separate
ARCHITECTURE §8 **< 5 ms full-screen recompose + diff** measurement. It is a
tool-and-workload record, **not an accepted pinned-host timing baseline**.
The Mac was busy throughout; the diagnostic run below saw a 1-minute load of
24.19 on a 10-core host. The idle-host timing run remains open alongside the
PTY event-to-bytes baseline in [p99-baseline-2026-09-26](p99-baseline-2026-09-26.md).

## Workload and source

`cvision_theme_switch_latency` switches a real `Application` between its
Classic and Dark themes over a 200×60 desktop, a status line and two overlapping
retained windows. The timed span starts after `set_theme()` invalidates the
tree and ends after `step()` repaints, composes, diffs and emits one frame to a
count-only terminal sink. The sink avoids PTY scheduling and virtual-display
parsing. Each transition must touch all 12,000 cells and emit one nonempty
diff. `theme_switch_work_gate` checks that obligation without judging time.

The source was `main` at `15387b2` plus the benchmark worktree edits below;
the new source contains all benchmark text and uses no external assets.
SHA-256 fingerprints before the final build and gate run:

| Input | SHA-256 |
|---|---|
| `benchmarks/theme_switch_latency.cpp` | `3171b6528cbeca89e55558c16c37e0645d185836cd3bba0d8ee68fb73ef7f0d5` |
| `benchmarks/CMakeLists.txt` | `54b5255880d20b3d1a6a357a99c16899359071534fe3843313398ea3aca14dfd` |
| root `CMakeLists.txt` | `e3bc0e9d5608e27ced484124c092e63d9221bd364491b4d883b7831ec12d25f0` |

## Build freshness

Before building, the checkout had one older project-owned Debug output at
`/Volumes/PRO-BLADE/git/ckvision/build` and no running ckVision application
from that output. Another project's sanitizer build was concurrently compiling
ckVision as its dependency in a separate checkout; none of its outputs was
removed or launched. The new output was isolated at
`/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/release-build`.
It was configured with AppleClang 21, Ninja, Release, examples and tests ON,
and warnings as errors. All temporary files used that task directory as
`TMPDIR`.

| Phase (UTC) | Elapsed from recorded start to final log write | Compiled/copied inputs | Result |
|---|---:|---:|---|
| Fresh benchmark-target build, 18:30:48–18:31:49 | 61 s | 101 Ninja actions, including the library, benchmark source and link | zero warnings/errors |
| Complete configured build, 18:34:19–18:37:58 | 219 s | 380 remaining Ninja actions, including examples, tests and tools | zero warnings/errors |
| Final source-change rebuild, 18:40:26–18:40:36 | 10 s | benchmark source compile and executable link | zero warnings/errors |

The two-phase fresh build covers all configured targets; the final incremental
rebuild contains the reporting change. The earlier Mac clean-build record in
[WP-37 evidence](../windows-wp37-evidence.md) was 76 seconds for a smaller
target/test set. It is not a comparable duration baseline for this 481-action
output. The logs show each compiled input, which explains the elapsed time
under the concurrently busy host. The final executable's timestamp is
2026-09-27 18:40:36 UTC, after the changed source, and its SHA-256 is
`0517e9a481e4be5700ce285a859abd575692fd522177200ef3e7729942ca3352`.

## Verification

- Complete Mac Release CTest suite: **218/218** passed in 54.08 seconds.
  It included `theme_switch_work_gate`, `provenance_check` and
  `provenance_checker_self_test`.
- After the final source-change rebuild, the theme workload, documentation
  links and both provenance gates passed again: **4/4**.
- A diagnostic **20-operation** run after four warm-ups returned valid JSON:
  p50 1.056 ms, p90 1.096 ms, p99 1.137 ms, maximum 1.137 ms, mean
  65,289 bytes/frame. The load average was 24.19/20.10/16.80 before and
  after; these figures are not a baseline.

The accepted gate still needs the default **1,000 measured transitions after
40 warm-ups** on the otherwise idle pinned Mac, with its JSON and source/output
fingerprints checked in. The literal limit applies to every measured
transition; later runs also compare p99 to the accepted baseline with a 5%
ratchet.
