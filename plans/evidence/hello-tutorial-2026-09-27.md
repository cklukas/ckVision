<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Timed clean-checkout Hello tutorial — 2026-09-27

The literal M11 tutorial exit is a clean Mac checkout reaching a running
`ckvision_hello` in under ten minutes from the prerequisites in
[Getting started](../../docs/getting-started.md). This run reached its first
frame in **194 seconds (3 minutes 14 seconds)**. It then opened the greeting
dialog through the documented Alt+G chord, dismissed it with Enter, and exited
through Alt+X with status zero.

## Clean source and prerequisites

The source was cloned locally from `main` into
`/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/hello-clean-checkout`
before the clock started. `git status --short` was empty; there was no
`CMakeCache.txt` or build directory in that checkout. Its commit was
`f08b2aeaa3e3fb0d3587b1bd039e042669f79eb4` and its complete tracked
source/asset tree fingerprint was
`8cd3409525c422b4aee25132dda0209208816817`. The Mac used AppleClang
21.0.0, CMake 4.4.3 and Python 3.13.1, satisfying the documented C++20,
CMake 3.25+ and Python 3 prerequisites. No ckVision Hello process was running
before launch.

The one path substitution required by this project's Mac temporary-workspace
policy was to build under `/Volumes/PRO-BLADE/tmp`, not `./build`. `TMPDIR`
was the unique task directory above. The substantive documented commands were:

```bash
cmake -S . -B /Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/hello-build -DCMAKE_BUILD_TYPE=Release
cmake --build /Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/hello-build -j8
/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/hello-build/examples/ckvision_hello
```

## Time and build identity

| Event | UTC |
|---|---|
| Clock started, before configure | 18:46:43 |
| Configure log's final write | 18:46:54 |
| Hello executable's build timestamp | 18:48:08 |
| Complete build log's final write | 18:49:08 |
| First live Hello frame received; clock stopped | 18:49:57 |

The build completed with zero warnings/errors and linked `ckvision_hello`
and the complete `cvision_tests` target. The freshly linked executable at the
absolute path above was newer than the cloned Hello source (18:46:00 UTC),
and the configured CMake source directory was exactly the clean checkout.
There are no separate runtime assets bundled by Hello; the Git tree hash
covers its source and the generated build's executable fingerprint is
SHA-256 `c74b800d0cf7ccd50956361201067f1c4116258239b10ab6c5f2d0923f975882`.
The direct Hello source fingerprints were:

| Source | SHA-256 |
|---|---|
| `examples/hello/main.cpp` | `5b1017b07b35bfe426ce0f05988106f94dba9bd1ece59c87a4e171fe39e93657` |
| `examples/hello/hello_app.cpp` | `12a97701f7e983d4b8a0498ccf593346fc9e07768d77c356eac8d85768a64745` |

The earlier same-project fresh output built in two phases with 481 Ninja
actions over 280 seconds at two jobs. This documented tutorial used eight
jobs and finished its complete build in about 145 seconds. The clean clone,
compiler/link steps and source-to-executable timestamp ordering establish
freshness; the duration alone is not used as proof.

## Live application

The interactive PTY process was PID **51354**, resolving to the exact
`hello-build/examples/ckvision_hello` path above. Its first frame displayed
the File menu and the Alt+X Quit/F10 Menu status entries. Alt+G produced the
`Hello, World!` dialog containing `How are you?` and an OK button. Enter
dismissed that modal; Alt+X restored terminal modes and exited with status
zero. A final process check found no Hello instance.

After the timed run, the complete Mac Release suite in that exact clean
checkout passed **218/218** in 45.65 seconds, including
`provenance_check`, `provenance_checker_self_test` and
`theme_switch_work_gate`. The suite was run after the tutorial clock stopped;
it is verification of the committed source, not part of the 194-second time.

This proves the timed tutorial exit. It is a PTY-run application observation;
it is not the separate product-owner Mac walkthrough or release approval.
