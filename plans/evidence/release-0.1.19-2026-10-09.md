<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# ckVision 0.1.19 release record — 2026-10-09

The selected internal source is `d186c5dffff322d58d26a026792fe7b21f51da02`
(tree `f2a948860623e3e387c4a11d8f485677384221fa`). Its 5,011 tracked
files match the SHA-256 source manifest in the release task with zero
mismatches; the manifest file SHA-256 is
`4143f0e06510d36fc0e7fd130eed3620bd7b0d2836c80ee26f485dd6be1aeb92`.
The publishable snapshot filters the repository according to
`tools/publish/publicignore` and is verified as its own tree. No source of
Turbo Vision or a derivative was read or used.

The public snapshot commit is `6b865375abaf6c92992f28e6d66b647178ac9e49`
(tree `aa48f98de3a80043683797adc8dc9527ec15be72`), a fast-forward
successor of the previous public `main`. It contains 4,885 of the internal
source's 5,011 tracked paths; 239 citations were rewritten in 143 files.
The snapshot's own clean Mac Release build had zero warnings, its 227
non-PTY and four PTY CTest entries passed, and `ckdocs --strict` built the
public documentation site. The exact snapshot was pushed to GitHub `main`;
internal planning and audit files were excluded by the publish filter.

From a second clean detached checkout of that exact public commit, a Mac
ARM64 Release package build with examples and capture tools completed all
305 Ninja steps between 20:52:49 and 20:57:48 UTC (299 seconds under high
host load) with zero warnings. The verified client archive is
`ckvision-client-0.1.19-6b865375.tar.gz`, **14,442,444 bytes**, SHA-256
`8695a3d208fd3d09716966a13acc258caca3e7cafea4d4465bbb4007711e13f8`.
Its 675 tar members include the installed Mac SDK and examples. All **225**
generated SVG figures match the public source bytes exactly. The installed
Terminal and Progress binaries match their clean-build executables with
SHA-256 `009c5a1e56490dfe46ce761cdb96a8dafe596fbcd155a5f91b989aff1fd19f69`
and `c9a4920ce0175963ded2febe88b3f5efe9b0340d7d309f96279e4c9a476a9753`
respectively. Internal planning and release-audit documents are absent from
the archive. The public checkout remained clean after packaging.

## Owner decision and outstanding acceptance

The owner approved release, explicitly **waived the interactive five-app Mac
walkthrough**, and directed us to **skip the idle-Mac p99 and theme-switch
baseline for this release**. These are exceptions to the WP-38 checklist,
not passing test results. The D-136 baseline remains open on the macOS 27.0.1
(build 26A434) reference host; M3 must not be marked Done on this evidence.

## Host verification

All Mac project scratch, source checkouts, build outputs and archives for this
run are under `/Volumes/PRO-BLADE/tmp/ckvision-wp38-v019.C6tX1e`; the external
volume was confirmed mounted. The clean Mac ARM64 Release build ran from
20:03:20 to 20:04:52 UTC (92 seconds), compiling 513 Ninja steps with zero
warnings. The previous comparable clean build was 122 seconds; all 416 C++
objects were rebuilt with no compiler launcher, so the shorter run was not a
stale or cached output. The full Release CTest suite passed **233/233**, and
benchmark smoke passed. After the bounded POSIX child-wait change, the five
POSIX shards and two mandatory provenance checks passed **7/7** on the final
source.

The clean Mac Debug ASan/UBSan build ran from 20:07:22 to 20:15:01 UTC (459
seconds under heavy unrelated host load), compiling 513 Ninja steps with zero
warnings. The full run passed 232 tests and hit only the prior 120-second
appearance-matrix timeout. The same generator passed all 3,551 generated
files directly, then its CTest entry passed with the sanitizer deadline set
to 600 seconds. Rebuilding the final POSIX test source passed the focused
five POSIX shards and two provenance checks **7/7**. The disposable sanitizer
build was removed after saving logs and executable SHA-256 values.

The `ckdev` Debian ARM64 VM verified the same 5,011-file manifest. Its clean
GCC 14 Release build used 513 Ninja steps, took 427 seconds under host load,
emitted zero warnings, passed **233/233** CTest tests, and passed benchmark
smoke. Its clean Clang 19 Debug ASan/UBSan plus 11 fuzz targets build used
538 Ninja steps, took 875 seconds, and emitted zero warnings. An initial full
CTest run encountered overloaded-host 60-second suite limits and 1-second
child synchronization waits. After the bounded test-only corrections at
`d186c5df`, the affected suites passed **9/9**, followed by a full **244/244**
CTest pass in 343.27 seconds, including all **11** fuzz corpora, the 133.09
second client bundle test, the 154.21 second appearance matrix, and both
provenance gates. The disposable sanitizer build was removed after saving
logs and executable SHA-256 values.

After the final POSIX test-only edit, the Linux Release tree was reconfigured
from the verified `d186c5df` source. Its incremental build compiled exactly
`test_posix_terminal.cpp` and relinked the test runner (two Ninja steps), with
zero warnings; product binaries and assets needed no rebuild. The **full
233/233** Release CTest suite then passed in 12.32 seconds, followed by
benchmark smoke. This closes the source-ordering gap between the original
clean build and the final selected revision.

The PRO-BLADE hosted Windows 11 ARM64 VM completed a clean Visual Studio
2022 ARM64 Release build from 20:40:00 to 20:59:17 UTC (1,157.22 seconds,
against the previous comparable 551.01-second build). Its log records 413
compiled C++ inputs and zero warnings; the Mac host's load rose above 100
while other work and two VMs were active, and the compiler stayed active
throughout the longer run. The 227/227 Windows CTest suite passed in 129.33
seconds, followed by benchmark smoke and the pinned modern ConPTY suite's
**34/34** cases. The final 5,011-file `d186c5df` manifest verified with zero
mismatches. Reconfiguration after copying the two final test-only sources
compiled **zero** C++ files, and the Progress, Terminal and test-runner SHA-256
values remained byte-identical to the clean build:

| Windows ARM64 output | SHA-256 |
|---|---|
| `ckvision_progress.exe` | `4a3238a15380b79f9f2b96164b9c4b7e1494e1412b135ce18f99d5e38929c92d` |
| `ckvision_terminal.exe` | `5591d76d53aa8a4bed1570abe219c1a64fe6c3ca1306bd5a6348094a63f39e90` |
| `cvision_tests.exe` | `ceb576dda43488e9c03e6f8942df2d56d023b7bbdc69a9e352abec162b8582f5` |

The clean GCC 14 Debug TSan build in `ckdev` completed 512 Ninja steps from
20:42:18 to 20:50:46 UTC (508 seconds under heavy Mac host load) with zero
warnings. Its first parallel CTest run encountered two 60-second visual
capture timeouts while the Mac package build and Windows VM were competing
for resources; no TSan race report or assertion failure occurred. After
stopping the Windows VM and running serially on the verified `d186c5df`
source, the **full 232/232** TSan CTest suite passed in 606.96 seconds. The
client bundle passed in 74.74 seconds, generated goldens in 28.61 seconds,
the appearance matrix in 146.24 seconds, and the scene budget gate in 59.38
seconds. The initially timed-out Spin and widget-shot visual captures passed
in 12.06 and 15.38 seconds. Both provenance gates passed in that full run.

The [public GitHub CI run](https://github.com/cklukas/ckVision/actions/runs/37989884456)
completed successfully on snapshot `6b865375`: fuzz, Linux Release, macOS
Release, Windows Release, ASan, UBSan and TSan all passed. The annotated tag
`v0.1.19` points to that public snapshot. The
[GitHub release](https://github.com/cklukas/ckVision/releases/tag/v0.1.19)
was published at 21:14:50 UTC with the client archive above; GitHub reports
the same SHA-256 digest and 14,442,444-byte size as the local verification.
