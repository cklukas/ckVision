# Explicit native-test scratch roots — 2026-10-03

The test build selects `CKVISION_TEST_TEMP_ROOT`, passes it to native fixture
constructors and supplies TMPDIR/TMP/TEMP to CTest children. The configured
default is an explicitly supplied absolute TMPDIR or the build's tests/scratch
directory. Native filesystem/todo fixtures exclusively create unique children
of that root and remove only their own child; shell-based recursive deletion
of a fixed host path is gone. The explicit child-cwd regression uses its own
scratch directory. The black-box VT instrument puts both its script and its
reference socket beneath its selected root, rejects absent/relative roots,
and quotes script paths. This is test infrastructure, not an application
filesystem backend or a native Windows application claim.

An audit of actual constructors found prior hard-coded `/tmp` sites despite
the driver exporting TMPDIR. Prior functional pass counts did not establish
temporary-workspace-policy compliance. Fixture regressions check the selected
root, exclusive child allocation/cleanup, rejection of escaping prefixes and
reference script/socket selection (including spaces/apostrophes). The latter
tests mock the reference process; the ordinary conformance test still drives
its documented black-box interface. No outside implementation was consulted.

Run by ckmux-windows Codex in the owned detached candidate on `987e0b68`:

- Apple Clang strict Release: 221/221, CTest exit 0, 16.90 s at parallel 8;
  focused scratch/filesystem/todo/subsession cases 5/5, 5.25 s. Benchmark smoke
  passes. Compile-definition change rebuilt 166 test objects plus link, not a
  full 484-action clean build. Configure-log creation through last build-log
  write: 16:57:45–16:58:39 UTC, 54 s (second-resolution filesystem record).
  Final no-work verification 17:00:16–17:00:17 UTC was investigated against
  Ninja actions and the changed input inventory, not treated as a clean build.
- Debian 13 ARM64 GCC 13.3 strict Release/FORTIFY: 221/221, exit 0, 21.91 s
  at parallel 2 after the final clipboard-note wording was included. Rebuild
  17:02:06–17:03:26 UTC, 80 s; aggregate test inputs recompiled.
- Linux Clang 19 Debug ASan/UBSan plus fuzz: 231/231, exit 0, 133.01 s at
  parallel 2. Rebuild 17:03:52–17:05:06 UTC, 74 s; aggregate test inputs
  recompiled. Benchmark smoke passes in both Linux configurations.
- Windows 11 25H2 ARM64 MSVC 2022 strict Release `/W4 /WX`: 208/208,
  exit 0, 42.26 s at parallel 2. Build 17:02:09.634–17:08:25.681 UTC,
  376.024 s; the aggregate portable-test compile definition rebuilt its
  translation units rather than the whole library/example graph. Benchmark
  smoke passes. No Windows ARM64 sanitizer result is claimed.

No comparable all-test-definition incremental duration baseline was recorded
before this change. The documented clean baseline is 484 actions/102.718 s
on Mac and 371 objects/880.171 s on native Windows; the smaller action scopes
above explain their shorter times. No build cache launcher was introduced.

Exact outputs:

- `/Volumes/PRO-BLADE/tmp/ckmux-windows-20261003-01/mac-ckvision-wp53/tests/cvision_tests`
  SHA256 `cc5f8944353ddfaf457f99bd5bb5718897b1df0e66c2406d19cf3f71a87d32ac`;
- `/home/klukas.guest/ckmux-windows-20261003-01/ckvision-release/tests/cvision_tests`
  SHA256 `c0be8ca5d6f1687378fb2057b7e81e0e70313c58b99a526c43142903e0fb1d66`;
- `/home/klukas.guest/ckmux-windows-20261003-01/ckvision-asan/tests/cvision_tests`
  SHA256 `8abb661dcfeeb6d65fc969deeb4566cc5aae17239cc68ea8592d25540c040c52`;
- `C:/Users/klukas/ckmux-windows-20261003-01/release/tests/Release/cvision_tests.exe`
  SHA256 `462b5bf88b7f19d13bb1fc413136e804230cf997421901e413e4ba0acd5e855a`.

The initial Mac full sweep found the already-landed private clipboard note's
duration-baseline wording matched the provenance guard. The note was written
after its earlier candidate gate, so that gate did not cover those later
documentation bytes. Wording now describes the factual baseline unambiguously;
the guard is unchanged, and the final full Mac/Linux gates include that note.
Driver interruption stopped the first Mac application sweep and Linux driver
after its library suites. Exact process/PID checks established that those
drivers were gone before resuming; complete logs, not truncated runs, supply
the counts above. This document records candidate bytes, not a future commit,
release, or user-facing visual acceptance.
