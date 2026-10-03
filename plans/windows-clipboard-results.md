<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Explicit clipboard export outcomes — D-124

`ClipboardWriter::write_text`, `Terminal::write_clipboard` and
`Application::set_clipboard_text` return `ClipboardWriteResult`. Native/memory
acceptance is `Ok`; an unacknowledged OSC 52 request is `Submitted`, not proof
that a remote clipboard changed. Refusals distinguish `Unsupported`,
`InvalidText`, `Unavailable` and `Error`. Native error numbers are retained
before cleanup, and `external_state_may_have_changed` describes failures after
the host may already have been modified. There is no platform retry or UI
workaround. Invalid UTF-8 and embedded NUL are rejected before native state is
touched. Native contention remains a synchronous environmental result.

Application always stores the internal text before exporting. Its optional
last-result value is empty initially, changes on each export, and does not
change on imported paste events. The injected host handler observes widget
copies as well as direct callers. It may replace/clear itself or export
reentrantly; the outer call returns its own result and the last-result field
retains the latest inner attempt. Callback exceptions terminate through the
terminal's existing restoration/failure boundary rather than escaping.

Record/replay includes every attempted copy, including refusals, and all
result fields. An unscripted replay cannot invent acceptance. Its remembered
clipboard text describes its last accepted request, not an external OS read.

## Candidate evidence

Source base `478c92ee` plus the owned API/test delta. These are candidate-byte
gates, not certification of a subsequent commit or release. No graphical app
was launched or visually certified.

- Apple Clang strict Release: 222/222, CTest exit 0, 41.59 s at parallel 2,
  benchmark gate exit 0. Initial clean build took 114 s; a public-header comment
  and test correction caused 341 incremental actions in 88 s. All expected
  translation units and zero compiler diagnostics were checked. The preceding
  same-project clean build took 102.718 s for 484 actions.
- Debian 13 ARM64 GCC 13.3 Release: 222/222, exit 0, 22.44 s at parallel 2.
- Clang 19 Debug ASan/UBSan plus fuzz corpora: 232/232, exit 0, 198.05 s
  at parallel 2.
- Native Windows 11 25H2 ARM64 MSVC strict Release: 210/210, exit 0,
  58.24 s at parallel 2, benchmark gate exit 0. Full source/asset manifest
  SHA256 `b2bf7241a4deb8e8df64039a3cd2eec0b982d5a528df4e44a146036e1c201e4c`;
  test executable SHA256
  `de236a5eac9a8f547c84e8541ddc5cf8f49564f03cd1111bccebe5a001fe6796`.
  Source refresh build: 20:22:17.447–20:35:02.737 UTC, 765.278 s. The preceding
  source-refresh native clean gate took 812.180 s; source hashes and actual
  compile actions establish which bytes were built, not elapsed time alone.
- GCC 13.3 Debug TSan: 221/221, exit 0, 361.01 s at parallel 2.

These initial API gates precede the 0.1.10 version bump, native diagnostic
assertions and extra replay mismatch/partial-result cases. Final release
candidate gates must include those bytes and the validation documentation.
Version-bearing reference dumps and the documentation SVG change only the
displayed version to 0.1.10. The first version-bump sweep passed the reference
dumps but found the SVG stale; its two version glyph cells were updated to
match the freshly generated figure byte for byte.

The first Mac sweep failed the new POSIX output-error fixture and the public
API comment gate. The fixture reversed PosixTerminal's output/input arguments,
closed its input instead of output and then killed the parent with SIGPIPE on
acknowledgement after its child failed. It now passes the named output sink in
the correct position and keeps acknowledgement alive for behavioral failures.
Documentation comments cover the new outcome declarations. Original failure
logs remain available; the successful retry is a separate record.

A compiled Application mutation calls the writer but replaces its returned
outcome with Ok. The public widget-copy and record/replay regressions reject
it, CTest exit 8. Strict baseline/restored focused gates pass 4/4, exit 0;
source restoration is hash-checked. This checks propagation of environmental
failure rather than merely that an observer was invoked.

The independent scratch-root assertion carried in this candidate is proven by
a compiled mutation: byte-identical candidate tests with a helper returning a
different, existing child root fail both expected-parent assertions, exit 1.
The unmutated helper passes both cases, exit 0. Both roots are within the
explicit task-owned workspace. This isolated helper check does not substitute
for the full platform gates and does not change historical evidence.
