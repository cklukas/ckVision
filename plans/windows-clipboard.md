# Native Windows clipboard export — D-122

The reusable `term::WindowsClipboardWriter` implements D-022/D-039;
`WindowsTerminal` delegates its default export to it. No platform clipboard
logic belongs in an application or widget. Each write converts/allocates first,
owns a message-only publication window, opens with that window, verifies
ownership after emptying, publishes immediate CF_UNICODETEXT, closes and
destroys the window. The system retains the exported Unicode data. No window
class registry, delayed rendering, native read, retry thread or message pump is
introduced. Invalid UTF-8, embedded NUL and clipboard contention do not replace
the existing content. A failure after emptying may leave the external clipboard
empty; Application's internal copy remains authoritative. The portable API is
still void/best-effort; typed failure reporting is separate unfinished work.

## Scope and provenance

Derived from the existing clipboard/service architecture and Microsoft's
OpenClipboard, EmptyClipboard, SetClipboardData, CreateWindowExW and clipboard
ownership documentation, linked in D-122. No outside implementation source was
consulted. On Windows 11 25H2 ARM64 build 26200, a compiled null-owner mutation
*without* ownership verification still exports successfully. Therefore this
work is a documented-ownership conformance fix, not a claim that data loss from
the previous backend was reproduced on that host. With the new postcondition
check retained, the compiled null-owner mutation fails three native Unicode
round-trip assertions, exit 1; restoration is hash-checked before rebuilding.

## Candidate verification, 2026-10-03

Run by ckmux-windows Codex, isolated at `84c9669598f8a4158fa652934e19989372f5013d`
plus this owned clipboard delta. Unrelated uncommitted widget work is excluded.
This is candidate-byte evidence; it does not certify a later commit or release.

- macOS Apple Clang, strict Release: 219/219, CTest exit 0, 20.90 s at `-j8`;
  benchmark smoke passes. Clean build 15:41:17–15:43:27 UTC, 130 s, 484 actions,
  Baseline: this repository's preceding 102.718 s/484-action clean gate.
  All expected translation units compiled; no compiler warnings or cache
  launcher. No-work rebuilds after Windows-only refinements were investigated:
  those native sources are deliberately excluded from the POSIX target.
- Debian 13 ARM64 GCC 13.3 Release/FORTIFY: 219/219, CTest exit 0, 27.63 s at
  `-j2`. The initial archive refresh changed unchanged sources' mtimes, causing a
  full 227 s rebuild. Later byte-verified overlays produced Ninja no-work builds,
  as expected for Windows-only implementation changes.
- Linux Clang 19 Debug ASan/UBSan plus fuzz corpora: 229/229, CTest exit 0,
  186.89 s at `-j2`. The final build was verified no-work because portable
  compiled inputs were unchanged; Windows sources are not compiled in this
  configuration. Benchmark smoke passes in both Linux configurations. No
  Windows ARM64 sanitizer result is claimed.
- Windows 11 25H2 ARM64, MSVC 2022 Release `/W4 /WX`: 206/206, CTest exit 0,
  109.03 s. The new isolated clipboard executable contains four cases: public
  Application/widget Unicode copy, invalid text/empty export, publication-owner
  release and adapter lifetime, and contention/recovery. Native modern ConPTY
  additionally passes 24/24, and benchmark smoke passes.

Windows source/asset manifest (4721 files) SHA-256:
`a7bf6f881e26c22023d06886dd2851f32b00e968179fc2f0af07b406d544fdbd`.
Restored native adapter SHA-256:
`9e45dfbb5fc00a9a9ca8d9645af570cde650e806522268f27a90e04e09f08911`.
Native clipboard test executable SHA-256:
`de574a79976608cb46306e7073ac4fa93613785e3dab48c65afb0ce18b1240e3`.

Windows initial archive-refreshed full build: 15:40:48.202–15:54:20.387 UTC,
812.180 s, versus the recent 880.171 s/371-object clean gate. This was not a
small incremental build; refreshed archive mtimes explain the full compile
actions. Corrected focused rebuild: 16.068 s; post-mutation full relink/rebuild:
16:04:11.668–16:04:46.600 UTC, 34.918 s. Exact current native output:
`C:/Users/klukas/ckmux-windows-20261003-01/release/tests/Release/windows_clipboard_contract.exe`.
Mac output: `/Volumes/PRO-BLADE/tmp/ckmux-windows-20261003-01/mac-ckvision-wp53`.
Linux outputs: `/home/klukas.guest/ckmux-windows-20261003-01/ckvision-release`
and the sibling `ckvision-asan` configuration.

## Verification instrument corrections

- The native clipboard fixture never touches the user's station. Unnamed
  CREATE_ONLY failed with Win32 183 under OpenSSH because that logon-session
  station already existed. A PID-named CREATE_ONLY station is fresh, and setup
  failure remains fatal. The test runner needs private-station creation rights;
  the library itself requires no administrator privilege.
- A newly focused InputLine already offers the entire text selected. Extending
  Home from that selection collapsed it. The event script now sends ordinary
  End then Shift+Home and positively asserts focus and nonempty selection before
  Ctrl+C. An incorrect `focused_view` spelling caused a compile failure; no
  stale executable was treated as passing evidence.
- Mac archive extended attributes generated AppleDouble sidecars, causing
  twelve Linux hygiene/golden checks to fail. Only those task-owned generated
  metadata files were removed. Archives now disable copyfile/xattrs/ACLs/flags;
  sources were fingerprinted and all gates repeated without weakening checks.
- A modern ConPTY driver invocation initially omitted RUNNER_TEMP. It stopped
  before tests; the corrected driver selects the task-owned temporary directory
  explicitly and all 24 native cases pass. No app was launched or visually
  certified by these headless/console checks.
