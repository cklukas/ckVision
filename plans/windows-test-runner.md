<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Native adopter test-header contract

The public `testing/cktest.hpp` runner must compile with native MSVC `/W4 /WX`
without a consumer defining `_CRT_SECURE_NO_WARNINGS`. The first native
application configuration contract exposed the former direct `std::getenv`
call's C4996 diagnostic; its strict build failed before a test could run.
Do not suppress that warning in an adopter or require project-local flags.

The runner copies CKTEST_FILTER into an owned string before selecting cases.
MSVC uses the CRT's allocated `_dupenv_s` copy, freed by scoped ownership;
other hosts copy the standard environment value. A test changing the process
environment cannot invalidate the remaining selection. Explicit command-line
selection still overrides the default, and an empty default matches all cases.
No new library dependency, worker thread or global mutable state is added.
An environment-copy error is reported with exit 2 before running any cases;
it must not broaden the requested selection. The native fixture forwards to
the actual CRT except in its explicit allocation-error child, which verifies
this failure path without exhausting host memory.

The native configuration-only contract compiles and passes five cases after
this fix, without a CRT suppression flag. This is not a native application or
full-library gate. An adopter-style standalone header target preserves the
compiler regression; real child processes prove environment mutation does not
change selection, explicit CLI override works, and an empty default is safe.

Authority: Microsoft documents allocation, errors and caller-owned freeing in
[`_dupenv_s`](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/dupenv-s-wdupenv-s?view=msvc-170).
Candidate gates by ckmux-windows Codex, isolated `d58d78bf` plus the owned
five-file header/test delta (before final record-only documentation edits):
Apple Clang strict Release 222/222, 64.74 s; Debian 13 ARM64 GCC 13.3 Release
222/222, 26.31 s; Linux Clang 19 Debug ASan/UBSan plus fuzz 232/232, 202.46 s;
GCC 13.3 Debug TSan 221/221, 292.06 s. All CTest exits 0, parallel 2.
Windows 11 ARM64 MSVC Release `/W4 /WX` passes 210/210 including the adopter
target and its explicit environment-copy error child, CTest exit 0. Mac and
native Windows benchmark `--gate` runs also exit 0. These library/fixture
counts do not claim a native adopter application or interactive acceptance.

Mac rebuild: 19:15:24Z–19:17:01Z, 97 s, output
`/Volumes/PRO-BLADE/tmp/ckmux-windows-20261003-01/mac-ckvision-filesystem`;
Linux rebuild outputs are the task guest's `ckvision-release`,
`ckvision-asan` and `ckvision-tsan`. Windows rebuild:
19:15:32.856Z–19:24:51.572Z, 558.700 s, output
`C:/Users/klukas/ckmux-windows-20261003-01/release`.
The native aggregate executable SHA256 is
`7667b8002a88e459279ed36c9b4d100441224de2f9f3fb16b411b5bc79bba4f5`;
the exact owned input-manifest SHA256 is
`5a92fe2200ebe7e446d26f06c76536cec20b87ba1dc7ebed22a61f5dd7b08298`.
The comparable preceding full header rebuilds took 68 s on Mac and
432.244 s on Windows. Final logs show the changed header's test translation
units rebuilt and the standalone adopter fixture linked, rather than trusting
elapsed time or an existing executable. The guarded CRT branch is absent on
POSIX: the final optimized aggregate executable hashes remain byte-identical
to the preceding successful Mac/GCC binaries, while the native executable
changes. No specific cause is attributed for the timing variation.
