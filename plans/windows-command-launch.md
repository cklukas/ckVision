<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Windows command-processor launch

Status: final API correction gates pass; clean isolated commit gate and
publication pending.

The full-gate candidate prepares release 0.1.11, without moving 0.1.10.
Version assertions, all sixteen TerminalReportDialog references and its
generated documentation figure are updated together. The figure matches
the fresh capture byte-for-byte; this headless capture is not a live UI gate.

Additional adopter testing found that cmd parses forward-slash image path
components as switches even with an explicit CreateProcessW application name.
Eight relocated-image cases fail with forward/mixed separators and pass with
native backslashes, using identical command, cwd and ComSpec values. D-125 now
encodes only the explicit cmd image token with native backslashes, leaving
ordinary argv and command syntax unchanged. Its typed launch value distinguishes
interactive cmd (no command) from one-shot execution (including an empty string).
New real-child coverage exercises both modes and both separator spellings in
ASCII/Unicode paths with and without ampersands. The full gates below describe
the earlier candidate; they do not certify this correction or the revised API.

## Final typed API and image-token correction gates

Run by ckmux-windows Codex against source/asset manifest SHA256
`080de577b4b3aaa130aca8d445ee2ec0f38b73600f2268f608474f887df56c73`:

- Apple Clang strict Release: 222/222, CTest exit 0, 72.29 s at parallel 2;
  benchmark gate exit 0. Broad 326-action header rebuild: 197 s. Its compile
  commands name this isolated source, version 0.1.11 and the strict flags;
  source hashes match before and after. The interval overlaps other task
  verification; it is not a new performance baseline or a freshness claim
  based on elapsed time alone.
- Debian 13 ARM64 GCC 13.3 strict Release: 222/222, exit 0, 32.87 s at parallel 2;
  build 104 s, benchmark gate exit 0.
- Linux Clang 19 Debug ASan/UBSan plus fuzz: 232/232, exit 0, 163.71 s at
  parallel 2; build 188 s.
- Linux GCC 13.3 Debug TSan: 221/221, exit 0, 275.00 s at parallel 2;
  build 146 s. This configuration has a different test inventory.
- Windows 11 25H2 ARM64 MSVC strict Release: 210/210, exit 0, 92.89 s at
  parallel 2; benchmark gate exit 0 and modern ConPTY 32/32 real cases.
  Build 395.705 s, 2026-10-03T23:37:36.579Z–23:44:12.289Z; runner SHA256
  `40fe8575ccc402cb10e115de434bb256a5aa761a6a81787e40f3ca385148f761`.

A compiled mutation removing only native image-token separators fails eight
native output assertions (CTest exit 8), build exit 0. Mutated adapter SHA256
`dfb240191b372cff977b96614b409ca6a0ceaffcfb500452290caa72328e45e8`.
Restoration matches `053d424b3b0f06b7626914d203a48564983ef9f1107ade3f22e975987c680d90`,
builds successfully and passes 32/32 native terminal cases, CTest exit 0,
8.67 s, before the final full gates. No mutation remains.

The first revised-API snapshot omitted the new public field's reference
comment. Its full Mac/Linux/Windows sweeps correctly fail only the API-comment
check; those attempts are not green full gates. The field comment is present
in the final manifest above. This evidence note follows that freeze and must
be included in the clean isolated commit gate. No GUI app was launched or
visually certified by these headless gates.

D-125 makes cmd execution an explicit launch-spec form. A generic MSVC argv
encoder is correct for native argument-vector programs, but cmd parses its
command remainder differently. The 0.1.10 native Windows 11 25H2 ARM64 probe
fails simple and quoted-text cmd commands with exit 1; both PowerShell cases
pass. The probe's first strict build correctly rejected an optional exit
value passed to printf; it was corrected before executing any probe. This
failed compile is not a negative command-launch test.

Focused gates by ckmux-windows Codex:

- Apple Clang strict Release: 2/2 CTest entries, two real cases, exit 0,
  0.30 s. POSIX rejects the Windows-only form before forking; ordinary argv
  still launches. Fresh 102-action focused build: 15 s; no exact previous
  harness baseline. The Windows-only test file is not compiled on this host.
- Windows 11 25H2 ARM64 MSVC strict Release: 3/3 CTest entries, seven cases,
  exit 0, 1.63 s. Command builtins/quotes/Unicode, quoted Unicode executable
  path with spaces/ampersand and operators, actual exit status, invalid/NUL/
  oversized input, argv conflicts, PowerShell and ordinary argv are covered.
  Native manifest SHA256 `81b4267c847051d82bd093ca1e26a2dd1c39321b1f757ba93f7419635cc87d7b`.
- A compiled mutation restoring the broken quoted-options/argv encoding
  fails nine cmd assertions (CTest exit 8), while PowerShell and ordinary
  argv still pass. Build exit 0, changed adapter hash
  `8185e30351110a1e0c23b9b7b202bba716e758b3687bb3be02509f8f022dab5e`.
  Restoration matches adapter hash
  `7fa490109a0fce375464d7ef72893a7e3d617fbf892ad661a36fcf8cc5b34c6c`,
  rebuilds successfully and passes 3/3 again (1.93 s).

Native setup initially failed before compilation because the harness passed
a backslash path through CMake without normalization. Its first strict test
compile then rejected deprecated C++20 u8path calls; tests now construct a
path from char8_t text, without warning suppression. These failed attempts
are retained, never counted as behavior tests. The focused harness and new
test file are not a complete native application or library-suite gate.

The core header and POSIX adapter compiled by the Mac gate are byte-identical
before/after the native test-only correction. This private evidence update
follows those candidate gates; final source/asset freezes include it separately.

## Full 0.1.11 candidate gates

Run by ckmux-windows Codex against frozen source/asset manifest SHA256
`d9a9021a67edbc5accb56f4baa8c2515858dbd0753e31523ce67291d514c3e47`:

- macOS Apple Clang strict Release: 222/222, CTest exit 0, 44.07 s at parallel 2;
  benchmark gate exit 0. The remaining 369-action build took 56 s after the
  separately recorded docgen targets; it is not a whole clean-build timing.
- Debian 13 ARM64 GCC 13.3 strict Release: 222/222, exit 0, 22.61 s at parallel 2;
  benchmark gate exit 0. Build 91 s, 387 actions.
- Linux Clang 19 Debug ASan/UBSan plus fuzz corpus: 232/232, exit 0, 196.55 s
  at parallel 2. Build 150 s, 400 actions.
- Linux GCC 13.3 Debug TSan: 221/221, exit 0, 293.99 s at parallel 2.
  Build 99 s, 386 actions; this configuration has a different test inventory.
- Native Windows 11 25H2 ARM64 MSVC strict Release: 210/210, exit 0, 101.98 s
  at parallel 2; benchmark gate exit 0. Modern ConPTY adds 30/30 real cases.
  Build 619.544 s: changing the version definitions rebuilds the library,
  and the common launch header rebuilds many aggregate-test translation units.
  This is a broad rebuild, not the one-adapter focused mutation build.

The exact outputs are the task-owned mac-ckvision-shell, native release and
Linux ckvision-release/ckvision-asan/ckvision-tsan configurations. Their durable
records contain UTC start/end times, output paths and executable hashes.
No GUI application was launched or visually certified. This validation note
is appended after the frozen candidate gates; the isolated commit gate must
check its final bytes too, before a landing is claimed.

The first remote archive included generated Apple metadata sidecars. Every
source hash matched, but extra-file checks correctly stopped both remote
wrappers before compilation. A metadata-free archive (SHA256
`77e1c4293c637a270e1e806511163a1c862d504c33cdf9b0f4a91e9cbfb6ce0f`)
and removal of only the 4737 generated sidecars per host precede the successful
runs above. These failed transfer checks are retained, not called test gates.

Required evidence before landing:

- Native real-child command execution: a simple builtin, quoted text, Unicode,
  command operators, and a quoted executable path containing spaces/ampersand.
  Require expected output and actual exit status; error text is not success.
- Invalid UTF-8/NUL, length overflow and conflicting argv/command specs fail
  before child creation; preserve ordinary argv and PowerShell behavior.
- POSIX rejects the Windows-only form without spawning a child.
- Public launch-spec construction and reference documentation agree.
- Clean Mac/Linux/Windows builds, all suites, sanitizer/fuzz gates, benchmarks
  and filtered publication checks with exact source fingerprints.

The Windows default shell selection and native application integration are
adopter policy. No executable-name guessing, raw CreateProcess workaround,
temporary script wrapper or encoded-command substitution is introduced.
