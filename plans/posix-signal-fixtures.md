# POSIX signal fixture teardown — 2026-10-04

The 0.1.11 public snapshot `f609cbe7` failed macOS GitHub CI run
`37163490098`: 219/220 passed, and `suite_test_posix_terminal_1` terminated
with SIGALRM. The last announced case was the interrupted signal-safe write
test. Linux Release, fuzz, ASan, UBSan, TSan and Windows Release jobs passed.
The release is held; this failed run is not a green gate.

The fixture stopped its interval timer then restored the previous handler.
Disarming a timer does not discard a signal already pending. Restoring the
default action before that pending alarm is delivered can terminate the
suite. The revised fixture blocks delivery, disarms the timer, installs
SIG_IGN to discard pending alarms, restores the action, and restores the
original mask. No production terminal code or test timeout changes.

Authority: POSIX [signal actions](https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html)
requires SIG_IGN to discard pending signals, including blocked ones.
The exact internal scheduler history of the hosted failure is unavailable;
the failure mechanism is reproduced deterministically, not inferred as a
proven execution trace of that job.

A forked regression queues SIGALRM while blocked, verifies it is pending,
uses the same teardown as the write test, verifies it was discarded and that
the default handler and blocked mask were restored, then unblocks safely.
A compiled mutation removing only the discard operation fails the pending
assertion and terminates the child on SIGALRM. Mac Apple Clang strict Release
build exit 0 (two compile/link actions, 3 s); case exit 1. Mutation source
SHA256 `8a16a8d3d923551581289afce29f7f06a2ed2f59e8eb94a81f6fb8853720bfae`.
Restored source SHA256
`b2e9ecfa5183116220b7262f87d7ff6daa6c6768b66e2ff84fadd4bfa6fe3761`.
The original 8 MiB interrupted-byte-delivery assertion remains intact.

The first focused build failed before testing because sigemptyset,
sigaddset and sigismember are macros on Apple headers: a leading namespace
qualifier is invalid there. The corrected calls compile without suppression.
That failed compile is not reported as a test pass.

Full candidate gates by ckmux-windows Codex, frozen source/asset manifest
SHA256 `5f3efa6a1e6d8e836f8e36d547115a9862a01bd917e22020066d682180c58eed`:

- Mac Apple Clang strict Release 222/222, CTest exit 0, 38.64 s at parallel 2;
  benchmark gate exit 0. The restored test TU compiled/linked in 3 s; the
  final fingerprint repeat built with no work and the identical test hash
  `731267b889c7c41f36e8632b68b4a6c98c62881fd5e37944bfa900d29f7301fc`.
- Debian 13 ARM64 GCC 13.3 strict Release 222/222, exit 0, 24.39 s at parallel
  2; benchmark gate exit 0. Focused test TU rebuild 5 s.
- Linux Clang 19 Debug ASan/UBSan plus fuzz 232/232, exit 0, 136.46 s at
  parallel 2; focused test TU rebuild 12 s.
- Linux GCC 13.3 Debug TSan 221/221, exit 0, 262.04 s at parallel 2;
  focused test TU rebuild 7 s.
- Windows 11 25H2 ARM64 MSVC strict Release 210/210, exit 0, 38.88 s at
  parallel 2; benchmark gate exit 0. Build 3.902 s, with no compiled native
  input changes: the test executable's before/after SHA256 is
  `40fe8575ccc402cb10e115de434bb256a5aa761a6a81787e40f3ca385148f761`.
  Only the POSIX test and private notes changed in this delta.

The first Mac gate overlapped manifest generation, so its recorded manifest
hash named a partial file. The source was unchanged and its final audit was
complete, but that record is not used as the final freeze evidence. The repeat
above started only after the manifest/archive completed. The Windows wrapper
first stopped before building because its permitted private-note filename was
misspelled; correcting the exact owned path preceded the successful gate.
These failed/overlapped instruments are preserved, not quietly relabeled.

Exact task-owned outputs remain mac-ckvision-shell/tests/cvision_tests,
Linux ckvision-release/ckvision-asan/ckvision-tsan/tests/cvision_tests and
native release/tests/Release/cvision_tests.exe. Durable per-host records hold
UTC start/end times, configurations, manifest/executable hashes and output
paths. No GUI application or visual acceptance is claimed. This evidence-only
note follows the freeze; the final clean-commit gate must verify its bytes.
