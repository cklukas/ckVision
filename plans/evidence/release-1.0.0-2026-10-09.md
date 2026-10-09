<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# ckVision 1.0.0 release record — 2026-10-09

The owner directed a 1.0.0 release of the current framework and explicitly
rejected a bug-free interpretation of that version. D-137 records the public
compatibility commitment. The previously waived five-app walkthrough and
deferred D-136 idle-host p99/theme-switch baseline remain disclosed; neither
is a passing result. Formal roadmap milestone acceptance remains open.

The internal version-promotion source is commit
`948d6c556fb894531c094248b29ec01c36400d6e` (tree
`ec6032ce5e70e7270ac7d5bdc99fe304a09db2f3`). It changes the CMake
package and library version to 1.0.0, updates the literal version test,
regenerates 16 Terminal Report appearance goldens and its documentation
figure, and documents the compatibility and acceptance policy. The selected
public snapshot is commit `f3bea2ee050592df24426f3bab25d427fa8ab30a`
(tree `6111ce6f276f3a855fc1512492f043bdcc41a2dd`), a fast-forward
successor of the 0.1.19 public commit. It contains 4,885 of 5,012 tracked
internal paths; 239 private citations were rewritten in 143 files. The
internal plans and audit documents remain excluded by the publish filter.

## Verification

All Mac scratch, checkouts, builds, generated figures and the package are
under `/Volumes/PRO-BLADE/tmp/ckvision-v100.40Dis9`; the external volume was
verified mounted. No ckVision example instance was running while the builds
were prepared. The clean Mac ARM64 Release build ran 21:22:10–21:24:09 UTC
(119 seconds, between the recent comparable 92- and 122-second clean builds),
completed all 513 Ninja steps, rebuilt 416 C++ objects, and emitted zero
warnings. The full CTest suite passed **233/233** in 21.88 seconds, including
the mandatory provenance gates, golden captures and benchmark gates. Strict
`ckdocs` built 31 pages; 89 source-backed snippets and the documentation link
and screenshot checks passed.

The filtered public snapshot passed its own clean Mac Release build from
21:25:28–21:30:20 UTC (292 seconds using CMake Makefiles), recompiling 416
C++ objects with zero warnings. Its **227/227** non-PTY and **4/4** PTY CTest
entries passed, and `ckdocs --strict` built the public documentation site.
The slower elapsed time used a different generator than the internal Ninja
build; the compiled-input count and zero-warning build log prove a fresh
build. The public commit was pushed to GitHub `main` before packaging.

From a separate clean detached checkout of exactly that public commit, a Mac
ARM64 Release package build with testing disabled and examples/capture tools
enabled completed all 305 Ninja steps from 21:30:59–21:32:43 UTC (104
seconds), rebuilding 223 C++ objects with zero warnings. This is shorter than
the comparable 0.1.19 package build's 299 seconds under heavy host load; the
305-step log and fresh checkout show the package was not stale.

The resulting archive is `ckvision-client-1.0.0-f3bea2ee.tar.gz`,
**14,441,960 bytes**, SHA-256
`423bfd06a4a5663fd0208c5379e40a9376de84bc6fc1f42845315f823aa1136f`.
Its 675 tar members include 655 files. The installed CMake version file
declares `1.0.0`. All **225** generated SVG figures match the public source
bytes exactly; internal plans and the release audit are absent. The installed
Terminal and Progress binaries match their clean package-build executables:

| Mac ARM64 output | SHA-256 |
|---|---|
| `ckvision_terminal` | `48eff992ab782c58926a52a1edcd771d448e71a67f6d4c4fc94cf811505ff5ca` |
| `ckvision_progress` | `e376cafd4e4cbd3641d4c7119228bd01c1ab4d5fdd0a7516401526839cfb9c8f` |

The public checkout remained clean after packaging. The functional code and
portable contract are the same as the fully verified 0.1.19 release except
for version metadata and version-bearing evidence; its prior ARM64 host gate
record remains historical context rather than a claim of rerunning those
native host builds for 1.0.0.

The [exact-public-commit GitHub CI run](https://github.com/cklukas/ckVision/actions/runs/37993890917)
completed successfully: Linux, macOS and Windows Release, address/undefined/
thread sanitizers and fuzz all passed (**7/7 jobs**). The annotated `v1.0.0`
tag resolves to public commit `f3bea2ee`. The
[GitHub release](https://github.com/cklukas/ckVision/releases/tag/v1.0.0)
was published at 21:53:03 UTC. GitHub reports the same 14,441,960-byte asset
size and SHA-256 digest as the independently audited local archive.
