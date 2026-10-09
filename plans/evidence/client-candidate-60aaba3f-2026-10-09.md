<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# ckVision 0.1.18 client candidate — 2026-10-09

This is the WP-38 technical candidate record, not the owner's release
approval. The selected source revision is
`60aaba3f27149520c8750eafc1c2691e182115de` on `main`, with tree
`36eb995cbe920737dbbda50b4c8a6c259475b592`. A detached clean checkout
of that revision supplied the bundle. Its 5,009 tracked files were checked
against a SHA-256 manifest with zero mismatches; the manifest's aggregate
tracked-content SHA-256 is
`df9afba1264a58984aa9993d4df79ab00c4878b44a6ba82dfcea6b23d8622410`.
The 4,068 tracked assets have manifest SHA-256
`d79afc28cb3270152ba42e77a6101e12b80950d451144d93bea6b1b6818ef1cb`.
No Turbo Vision source or derivatives were consulted; the package repair
followed ckVision's own screenshot manifest and client handoff specification.
The [host audit JSON](wp38-candidate-host-audit-2026-10-09.json) preserves
exact build intervals, configurations, executable SHA-256 values, warnings and
full-suite counts independently of disposable build trees.

## Mac build and package

The source, temporary files and build outputs were kept under
`/Volumes/PRO-BLADE/tmp/ckvision-wp38-rc-Ob01bw`, after confirming the
external volume was mounted. The clean Release build output is
`build-mac-final`; its build ran from epoch `1791544579` to `1791544701`
(122 seconds), with 513 Ninja steps, 416 project object files built within
that interval, and zero compiler warnings. Comparable earlier clean builds
took 127 and 130.27 seconds. The build compiled `fe58aba4` source; the only
subsequent change in the selected `60aaba3f` revision was
`.github/workflows/ci.yml`, so a no-work rebuild at the selected revision
confirmed all compiled inputs and assets remained identical. The output
`examples/ckvision_progress` has SHA-256
`19ae11b041ce7121f257b85a69593ab5c30c5f023c8eec25c1300ed63b2820b2`;
`examples/ckvision_terminal` has SHA-256
`a8e06473d725c4d7ed0ff06a76a554c75fc2b1ba7ad11addf3438e560581379c`.

The full Mac Release CTest suite passed **233/233**, including
`provenance_check`, `provenance_checker_self_test`, package smoke, all golden
and script gates, `scene_budget_gate`, and `theme_switch_work_gate`. The clean
Mac Debug ASan/UBSan build took 155 seconds and its full CTest suite passed
**233/233**. `bench_smoke_run` completed with the deterministic budgets met.
The D-136 wall-clock PTY p99 and theme-switch baselines remain pending until
the reference Mac is otherwise idle.

The package script installed the SDK and examples, regenerated every figure
listed by `tools/docgen/screenshot-manifest.txt`, copied source documentation
and created the archive. The archive has 676 members, is 14,443,315 bytes,
and contains all **225** manifest SVG figures, each byte-identical to its
generated source. It contains the installed `ckvision_progress` and
`ckvision_terminal` executables with the verified build hashes above.

| Artifact | Exact path or identity |
|---|---|
| Bundle | `/Volumes/PRO-BLADE/tmp/ckvision-wp38-rc-Ob01bw/ckvision-client-0.1.18-60aaba3f` |
| Archive | `/Volumes/PRO-BLADE/tmp/ckvision-wp38-rc-Ob01bw/ckvision-client-0.1.18-60aaba3f.tar.gz` |
| Archive SHA-256 | `54146819e4e50450294a3bfc9a9ee4b3baea902316872eabf737e9230b3af8c2` |
| Source transport tar SHA-256 | `b7d41e669e06b5ad41d646cc2d02b646b4a49c5d376bf24b59c777d068a1fd53` |

## Remaining gates and approval

The Windows ARM64 clean Visual Studio 2022 Release build took 551.01 seconds,
against earlier comparable 580.23- and 829.2-second builds. It emitted zero
warnings; Progress Lab, Terminal and the test runner have output timestamps
inside that build interval. All 5,009 Windows source files matched the same
candidate SHA-256 manifest with zero mismatches. The Windows CTest suite
passed **227/227** in 103.8 seconds, followed by benchmark smoke and the
pinned modern ConPTY suite (**34/34** cases). The Windows VM was then shut
down so the reference Mac can become idle. Its full build, source verification
and test logs are retained under this task's `evidence/windows` directory.

The Debian 13 ARM64 rpi4 clean Release build completed in 1,880 seconds,
compiling 416 project object files within the recorded interval with zero
warnings. It was slower than the earlier 200-second clean Linux build on the
different, faster `ckdev` VM: the rpi4 kept four compiler processes busy and
reported 83.7–84.7 °C with active soft thermal limiting. The six changes
between its compilation source and the selected candidate are documentation,
CMake and shell/CTest scripts, with no C++ or asset byte changes. After these
were copied, all 5,009 tracked files matched the selected candidate manifest
with zero mismatches, CMake reconfigured, and Ninja reported no work. The
Release CTest suite passed **233/233** in 50 seconds, and benchmark smoke
completed. The full timing, input, output and test logs are retained under
this task's `evidence/linux-rpi4` directory.

The external-drive `ckdev` Debian ARM64 VM unpacked and verified the same
5,009-file candidate source manifest. Its clean Clang 19 Debug ASan/UBSan
build compiled 429 project objects and all 11 libFuzzer targets in 180
seconds, with zero warnings and build timestamps inside the interval. Its
full CTest run passed **244/244** in 92 seconds, including all 11 bounded
fuzz corpora and both provenance gates. Build and test logs, source and
output fingerprints, and the CTest result are retained under this task's
`evidence/linux-vm` directory. The completed sanitizer build tree was
removed after preserving those records to free the VM's external-drive disk
for the TSan lane.

The same VM's clean GCC 14 Debug TSan build compiled 415 project objects in
129 seconds with zero warnings and output timestamps inside the interval.
Its full CTest suite passed **232/232** in 203 seconds, including provenance,
package, golden, script and deterministic budget gates. The completed TSan
build tree was removed after its audit and full logs were retained under
`evidence/linux-vm`, and the VM was shut down.

The D-136 idle-host latency baselines and the owner's interactive Mac
walkthrough are the remaining Owner acts in the
[release audit](../../docs/release-audit.md).
Until those results and the owner's decision are entered, this remains a
technical candidate.
