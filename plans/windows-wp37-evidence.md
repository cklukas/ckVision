# WP-37 Windows outer terminal implementation evidence — 2026-09-24

## Scope

The named ARM64 Windows 11 25H2 Lima VM on the PRO-BLADE volume now builds
WindowsClock and WindowsTerminal into the public cvision library. The backend
requires VT-capable console standard handles, saves and restores console modes
and UTF-8 code pages, writes VT frames, feeds VT input through InputDecoder,
uses console input records for resize notification, supplies native wait/wake
handles, and exports text with CF_UNICODETEXT. The gallery, editor, and embedded
terminal examples now build as native ARM64 Windows executables. The terminal
example selects COMSPEC and USERPROFILE and has a PowerShell Sixel demo child.

This is an implementation and transport checkpoint, not WP-37 acceptance.
Microsoft's documented VT input mode, VT output mode, console input records,
and waitable console handles are the Win32 behavior used here:
https://learn.microsoft.com/en-us/windows/console/getconsolemode
https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
https://learn.microsoft.com/en-us/windows/console/low-level-console-input-functions
https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects

## Native build freshness

The only active source/build pair is
C:\Users\klukas\ckvision-wp50-20260924a\src and
C:\Users\klukas\ckvision-wp50-20260924a\build on the VM's external-drive disk.
No ckVision example instance was running before or after the validation.
The native Visual Studio 2022 ARM64 Release build uses strict /W4 /WX settings.
The first example build started at 08:49:34.36 VM-local time and finished at
08:50:58.55, 84.19 seconds. It compiled the newly enabled example object
graphs and all three entry points. The final incremental build started at
08:54:08.83 and finished at 08:54:34.50, 25.67 seconds, recompiling
windows_terminal.cpp and terminal_app.cpp and relinking all three examples.
The shorter run is explained by reuse of the unchanged object graphs. The
compiler log lists the recompiled inputs and all resulting targets.

The first verified guest source SHA-256 matched the Mac worktree for the ten changed
build/source/header inputs inspected. In particular, windows_terminal.cpp is
083A6F255D1AF63C23520E54D78FC0F7473CA275EACD96E95F20F3996F7D9A7C,
and terminal_app.cpp is
C1E7180663F692FAA47F3C81152E4CDAA0D2E2D34BF2630075D7D71598645A5E.
The first verified cvision.lib was written at 07:54:21.164 UTC and has SHA-256
2E245F17B96DF82110EC9B57B770F8C7CB3948572B59725FA2AA33464325F751.
The first verified example executables were written after that library and after their
source inputs:

- gallery, 07:54:22.148 UTC, SHA-256 501A7F09A21E0BB7C5076FDDC89315F4ADE6CB6197F274022155F7F3542F1621
- editor, 07:54:26.099 UTC, SHA-256 6B9A8235B173680DE521AD08FC9AC0F5A85F66B88D24CEFB27A0D4EA1555F55B
- terminal, 07:54:34.285 UTC, SHA-256 B161DDDBDE734731A91783647968E12D4F8C1C8B6FF820179A66A287BE7921D0

The earlier 2.3-second attempt failed because the previous VM CMake cache had
examples disabled. It produced no executable and was corrected by
reconfiguring the same build with CKVISION_BUILD_EXAMPLES=ON.

## Runtime and gates

A pre-final-source build of the gallery ran in a live SSH-created Windows
console. The rendered desktop and controls were visible in its VT output;
Alt+X exited cleanly and emitted terminal restoration bytes. A pre-final-source
build of the contained-terminal example opened a real cmd.exe under ConPTY.
The entered command printed CKVISION_WP50_INPUT_OK inside the terminal view.
The child exited and the view displayed its exited state. The parent was then
terminated by its exact PID to clean up the smoke session; this is not a
graceful-close restoration assertion. The final source edit only removed
allocation from WindowsTerminal::restore and changed example comments;
the final binaries have been rebuilt and fingerprinted, but not run again.

The native Release CTest conpty label passed (one suite containing nine
adapter cases). The Mac layer checker passed over 220 files. The mandatory
provenance check and checker self-test passed. Documentation links and
screenshots passed, and git diff --check is clean.

## Remaining acceptance

WP-50 still needs a real Windows Terminal GUI run of the contained terminal,
nested ckVision, live Sixel containment and fallback, resize, error/close,
resource recovery, and simultaneous sessions. WP-37 still needs GUI keyboard,
cell/pixel mouse, resize, Sixel/fallback, native clipboard import/export,
abnormal-exit restoration, multi-Application isolation, legacy-console
refusal, editor workflow, and the full warnings-as-errors, golden, sanitizer,
benchmark, and script matrix across macOS, Linux, and Windows. At this
checkpoint, enhanced keyboard negotiation was not yet present in
WindowsTerminal; the later checkpoint below records its implementation. WP-38 remains
blocked on that evidence.

## Final follow-up on this checkpoint

After the initial build, the Windows clock was moved to the documented
high-resolution QueryPerformanceCounter source (with GetTickCount64 only as a
failure fallback), an empty native clipboard export was handled, and Windows
example entry points gained a visible exception diagnostic. The final native
Release build started at 09:07:58.08 VM-local time and finished at
09:08:23.70, 25.62 seconds. Its log names windows_clock.cpp,
windows_terminal.cpp, and the three example main.cpp files as recompiled
inputs. The 113-second intermediate entry-point-only rebuild at 09:03 was
investigated: the log showed exactly those three changed entry points and
three sequential MSBuild link steps, with no unexpected source or cache use.
The final 25.62-second build and source/output hashes below establish the
current runnable output rather than relying on either duration.

Final source SHA-256:
- windows_clock.hpp AD18DD81C0B13DBC9BA8B44F18F8714633FD3C18527BAC475BFFD7107C9AE368
- windows_clock.cpp D9FAC157602222F4A7CB276EDBA9D88FEF85C6D6A492D423478A7297D52F1E74
- windows_terminal.cpp D372F297362B1B14C76DAE3CBDFE657AEE2F69BD6C4834DC16AB1EC17C671F04
- terminal/main.cpp B61B981432862DA9856714766B5EF0205143BD9259E25D01016EE777FB1D56CA

The guest copies of all inspected changed inputs match the current worktree.
The final library was written at 08:08:07.260 UTC, SHA-256
D7C81AC2C1596E2817B915285651D1E4FD3968B7FE0B46494A642AF663C68CEE.
The final example executable timestamps and SHA-256 values are:
- gallery 08:08:11.063 UTC AF389C76F45E4A253BC2B4836A21748E71F2A05E0A2B8F42C24BC0299327CFEB
- editor 08:08:17.262 UTC 66203F482CD390617CF805FC540ED8CFDE3F4F82EAD345FDF53826060D8C67D1
- terminal 08:08:23.544 UTC 3528BD8CC41F3D80CAE15D13379EB43244D83566142316496A5A401869E812AF

The final gallery executable rendered its desktop in the SSH-created VT
console and exited on Alt+X with restoration bytes. Running the same final
executable through non-PTY SSH returned exit code 1 and the diagnostic
"Windows terminal: stdin and stdout must be attached to a VT-capable console".
No example instance remained afterward. The final native ConPTY CTest label
passed again (one suite, nine internal cases, 1.66 seconds).

The Mac AppleClang Release build was configured clean beneath
/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-build, the only ckVision Mac
build directory found in the external temporary workspace. It started at
08:01:05 UTC and finished at 08:01:29 UTC, 24 seconds, compiling the whole
library and all three example object graphs and entry points without warnings.
There was no recent comparable clean baseline in this workspace; the full
compile log and fresh output timestamps explain the duration. After the
entry-point diagnostic edits, an incremental build ran 08:03:35–08:03:39 UTC,
recompiling and relinking those three mains. The current Mac example outputs
were written at 08:03 UTC after their source changes. Mac static gates and
git diff --check remained green.

## Restoration follow-up and GUI availability

A final review found that orderly restoration and post-restore diagnostics
used a single Win32 write, which could truncate on a short write. They now
use an allocation-free, bounded-chunk write loop that continues through
partial writes and stops on a real error. The source SHA-256 is
BC7F78FF5F4C6E182DF701D073D483FC25489CB4C8EED5A94E94127554868488.
The native strict Release rebuild started at 09:14:33.35 and finished at
09:14:50.79 VM-local time, 17.44 seconds. Its log recompiles exactly
windows_terminal.cpp and relinks the library and three examples. The final
cvision.lib was written at 08:14:42.562 UTC (SHA-256
B6062C64D4753B1E2B3FED5F9D389FFD8044F99FBE9C6A00C920D7AEAEA263C1);
the gallery, editor, and terminal executables followed at 08:14:43.496,
08:14:46.838, and 08:14:50.524 UTC, respectively. Their SHA-256 values are
A829DDB1D5F20C20FE3359BD1A544925A168CCB04206444D87C29EC9D83FE7EC,
14E3EC2A103242E43EB43BC51060FCDF82A2EA7857CA7931263C0879E53AEB3F,
and 7E7BC58D50A300FB0138C85806A087F2769C35D6C8A7200BF299A25BC2BB61EC.
The final gallery binary rendered in the SSH-created VT console and exited
on Alt+X with restoration bytes. The native ConPTY CTest suite passed again
(1.58 seconds); layer and provenance gates, including the provenance
self-test, remained green.

The QEMU display was inspected through the VM's QMP screendump after waking
the screen: it shows the Windows lock screen, so graphical display is
available. No Windows desktop user session is currently signed in. A real
Windows Terminal GUI acceptance run has therefore not yet been performed.

## Hosted Release matrix checkpoint

The full warnings-as-errors Release build and hosted CTest suites are now green
on all three named operating systems. This validates the available headless,
script, golden, benchmark-smoke, package, and platform-smoke entries. It does
not establish interactive Windows Terminal behavior, sanitizer/fuzz coverage,
or byte-for-byte cross-platform golden equality beyond the checked-in golden
oracles exercised by each host.

| Host and compiler | Build | Hosted CTest |
|---|---|---|
| macOS, AppleClang, external PRO-BLADE build | 08:39:39–08:40:55 UTC, 76 s; all targets, zero warnings | 176/176, 24.72 s |
| Linux ARM64 VM, GCC 14.2, isolated `/tmp/ckvision-wp37.TpubvQ/build` | 08:44:24–08:48:53 UTC, 269 s; clean all-target build, zero warnings | 176/176, 9.96 s; 08:49:12–08:49:22 UTC |
| Windows 11 25H2 ARM64 VM, MSVC 2022 `/W4 /WX` | 09:19:55–09:28:37 VM-local, about 522 s; clean all-target build, zero warnings | 163/163, 94.08 s; final run 09:37:21–09:38:55 VM-local |

The clean Linux build has no comparable recent baseline. Its log compiled the
library, all examples, tools, benchmarks, and the complete test runner before
linking; the 269-second elapsed time is consistent with that complete input
list. The Windows full build has no comparable all-target baseline either; its
log similarly names the full target graph. The final Windows incremental build
ran 09:36:29–09:37:01 VM-local (32.42 s): CMake regenerated because
`tests/CMakeLists.txt` changed, then MSBuild compiled the corrected spin smoke
source and relinked the runner. The source/output timestamps and SHA-256 values
below establish freshness; the shorter duration reflects unchanged object
reuse. A prior 159/163 Windows run exposed four host issues: the spin smoke
advanced beyond its own deadline under load, package smoke assumed a suffixless
single-config executable, provenance scanning assumed Git existed in the source
copy, and test inventory assumed `ctest` was on PATH. Each was corrected in its
own test/tool integration and all four passed in the final 163/163 run.

Selected current source SHA-256 values, matched on the guest where applicable:

- `src/term/windows_terminal.cpp`: BC7F78FF5F4C6E182DF701D073D483FC25489CB4C8EED5A94E94127554868488
- `src/term/windows_terminal_subsession.cpp`: F455EC9D0DAA774A8C67F773BB7752BF1EDAA9184EEA17E9C2ABFFE99D2E2589
- `examples/sysinfo/build_report.cpp`: B357E599872874291CE3C5F6DA3A22DBA6B3CC856B9F49E19499E1733C66C07C
- `tests/test_spin_smoke.cpp`: 530467DB435A5B0EFB1976D33A67A90F77AD1AFEB017C89236CC70C4569A8306
- `tools/check_provenance.py`: 821381D9588309EAB95552EEA793A0B2B5F3FB7AEC875DCED965681B9884B6D6
- `tools/report_test_inventory.py`: 880A8A85BCB81ADF24A865B181E7B0071DF18134F472A4E594B60ABF9D4649B7

Selected output freshness and SHA-256:

- macOS: `/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-build/tests/cvision_tests`, 08:40:55 UTC, 32FD6051DA62345EA388D8C2B899D07DD312C0B6C3B43A9BC79A36FFE294A034; the gallery output is CA7F34C54D19CEA11CC9B2642FBA46CF868D1913BB8257E78F3209334DF61F7F.
- Linux: `/tmp/ckvision-wp37.TpubvQ/build/tests/cvision_tests`, 08:48:53 UTC, 314B6688D72F76D9ED119FC34DF7457F390FA6ABCE1E6B8AFAE8340983931B83; `libcvision.a` is DB15B3A9D36C80CD7EB9B7D641DDA7FDFFAA39513B56E7E984046FE0109F6394.
- Windows: `C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe`, 08:37:00 UTC, 4FE2055E575D54500E1E878BDA638E49B834101DA7411A10CD8E5B27B8AA01EC. The library and three executable fingerprints appear in the restoration checkpoint above.

No graphics or other checked-in assets changed in this checkpoint. The Mac and
Linux examples were built but not launched for a new visual claim. The only
Windows runtime claims remain the exact-binary SSH VT smokes above; no GUI
acceptance was attempted. The Windows display is at the lock screen and sign-in
requires explicit user authorization. The Linux build was removed after this evidence and the complete test log were
retained under the external task workspace. The current Mac and Windows
verified builds remain available for unfinished interactive and sanitizer work.

## Linux sanitizer matrix follow-up

A full Linux ARM64 Debug build with GCC 14.2 and
`CKVISION_SANITIZE=address,undefined` completed 08:59:46–09:03:41 UTC
(235 seconds) with zero compiler warnings or errors. The output path was
`/var/tmp/ckvision-wp37-asan-V7k2/build`. The instrumented runner was
written at 09:03:41 UTC, after the unchanged source inputs, and has SHA-256
43DD748E0A37944C87B5B57BAEBBC4FD93859C694E17A6CC76D7E7DC6F2BBB1E;
the instrumented library has SHA-256
1BD39618C6209ACFD6D06053E584899DABA152EA82E9DAA6B0F9FE499F110A66.
No comparable recent clean sanitizer baseline exists. The complete build log
names all library, example, tool, benchmark, and test inputs, and a subsequent
no-change rebuild succeeded, confirming the initial wrapper's nonzero status
was not a failed build.

The first attempt under the Linux guest's 2.9 GiB `/tmp` tmpfs exhausted that
filesystem while linking examples at 59%; its partial build was deleted after
saving the failure log. The verified build used the guest's 99 GiB disk-backed
`/var/tmp`, on the VM disk stored on PRO-BLADE. The full CTest run passed
176/176 at 09:04:34–09:06:18 UTC (103.73 seconds) with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Full build and test logs
are retained under the external task workspace. This closes one Linux
sanitizer row; ThreadSanitizer, macOS/Windows supported sanitizer rows, fuzz
corpora, and GUI acceptance remain open.

The Release CI workflow now runs a complete hosted CTest pass in addition to
its named label gates, so unlabelled entries cannot silently disappear from
Release coverage. The edited YAML parses successfully.

## Linux ThreadSanitizer follow-up

The Linux ARM64 GCC 14.2 Debug `CKVISION_SANITIZE=thread` build completed
09:08:24–09:15:22 UTC (418 seconds) under
`/var/tmp/ckvision-wp37-tsan-P4r8/build`, with zero compiler warnings or
errors. The full compile log includes library, examples, tools, benchmarks,
and test runner. There is no recent comparable clean TSan baseline; the
longer time than ASan/UBSan reflects the different instrumentation and the
complete recompile, not a stale output. A no-change rebuild returned success.
The TSan runner was written at 09:15:22 UTC and has SHA-256
1E161C1660588C06CCC6E4C0AD56FEF6435425C2A86B85EBD60E4849A5CECDA8;
its library has SHA-256
52D282788C22C23A3A2668B00B9D93571129530E5C599B6625079DABA242EE16.

The full TSan CTest run passed 175/175 at 09:15:54–09:22:39 UTC
(404.72 seconds) with `TSAN_OPTIONS=halt_on_error=1`. Its count is one below
Release/ASan because `test_hot_path_allocations.cpp` replaces global
allocation functions, which conflict with TSan's allocator interception;
`tests/CMakeLists.txt` declares that exclusion. The deterministic scene and
TODO benchmark gates still ran; the scene gate took 303.09 seconds under
instrumentation, inside its documented 600-second sanitizer timeout. The
complete build and test logs are retained under the external task workspace.
The TSan build was removed after this evidence and its complete logs were retained.

## macOS sanitizer follow-up

A separate external-drive AppleClang 21.0 Debug build with
`CKVISION_SANITIZE=address,undefined` completed 09:24:26–09:26:08 UTC
(102 seconds) with zero warnings under
`/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-asan-build`. There is no
recent comparable clean Mac sanitizer baseline; the complete log shows the
library, examples, tools, benchmarks, and test runner compiled before the
final link. The runner was written at 09:26:08 UTC after the source inputs,
with SHA-256 E82BD25F77A3B5A9352209F1C3955C9D6D7F25762627EC341A01913B714D65CE;
the library has SHA-256
8C7833971E6785DDE48BCEDC6D65BE355270D4873217973CB34FD560D2277BAE.

The complete macOS ASan/UBSan CTest suite passed 176/176 at
09:26:22–09:27:50 UTC (87.59 seconds), including the PTY, golden, package,
provenance, and deterministic benchmark budget entries. It used
`ASAN_OPTIONS=halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. The instrumented build was removed after its logs and hashes were retained;
the verified Release build remains available. Windows sanitizer support and fuzz corpora are still
unverified, and interactive GUI acceptance remains open.

## Current-source Release refresh and Debug diagnostics

A strict ARM64 Windows Debug build exposed two MSVC C4702 diagnostics in
`Application::set_focus` and the SysInfo language report. The redundant abort
was removed and the language selection now uses compile-time preprocessor
branches. Its first full hosted Debug run found four failures: the hot-path
allocation assertions (MSVC Debug library allocations), a narrow-resize crash
in `layouts_visual_capture`, and timeouts in the scene and TODO benchmark
gates under unoptimized Debug. The resize crash was symbolized to
`compute_dock_layout`: `AnchorPane` intentionally retains a signed extent for
later growth, while Dock's visible layout had passed that negative width to
`std::clamp` as an upper bound. Dock now normalizes its visible area to zero on
empty axes. Pure and integrated shrink/recovery tests were added; the focused
Debug Dock and layouts capture gates pass, including the five layout states.
The full Debug suite has **not** been rerun or accepted as green. Its first run
and focused rebuild logs are retained in the external task workspace.

The installed ARM64 MSVC toolchain warns D9002 and ignores
`/fsanitize=address`. CMake now checks that flag at configure time and
rejects unsupported MSVC targets instead of producing an uninstrumented build
labelled ASan. The rejected configure probe is retained in the external task
workspace. This is a toolchain limit, not Windows sanitizer pass evidence.

After these source changes, the exact current-source strict Release outputs
were refreshed on all three operating systems:

| Host | Current-source all-target build | Complete hosted CTest |
|---|---|---|
| Windows 11 ARM64, MSVC 2022 `/W4 /WX` | 10:03:51–10:04:19 UTC, 28 s incremental; CMake regenerated and changed objects recompiled | 163/163, 10:04:51–10:05:50 UTC, 58.19 s |
| macOS, AppleClang 21 | 10:12:44–10:12:52 UTC, 8 s incremental; changed objects recompiled | 176/176, 10:13:09–10:13:23 UTC, 13.31 s |
| Linux ARM64, GCC 14.2 | 10:14:42–10:17:15 UTC, 153 s clean all-target build | 176/176, 10:17:32–10:17:42 UTC, 9.78 s |

The Windows and Mac incremental durations follow their previously verified
builds; logs show CMake regeneration and the changed inputs, rather than a
skipped build. The Linux duration is consistent with its earlier 269-second
clean Release build; the log includes 321 C++ compile inputs and all target
links. All three builds have zero compiler warnings. The Windows guest source
hashes for CMake, Application, Dock, SysInfo, Dock tests, and the restored
layout fixture match the Mac worktree. Selected final output hashes are:

- Windows `cvision.lib` C6500A591E9A47A556AFED29ACC6E58BFFB7B4B82BE877B75AE26AB138F61A31, written 10:04:06 UTC; test runner 6E4A7B243980AD708AA92A622678AF57B47D88ED127872A6BC67F54E4FE1BA9B, written 10:04:19 UTC; gallery 8291B78700443E4BD4237CA1EAD94185D50FB2F364B0AA89C91E8BC95A0A7AE1, written 10:04:14 UTC.
- macOS `libcvision.a` 2DA379115A6C28AE68BD3426F36766BB9C682379DF8308E1D2D6C10AC5B26B4F, written 10:12:48 UTC; test runner 1A6847DFBCA479F6EA115EBB47463435056A8582BD970691C71C245EAB6F26FA, written 10:12:52 UTC.
- Linux `libcvision.a` 95A5FEB67BF8D68D2687911121DD249FA879A52589C0519067DFA25727B8590F, written 10:15:18 UTC; test runner 927443B65EF3C10C93FB5A96AD329E79145F1578AFAB745B0A77363ABC76E81B, written 10:17:15 UTC.

The previous full Linux GCC ASan/UBSan and TSan and Mac ASan/UBSan passes
preceded the Dock repair. They establish the matrix at that source checkpoint;
a current-source instrumented run is recorded separately below when complete.
The superseded Linux Release workspace was removed after preserving its logs
and fingerprints. The current Mac and Windows Release builds remain available.

## Current-source Linux fuzz and sanitizer gate

A clean Linux ARM64 LLVM Clang 19.1.7 Debug build with
`CKVISION_SANITIZE=address,undefined` and `CKVISION_BUILD_FUZZERS=ON`
completed 10:21:58–10:26:02 UTC (244 seconds) with zero compiler warnings.
Its disk-backed VM workspace was
`/var/tmp/ckvision-wp37-fuzz-A6q1/build`; the VM disk is on PRO-BLADE. The
full all-target log names the library, examples, tools, benchmark, test runner,
and nine libFuzzer executables. This duration is comparable with the earlier
235-second GCC ASan/UBSan clean build, and output timestamps and hashes
establish that the build contains the current Dock repair. The runner was
written 10:26:02 UTC, SHA-256
716DE3FF12B4DA7F294450F9113F633343A8FFDCED063685BD017AD11810847A;
the library was written 10:23:27 UTC, SHA-256
2F02B66580FD8178A48262216E3830C8709485D20B47945502A502DEFF42A20B.
The current Dock and Dock-test source hashes were
D839F8A9406912A6AB9CDB06871E6B72CB1FEC898275B87E51FD3B12170F7061
and F46FB524D9A408F0AF4490896BB60DE655D11C13BF2AA227A7CB23BF7E565A34.

The complete hosted CTest suite passed **185/185** at
10:26:25–10:29:17 UTC (172.20 seconds), including all nine `fuzz-corpus`
entries, golden/script/PTY/package/provenance gates, and the scene and TODO
benchmark gates. It used leak-detecting ASan and halt-on-error UBSan. The
complete logs are saved under the external task workspace; the superseded
Linux build was removed afterward. This is corpus execution, not an unbounded
fuzzing campaign. Interactive Windows Terminal and cross-platform byte
equality remain separate acceptance rows.

## Current-source macOS sanitizer refresh

A clean AppleClang 21 Debug all-target build with
`CKVISION_SANITIZE=address,undefined` completed 10:30:16–10:31:38 UTC
(82 seconds), zero warnings, under the external task workspace. The source
Dock and Dock-test inputs were written at 10:00:52 UTC, preceding the library
at 10:30:37 UTC (SHA-256
5527F84FEAA7198AC489EFD19C4FC9F95D571EBC004BC096566A8A2B3236270A)
and the test runner at 10:31:38 UTC (SHA-256
40447075C8FD5F94D67C8163FD53B72A92191E935BD803EA3F504C438D2C4319).
The 82-second build is comparable with the preceding 102-second clean Mac
sanitizer build; the log names the full input graph.

The full CTest suite passed **176/176** at 10:31:54–10:34:30 UTC
(156.18 seconds), including provenance, goldens, PTY, package, scene budget,
and TODO benchmark. `ASAN_OPTIONS=halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` were set. This is longer
than the earlier 87.59-second run: the Linux TSan build was compiling
concurrently, and the detailed log shows slower platform/client and scene
budget entries, all within their test timeouts. The complete logs and hashes
were retained; the instrumented build was removed afterward.

## Current-source Linux ThreadSanitizer refresh

A clean Linux ARM64 GCC 14.2 Debug `CKVISION_SANITIZE=thread` all-target
build completed 10:31:34–10:35:16 UTC (222 seconds), with zero warnings under
`/var/tmp/ckvision-wp37-tsan-current-J5m9/build`. Its runner was written
10:35:16 UTC, SHA-256
6E4657E6DB456211F01338925BBE6A03F18C9F9811473BBCB7F6CA739CE5F671
and its library at 10:32:23 UTC, SHA-256
37724AF90653230C3A1C882701EC668D95262E84221B944CF4D33784B71D809A.
The source Dock repair and tests were already in place. This clean build was shorter than the earlier
418-second TSan build. Both logs show the same 320 C++ object compiles and
59 link steps, and this build's output timestamps and hashes are fresh; the
elapsed time alone was not used as freshness proof.

The full hosted TSan CTest suite passed **175/175** at
10:35:47–10:42:17 UTC (390.13 seconds). The one excluded allocation-hook
suite is the declared TSan allocator interception exception. The TODO
benchmark passed in 115.08 seconds and the scene budget in 270.26 seconds,
inside its 600-second TSan timeout. Provenance, golden, script, PTY, and
package gates passed. The complete logs were retained on PRO-BLADE; the
instrumented build was removed afterward. At this source checkpoint, the
supported Mac/Linux ASan/UBSan, Linux TSan, and nine Linux fuzz-corpus gates
are green. ARM64 MSVC ASan remains unsupported by the installed toolchain.

## Final static review checkpoint

After the evidence and queue-status edits, the current macOS Release runner
passed all seven documentation gates, the layer checker, `provenance_check`,
and `provenance_checker_self_test` (10/10 selected CTest entries at
10:44:03 UTC). The edited CI YAML parses and `git diff --check` is clean.
The Dock repair derives from ckVision's signed AnchorPane geometry and the
standard `std::clamp` bound precondition; the Windows transport design and
behavior are grounded in the project's D-073/D-020 decisions and the
published Microsoft console/VT APIs cited above. No prior implementation's
source was consulted for the re-derived behavior.

M7, WP-50, and WP-37 remain open for live Windows Terminal interaction,
nested-session containment, capability/degradation evidence, and complete
appearance/golden equality. WP-38 remains blocked on their acceptance. The
Windows VM display is at its lock screen; no graphical user session has been
authorized or entered for these tests.

## Bounded outer-host probes and current Windows Release checkpoint

The Windows outer backend now sends the published kitty keyboard query during
its 250 ms probe interval. A verified reply leads to a session-owned flag push
and readback; restore pops that stack entry. The backend falls back to legacy
keyboard input when the host is silent or proves that the requested associated
text is unavailable. Its probe deadline also bounds `poll(INFINITE)` when the
application has no other deadline. On resize it temporarily withholds prior
pixel/graphics capabilities, re-probes, and restores a previously verified
Sixel capability after a silent resize. Bracketed paste imports as a paste
event. These behaviors follow D-020 and the published
[kitty keyboard protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/).

Four new cases exercise the actual Windows outer backend inside private ConPTY
hosts: verified kitty negotiation and console restoration; silent-host probe
expiry and legacy fallback; bracketed paste import; and resize with Sixel
capability withholding and restoration. The focused `conpty` suite now runs
13 internal cases and passed 13/13 at 11:04:31–11:04:44 UTC. This is protocol
and transport evidence, not an interactive Windows Terminal GUI run.

The strict ARM64 MSVC Release build began 11:03:47.565 UTC and finished
11:04:15.138 UTC, 27.57 seconds, with zero warnings. The changed outer
backend, fixture, and test objects were recompiled; the duration matches
recent 28–40 second incremental builds. The exact build is
`C:\Users\klukas\ckvision-wp50-20260924a\build`. Source and output ordering
and SHA-256 fingerprints establish freshness:

| Artifact | UTC write time | SHA-256 |
|---|---|---|
| `src/term/windows_terminal.cpp` | 11:02:20 | `18F998136D45CB44F6C0ED395646A3BEA3105A73BCE4790464FA48F40729CFCC` |
| `include/cvision/term/windows_terminal.hpp` | 10:53:01 | `F2236159B98994002C71E4A9F1EF8F8D8577E588AFF080242D1482D818CD4D8A` |
| `tests/windows_terminal_child.cpp` | 11:03:11 | `DBB31D672EB0F150A6E0089A893A6BA8AD378A2F611E9D16D9079E2FF7439B18` |
| `tests/test_windows_terminal_subsession.cpp` | 11:03:11 | `A2A10333F2069BC74D041111DF93BE2ABC427CAFC56EEDFC0384427E5B31CBC4` |
| `Release/cvision.lib` | 11:03:58 | `178FC7BE7969D19B69F44E7FB16FE808D3876237EF4FD165E6F66D3B38B42AE4` |
| `tests/Release/windows_terminal_child.exe` | 11:04:03 | `6C5236E1C31E5294A79C7468D353EA7E6E590B8F9DA4B021739F96EBF745650E` |
| `tests/Release/cvision_tests.exe` | 11:04:14 | `D1FB2963210152F18F5B9483A5C261AFBD5F3C7B605184E5F72F45B976FCA2F1` |

The final hosted Windows Release CTest run passed **163/163** from
11:05:15.240–11:06:40.625 UTC, including layer and both mandatory provenance
gates. A preceding run exposed five AppleDouble `._*` metadata files from
macOS archive transfer; they caused the layer/provenance text scanners to
fail on binary metadata. Only those five guest metadata files were removed,
and later transfers disable macOS metadata with `COPYFILE_DISABLE=1`; the
guest source tree was checked for invalid UTF-8 before the final run. The
final macOS Release runner also passed the ten selected documentation,
layer, and provenance CTest gates after the code and documentation edits.
Logs are retained under `/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn`.

Windows Terminal Stable 1.24.11911.0 remains installed. Microsoft's
[1.25 Preview announcement](https://github.com/microsoft/terminal/discussions/19947)
reports kitty keyboard support in Preview. `winget` verified the official
1.25.1912.0 installer hash but package registration failed with Windows error
`0x80073d19` because the VM user was logged off. `Get-AppxPackage` returned
no registered Preview package. The official MSIX bundle is staged on the
external-drive VM disk at
`C:\Users\klukas\ckvision-wp50-20260924a\packages\Microsoft.WindowsTerminalPreview_1.25.1912.0_8wekyb3d8bbwe.msixbundle`
(24,029,967 bytes; SHA-256
`7C866975C4CE6B191F02FC0460ED3E31BDE53AA417716992B62164B4B1F13C4E`).
Registration and live keyboard/graphics acceptance await an authorized
interactive Windows desktop session. WP-37 and WP-50 remain in progress;
WP-38 remains blocked on their remaining acceptance rows.

## Redirected-handle refusal checkpoint

A Windows-only regression now constructs the real outer terminal with
redirected anonymous-pipe handles. It verifies the actionable
`stdin and stdout must be attached to a VT-capable console` diagnostic and
that the attempted construction leaves the process console code pages
unchanged. This covers the non-console refusal path; it does not claim a
live legacy-console-host acceptance run.

Before the rebuild, the named VM had one project source tree, one active
Release build directory, and no running ckVision processes. The changed
`tests/test_windows_terminal_subsession.cpp` matched the Mac source byte for
byte (SHA-256
`7DAA70A05BA9B9111E4A3537F28AB22C0CBAF78C943252F86954F3834B010E3F`,
guest write time 11:17:44 UTC); no AppleDouble metadata file was transferred.
The strict ARM64 MSVC Release build ran 11:20:31.380–11:20:52.319 UTC
(20.94 seconds), recompiled that test translation unit, relinked the runner,
and reported zero warnings. This falls within the recent incremental build
range. The exact runner at
`C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe`
was written 11:20:51 UTC, after the new source, and has SHA-256
`40EC057EAD5E68170BCB449EA8996F058BACDBA9519375F493FF8CF16EC556C0`.
The unchanged library/fixture fingerprints are recorded in the preceding
checkpoint. The focused ConPTY suite passed **14/14** internal cases at
11:21:10–11:21:15 UTC. The full hosted Release CTest matrix passed
**163/163** at 11:21:35–11:22:55 UTC, including the mandatory provenance
checks. Logs and hashes are retained in the external-drive task workspace.

## Three-platform generated-golden byte checkpoint

The six ckVision golden generators now write binary files on every host. Four
manual generators previously opened text-mode output, allowing Windows CRT
newline conversion. The other two were already binary. The three checked-in
editor search theme fixtures were refreshed after review of the current
editor chrome and wrap/status output; the editor smoke suite now compares
Dark, Light, and Mono frames byte for byte against their respective fixtures.
A new `generated_golden_bytes` CTest entry runs all six generators in an
explicit build-owned temporary directory, compares all 16 generated outputs
with the pinned fixtures, and removes successful scratch output. It uses only
the Python standard library.

| Host | Strict Release build | Full hosted CTest | Generated fixture bytes |
|---|---|---|---|
| macOS AppleClang | incremental all-target build, 11:31:18–11:31:23 UTC, 5.48 s, zero warnings | 177/177, 12.91 s | 16/16 |
| Linux ARM64 GCC 14.2 | clean all-target build, 11:41:15–11:42:53 UTC, 97.45 s, zero warnings | 177/177, 9.50 s | 16/16 |
| Windows 11 25H2 ARM64 MSVC 2022 `/W4 /WX` | clean all-target build, 12:16:14–12:40:30 UTC, 1456.59 s, zero warnings | 164/164, 92.86 s | 16/16 |

Each host reported the same aggregate manifest SHA-256:
`03fab0fe422463bdfbcbd11bfce8c08e09ad2d0b5a3cc39734199d2caea52ac4`.
The Windows editor smoke suite passed all 17 internal cases, including the
three theme comparisons. Both mandatory provenance CTest gates passed in each
full suite. The Mac output is
`/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-build`; the Linux output
was `/tmp/ckvision-wp37-golden-20260924a/build` in the Linux guest; the
Windows output is
`C:\Users\klukas\ckvision-wp50-20260924a\build` on the external-drive
VM disk. Build and CTest logs are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn`.

The Mac build was incremental after the four generator and fixture changes;
its log names the recompiled generators and test runner, and the runner was
written at 11:31:23 UTC, after those inputs. The Linux build compiled 380
Ninja steps from a clean directory; no comparable recent clean GCC build with
this exact target set exists. The Windows configure took 124 s, followed by
a genuinely cold 299-translation-unit compile. Its 1457 s compile took about
2.8 times the earlier 522 s all-target run, whose log named only 135 C++
inputs because it reused objects built earlier. The current log shows the
full input graph and zero warning/error diagnostics; duration alone is not
used to establish freshness. The Windows library was written at 12:26:52 UTC
(SHA-256 `B7EEF12B1B738BEA5D7572857F1731DFCA41E6B32AD1E2889D8C6475A6E342D4`),
and the test runner at 12:40:28 UTC
(`E171A15407109953063BDBF315803831773C8E049675916BBA82CDF7FA040EEC`).
The Mac runner SHA-256 is
`8F49E813767EF7DBC8698A66A845BE216CD54F34EEB6B53E73A9C2D7870A45D5`.
Source fingerprints for the new checker, editor test, Dark fixture, and
desktop-dialog generator are respectively
`F08265E5863BE75AA58AC39A1D2119A14FD2560AAB3E5B7FD6B54C3E7B8A283F`,
`6842F3C8EB432E75FEEA7EB78D3DFC87742E8707B18EBB4B0215C3080916B840`,
`1A42ABFFFC9430FA71D4EA71828E800562117B4835A0573AC492E335B4C8B501`,
and `F4B6A0104D3AEE52F1C7815453312AD766AC1869C23485B929A58CE66571F925`.
All 798 guest source files matched the Mac source manifest byte for byte
before the successful build and again after it.

During preparation, the Windows VM's SSH service stopped returning a banner.
Lima's graceful stop did not complete within its three-minute window and
force-stopped QEMU. The next source audit found 12 guest-file mismatches; one
golden generator had been replaced by zero bytes. Those guest files were
repaired from the Mac worktree, all 798 source hashes were rechecked, the
failed/partial Windows build was discarded, and the strict build and full
suite above ran from a fresh build directory. The guest clock was corrected
to UTC before timestamp validation. The failed build log and source-audit
manifest are retained beside the successful logs.

This checkpoint proves equality only for the 16 named generated symbolic
fixtures. It does not close the complete WP-37/WP-50 cell/pixel golden and
interactive Windows Terminal matrices. The VM still presents a lock screen;
Windows Terminal Preview 1.25 remains staged but unregistered until an
interactive desktop session is available. WP-37 and WP-50 remain in progress;
WP-38 remains blocked on their acceptance.

After these evidence and roadmap edits, the current Mac runner passed all ten
selected documentation, layer, and provenance CTest gates; `git diff --check`
was clean. The Linux scratch build was removed after its logs were preserved.
Fifteen older in-repository ckVision build trees (about 16 GiB, all predating
the current source) were inventoried by exact path, size, timestamp, and CMake
source identity, then removed. The inventory is retained as
`obsolete-build-inventory.tsv` in the external task workspace. The current
verified Mac and Windows builds remain available.

## 2026-09-25 nested-child host checkpoint

WP-50 now has a real separately launched ckVision `Application` inside a
Windows private ConPTY, driven through a parent `TerminalView` and focused
`Application` input route. Its parent headless scene retains all cells outside
the view. The named Windows host passes 15/15 inbox and 15/15 pinned modern
ConPTY cases; the strict hosted Release suites pass 177/177 on Mac, 177/177
on Linux, and 164/164 on Windows with this fixture. Exact source/output
hashes, build timings, and the still-open live Windows Terminal boundary are
in [WP-50 evidence](windows-wp50-evidence.md). WP-37 and M7 remain open for
the real Windows Terminal and complete golden/degraded-host matrix.


## Live desktop availability and outer-host checkpoint — 2026-09-25

The earlier lock-screen and unregistered-Preview statements above are
historical. The owner authorized a dedicated standard `cktest` account and
automatic sign-in for this external-drive VM. Restart, `quser`, and the
actual VM display confirmed an active `cktest` console session; Terminal
Preview 1.25.1912.0 is registered there. The exact Preview and current
ckVision GUI process paths, account/autologon facts without credentials,
source/output hashes, screenshots, and live nested/Sixel/paste/resize
results are in [WP-50 evidence](windows-wp50-evidence.md). The live paste
trial found and then verified a D-040 decoder correction. Current Windows
focused suites pass 78/78 input-decoder and 15/15 inbox plus 15/15 pinned
modern ConPTY cases; Mac and Linux hosted CTest suites pass 177/177 each.

WP-37 and M7 remain open for the complete live cell/pixel mouse and graphics
matrix, full cross-platform golden equality, remaining capability/degradation
and failure/recovery rows, and all global release gates. WP-38 remains
blocked on that acceptance, not on the former VM sign-in condition.

## Clean-build live Windows Terminal checkpoint — 2026-09-25

The source-refresh incremental Windows `ALL_BUILD` produced 13 failures in
164 hosted CTest suites, so that mixed-object output was discarded. A fresh
Visual Studio 17 2022 ARM64 Release `ALL_BUILD` completed in 657.50 seconds
with zero compiler/linker diagnostics; the current runner passed 164/164 hosted
suites, including both provenance checks, and focused 78/78 decoder plus
15/15 inbox and 15/15 modern ConPTY cases. The verified GUI SHA-256 is
`7EBD965F9163D17B849A68DAC794D40C68691D6BD917EB49C33CA56F8109297E`.
The real Preview 1.25.1912.0 console session showed clean Ctrl+V text, nested
ckVision input and exit, and contained Sixel under the modern runtime. Exact
timings, process paths, hashes, logs, and screenshots are in
[WP-50 evidence](windows-wp50-evidence.md). WP-37 is still open for live mouse,
degraded-host, complete golden, and broader platform/release acceptance.

## Live mouse and lifecycle checkpoint — 2026-09-25

After calibrating to the VM's actual 800×600 framebuffer, an interactive
mouse click opened ckVision's File menu in Windows Terminal Preview. Three
private ConPTY hosts were concurrently present under one ckVision process;
Window → Next returned to the unchanged Sixel demo. File → Quit removed the
GUI and all observed private child processes, and the verified GUI relaunched
with a new PID. [WP-50 evidence](windows-wp50-evidence.md) records exact PIDs
and captures. Pixel mouse, complete per-window isolation, degraded-host
recovery, and golden/release matrices remain open.


## Persistent test desktop and real degraded outer host — 2026-09-25

With the owner's approval, the dedicated VM test account now disables its
screen saver and secure auto-lock, and the VM power scheme sets AC/DC display,
sleep, and hibernate timeouts to never. Reboot confirmed automatic `cktest`
sign-in and an active console session; no plaintext Winlogon password was
stored. The before/after and reboot verification records are linked from
[WP-50 evidence](windows-wp50-evidence.md).

The same hash-verified ckVision example ran under the real Windows Console
Host for a degraded outer-host trial. Its nested modern ConPTY Sixel child
produced a visible `[sixel]` text fallback in the parent window, while the
Preview host had rendered the contained color raster. The Console Host and
child PIDs, visual capture, clean File → Quit, and verified Preview relaunch
are recorded in [WP-50 evidence](windows-wp50-evidence.md). The live Preview
run also now covers individual terminal-window close, a surviving shell,
immediate reopen, and a nonzero child exit without parent termination.

WP-37 and M7 remain **Open** for complete pixel-mouse and degraded-input
routes, cross-platform cell/pixel golden equality, and global release gates.


## Isolated private ConPTY host failure — 2026-09-25

The live Preview trial killed only one verified app-local OpenConsole child
host. Its terminal became exited while the sibling terminal continued to
execute commands; a newly opened terminal received a fresh child host and
usable prompt. [WP-50 evidence](windows-wp50-evidence.md) records exact PIDs,
paths, before/after process snapshots, GUI captures, clean teardown, and
relaunch. This closes that observed failure path, while WP-37 remains open for
the complete degraded-input, pixel-mouse, golden, and release matrix.


## Live gallery cell mouse and terminal pixel-mode probe — 2026-09-25

The hash-verified gallery (`E46BD59F5B1D5AC4D45172B27EDE423D11EF12A617EEC40731ABAB3AF499B4FA`)
ran in Windows Terminal Stable 1.24.11911.0, console session 1. Its Controls
window partially covered a live Sixel gradient. A cktest-session pointer at
VM pixel `(200,276)` targeted the Terminal window, and one real click on
**Greet** opened a modal displaying `Hello, stranger!`. Enter dismissed it;
the image and overlapping windows redrew without visible spill or residue.
`windows-gallery-processes.json`, `windows-gallery-pointer-result.json`,
`windows-gallery-launch-screen.jpg`, `windows-gallery-greet-click.jpg`, and
`windows-gallery-after-modal.jpg` under
`/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g` are the process,
pointer, and actual-display evidence. This confirms a live cell mouse route
and observed Sixel occlusion/redraw, not pixel-coordinate reporting or a
pixel-plane golden.

The Windows-only fixture `windows_terminal_child.exe` now has an
`outer-live-mouse` mode that records negotiated capabilities and actual
`MouseEvent` coordinates. The final source SHA-256 is
`8F597EBF11F007710DDCB3D9DF6A32C9CFA76A43F974C6D3637D092962262C5A`.
A strict ARM64 Release `ALL_BUILD` recompiled that source and linked the
fixture in 8.26 seconds (08:40:52–08:41:00 UTC), with zero compiler/linker
diagnostics; the 48-line log names `windows_terminal_child.cpp`. Its output
SHA-256 is `E74A3593BA755BF3B4F6806979F4AA6E85743CBD9CBECEE0D923807AB35043B6`,
written after build start. The prior comparable one-source incremental builds
were 6.02 and 5.71 seconds; the extra time did not add targets or diagnostic
failures. The final hosted Windows CTest run passed **164/164** in 26.15
seconds, including both provenance gates. Build/test logs, timestamps, and
hashes are in the external evidence directory.

A live click in Stable 1.24 produced press/release at cell `(18,10)` with
`pixel=none`; its probe reported SGR mouse, `cell_pixels=10x20`, Sixel
available, and `pixel_mouse=0`. The same verified fixture was launched through
Preview 1.25.1912.0's installed `wt.exe` without changing the account's
default terminal. A live click there produced press/release at cell `(12,7)`
with `pixel=none`; Preview reported the same capability values. The logs are
`windows-live-mouse-probe.log` and `windows-preview-live-mouse-probe.log`;
`windows-stable-live-mouse-processes.json` and
`windows-preview-live-mouse-launch.json` identify the binaries and hosts.
The one-off tasks and profile scratch were removed; the original unset
`cktest` default-terminal preference was verified unchanged. The VM was left
with one hash-verified ckVision terminal GUI in Preview.

The installed Windows Terminal hosts do not advertise SGR pixel mouse mode
1016. Microsoft's [open upstream feature request](https://github.com/microsoft/terminal/issues/18591)
corroborates that host limitation. ckVision correctly retained cell mouse
input instead of inventing pixel positions. WP-37 and M7 remain **Open** for
true live pixel-mouse acceptance on a capable host, the complete pixel/cell
golden matrix, and the other release gates.


## Live native clipboard export and paste import — 2026-09-25

The Windows-only fixture now also has an `outer-live-clipboard` mode. In the
active `cktest` desktop, it calls `WindowsTerminal::write_clipboard` with the
known UTF-8 string `CKV-LIVE-CLIPBOARD-✓` (22 bytes), then waits for a
paste-marked `TextEvent`. It ran as PID 2076 under Windows Terminal Preview
1.25.1912.0 PID 9012 and its outer OpenConsole PID 9076, launched explicitly
through the installed Preview `wt.exe`. A real Ctrl+V delivered one event with
22 bytes and an exact match. `windows-preview-live-clipboard-probe.log` records
`EXPORTED bytes=22`, `PASTE bytes=22 matched=1`, and `END matched=1`;
`windows-preview-live-clipboard-launch.json` records the exact paths, PIDs,
and fixture fingerprint. This is a live export/import round trip through the
native Windows clipboard and Preview's bracketed-paste input route.

The final fixture source SHA-256 is
`8990414F8BF31308B0C421B8CDF673B4DB7FA6357C94A1DEFF39FAACEC7CABA7`.
Strict ARM64 Release `ALL_BUILD` recompiled it at 08:49:49–08:49:58 UTC
(8.97 seconds) with zero compiler/linker diagnostics; output SHA-256
`E8AA4900ACFDA6391990C625E9468C748A7E11C5B9CFA42848E9BF6B12C09D8A`
was written after build start. This one-source build is consistent with the
preceding 5.71–8.26 second incremental runs; its log names the compiled
source and linked fixture. The full hosted Windows CTest suite passed
**164/164** in 28.86 seconds, including both provenance gates. Build, CTest,
process, and clipboard logs are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g`. The temporary
Preview task, copied fixture, and guest test scratch were removed. The
`cktest` default-terminal preference remained unchanged.

WP-37's observed native clipboard round trip now passes. True pixel mouse,
complete cross-platform cell/pixel golden equality, abnormal-exit and broader
degraded-host rows, and release gates remain open.

## Refreshed portable golden lanes — 2026-09-25

The current external-drive macOS build passed its `ctest -L golden` lane
**27/27**. The current Windows ARM64 Release build in the PRO-BLADE Lima VM
passed the same lane **26/26** at 09:01:13–09:01:21 UTC. The one-test count
difference is the Mac-only `terminal_visual_capture` artifact test; both
hosts ran and passed `generated_golden_bytes`,
`suite_test_virtual_display_visual_golden`,
`suite_test_gallery_visual_golden`, and
`suite_test_terminal_cell_golden`. The Windows test used the Visual Studio
BuildTools CTest executable and a guest-local temporary directory. The
earlier clean Linux ARM64 build passed its complete 177/177 suite, including
the golden lane, as recorded above. These runs reconfirm the pinned symbolic
bytes and hash-pinned decoded pixel scenes on the available builds. They do
not yet provide the complete raw cell/pixel byte matrix or close the live
pixel-mouse and remaining host rows.

## Three-platform raw cell/pixel golden expansion — 2026-09-25

`generate_terminal_plane_goldens` adds ten compact protocol, embedded-child,
and Presenter fixtures and three captures from the real Gallery application
object graph. Six `.rgba` files contain a fixed ASCII dimensions header and
complete row-major RGBA planes; seven `.dump` files contain exact styled-cell
frames. The Gallery Sixel plane is 720×432 pixels with 41,472 opaque pixels;
its NoGraphics companion shows `[image]` fallback cells and no raster output.
The other fixtures cover decoded protocol Sixel, embedded-child Sixel,
partial occlusion, a moved raster with stale-pixel clearing, and the
NoGraphics presenter fallback. The existing 16 generated symbolic scene
dumps remain in the same byte comparator, giving **29/29** pinned outputs.
`docs/golden-format.md` documents the raw companion format.

| Host | Freshness/build evidence | Final full hosted suite | Generated byte manifest |
|---|---|---|---|
| macOS AppleClang | new generator compiled strictly at 09:14:45–09:14:49 UTC; final all-target build completed at 09:16:42 UTC, zero diagnostics | 177/177, 14.68 s | 29/29 |
| Linux ARM64 GCC 14.2 | clean 382-action Release build 09:09:38–09:13:59 UTC; final generator recompiled 09:15:46–09:15:49 UTC, zero diagnostics | 177/177, 13.45 s | 29/29 |
| Windows 11 25H2 ARM64 MSVC 2022 `/W4 /WX` | final generator compiled 09:16:29–09:16:37 UTC; final `ALL_BUILD` completed 09:17:16–09:17:22 UTC, zero diagnostics | 164/164, 21.25 s | 29/29 |

Every host reported the same aggregate SHA-256 manifest:
`5b28144dbfde2845aaa8e1f206aaa14467145df91f2f35e88a68719659a2e8b8`.
The final generator source SHA-256 is
`8e3b976aae2721417894dcc5c84481843692a0016b4c72c65175ee5e9828520c`;
the comparator SHA-256 is
`27692296546b5a143f67b2a15d6cc9a5e6ab0fe406400ee40fe5fc14242eff66`;
the pinned full Gallery RGBA SHA-256 is
`39313af5ea51d002705f808eb9fb1b1b04f308ee65df7e39fd524654d3552564`.
Windows verified matching guest source hashes before compiling; its output
`C:\Users\klukas\ckvision-wp50-20260924a\build\tools\docgen\Release\generate_terminal_plane_goldens.exe`
was written at 09:16:37 UTC after those inputs and has SHA-256
`498f81d4fe684dce5891dbe815c799976fd5590ac5b70ce60e04806ed832caca`.
The corresponding Mac and Linux output paths are
`/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-build/tools/docgen/generate_terminal_plane_goldens`
and `/home/klukas.guest/ckvision-rawgolden-20260925-0909/build/tools/docgen/generate_terminal_plane_goldens`.

The Linux clean build took 261 seconds versus the prior 97.45-second clean
GCC checkpoint. Its retained log shows all 382 compile/link actions, no
cache-only shortcut, and no compiler diagnostics; it overlapped the Mac
all-target build and Windows VM work on the same host. Contention is a
plausible contributor to the longer elapsed time; the complete action log and
source/output hashes establish freshness independently of timing.
The final incremental Linux generator build was three Ninja actions in about
three seconds. Build, full-suite, and byte-comparison logs are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g`.

This proves equality for the named 29 outputs, including actual decoded
pixel bytes and Gallery cells. WP-37 and M7 remain **Open** for unrepresented
cell/pixel scenarios, true live pixel mouse on a capable host, remaining
host-degradation rows, and global release gates; WP-38 has not begun.

## Live multi-session input and history checkpoint — 2026-09-25

The named Preview 1.25 session now also passes one real two-window isolation
sequence: distinct ConPTY cmd/host pairs, different input/output markers in
each window, Terminal 2 PageUp history preserved across a switch to unchanged
Terminal 1 and back, typing from history returning to the live edge, and
individual close leaving Terminal 1 operational. A genuine File-menu
press–drag–release also opened a fresh third child, which closed cleanly. The
exact PIDs, executable hash, guarded pointer records, and actual VM screenshots
are in [WP-50 evidence](windows-wp50-evidence.md). This advances WP-37's
live keyboard, cell-mouse, and embedded-terminal rows. Pixel-mouse mode 1016,
other degraded-host/input combinations, remaining cell/pixel scenes, and
release gates stay open.

## Contained TerminalView raw planes — 2026-09-25

The `generated_golden_bytes` matrix now pins **45/45** outputs: 16 symbolic
scene dumps plus 15 raw cell dumps and 14 complete decoded RGBA planes. Eight
new cell/pixel pairs capture the real contained `TerminalView` through initial
paint, parent move and resize, overlap, child clear, repaint, parent close,
and NoGraphics fallback. The child raster starts with 240 opaque pixels in a
120×72 plane; the higher window leaves 48, clear and close leave zero, and
NoGraphics emits a `[sixel]` cell fallback with zero raster pixels. The fixture
and generator compare raw bytes, not only raster bounds or hashes. WP-50's
[contained-child checkpoint](windows-wp50-evidence.md) records the scene path
and assertions.

| Host | Final generator build | Full hosted suite | Raw-byte comparison |
|---|---|---|---|
| macOS AppleClang | external PRO-BLADE build, final strict generator recompile 09:57:34–09:57:37 UTC, 3 s, unchanged source bytes | 177/177, 12.95 s | 45/45 |
| Linux ARM64 GCC 14.2 | fresh Release build 09:48:22–09:49:30 UTC, 68 s, 322 C++ compile actions; final incremental rebuild 09:52:22–09:52:24 UTC, 2 s | 177/177, 10.19 s | 45/45 |
| Windows 11 25H2 ARM64 MSVC 2022 `/W4 /WX` | final generator compiled and linked 09:52:26–09:52:32 UTC, 6.40 s | 164/164, 19.83 s | 45/45 |

The aggregate output manifest is
`611b95747c089cbe8332afd151d03a2b6cc6b204f24ca78669301984ff633cea`.
The final generator source SHA-256 is
`173a334875c5bcca9f8396bb8e3332a91366e560abd218432dbeb7777f1ff7d6`;
the checker SHA-256 is
`967e3deba8b295a63f685b55497ab0511aea8638f75c2f8e677e76df2e433eac`.
The pinned initial/close RGBA hashes are
`eb4934b63488075ac37f4080dcc1c7c191f0a230c508db897d04db005401746d`
and
`ef52a2a3d415c2811704ec265d6ffded2ea5aebb49a3300b073b39614e8aefaa`.
The Windows guest verified matching source/fixture hashes before testing; its
rebuilt generator executable SHA-256 is
`c23e8831bda6978ec3021d7b23f6bc73fe5a9dd64856ac2eb18f26016fcd540c`.
The verified Mac/Linux executable paths were
`/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-build/tools/docgen/generate_terminal_plane_goldens`
and
`/home/klukas.guest/ckvision-contained-20260925/build/tools/docgen/generate_terminal_plane_goldens`.
The guest-local Linux build was removed after its logs and executable hash were
retained under `/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g`.
The Windows output is
`C:\Users\klukas\ckvision-wp50-20260924a\build\tools\docgen\Release\generate_terminal_plane_goldens.exe`.

The Linux clean build was shorter than the prior 97.45-second baseline. Its
fresh Makefiles build log contains 322 C++ compilations and subsequent links,
with no compiler-launcher cache configured and no diagnostics; its successful
full CTest and matching raw bytes rule out a skipped generator. The prior
261-second checkpoint was longer under concurrent host work, so elapsed time
alone is not treated as proof. The validated raw-byte evidence advances WP-37,
while pixel mouse on a capable host, the unrepresented scene/input combinations,
and WP-38 release audit remain open.

## Live Windows alternate buffer and private child resize — 2026-09-25

The named Preview 1.25 session now adds a real contained `alternate` fixture
observed before and after a host resize: parent chrome stayed intact, the
alternate text remained inside Terminal 1, and exiting restored the previous
primary cmd screen and prompt. A separate fixture reported its private ConPTY
size changing from **70×18 to 64×17** after the host shrank and the parent
window was clamped. The host was returned to its original bounds; the child
window retained its smaller bounds under the documented grow policy. The
exact rectangles, process lineage, hashes, and screenshots are in
[WP-50 evidence](windows-wp50-evidence.md). This advances the live Windows
alternate-buffer and resize rows. Uncovered combinations and the release
audit remain open.

## Live held mouse motion and hosted-gate repeat — 2026-09-25

The named signed-in Preview session passed a guarded left-button drag inside
Terminal 1's real nested child: one down, four held moves, and one up, all
recorded by the child `WindowsTerminal` backend. The exact source/output
hashes, process lineage, cell coordinates, and injection-method comparison
are in [WP-50 evidence](windows-wp50-evidence.md). The outer host advertised
SGR cell mouse and no pixel mouse. The previously unproven live held-motion
route is now observed; parent-window grip resize and a pixel-capable host
remain open.

The fixture-only strict Windows Release rebuild compiled the changed source
without warnings. A first full four-way CTest run failed 2/164 under load:
the ConPTY resize test missed its expected `SIZE:57x10` text after sending
input immediately after `ResizePseudoConsole`, and `scene_budget_gate` hit its
60-second timeout. Both passed in a focused serial rerun (2.97 s and 9.81 s),
then the full serial suite passed **164/164** in 47.99 s at
10:30:02–10:30:50 UTC. A complete four-way rerun also passed **164/164** in
18.81 s at 10:37:27–10:37:46 UTC; its ConPTY and budget entries took 3.35 s
and 14.50 s. The initial failure remains recorded as a possible
load-sensitive resize/benchmark risk; one green repeat does not prove it
cannot recur. Test logs are under
`/Volumes/PRO-BLADE/tmp/ckvision-live-mouse-n6MuYtYb`.

## Live parent-window grip resize — 2026-09-25

A guarded, signed-in Preview pointer drag on Terminal 1's bottom-right grip
resized the actual contained window. Its private cmd then reported
**57 columns × 15 lines** via `mode con`, versus the previous 64×17 ConPTY
size. The parent menu, frame, and cmd prompt remained live and visually
contained. The exact PID guards, pointer positions, screenshots, and one-off
task recovery are in [WP-50 evidence](windows-wp50-evidence.md). This adds a
real mouse-driven parent-to-ConPTY resize route to the Windows live matrix;
resize storms and the pixel-mouse-capable-host row remain open.

## Live Sixel clipping and private resource close — 2026-09-25

The named Preview GUI rendered a child Sixel image in a second contained
window. A guarded mouse-grip drag narrowed that window through the image:
pixels inside the new frame retained their original raster colors, while
samples newly outside it became the underlying Terminal 1 black. Closing
only the Sixel window removed its PowerShell child and app-local ConPTY host;
the original cmd, private host, and single GUI survived. The exact live
screenshots, sampled RGB values, coordinates, and PID lineage are in
[WP-50 evidence](windows-wp50-evidence.md). This adds a real graphics plus
mouse-resize plus close route to WP-37. The remaining cell/pixel scenario and
degraded-host matrix still require acceptance.

## ConPTY resize-gate correction and full Windows repeat — 2026-09-25

The earlier loaded four-way test failure's resize case used a one-shot child
size read immediately after `ResizePseudoConsole` and an input trigger. The
Windows fixture now waits up to eight seconds for the child console itself
to report the requested size, then exits with the exact observation or a
nonzero timeout. The regression still asserts `SIZE:57x10` and exit zero.
This test-only change makes the acceptance claim about observed ConPTY size
rather than scheduling order. The strict ARM64 runner and fixture were rebuilt
from matching source hashes without diagnostics. The ConPTY suite passed ten
consecutive runs; the full Windows four-way suite passed **164/164** in 17.46 s,
including its generated-golden, budget, and two provenance gates. Exact output
hashes, timing, and logs are in [WP-50 evidence](windows-wp50-evidence.md).
The previous budget timeout under load is retained as a separate performance
risk; its current 13.88-second parallel pass does not prove the gate cannot
be delayed by future contention.

## Three-platform contained resize-sequence matrix — 2026-09-25

Five new paired raw cell/RGBA captures cover a real `TerminalView` inside the
portable parent scene through initial raster, clipped shrink, move, restore,
and several bounds changes before one presented frame. The generator now
checks exact opaque counts and rejects any pixel outside the current child
content rect. It verifies that the final restored/burst outputs match the
earlier enlarged state byte for byte, while all 29 previous terminal-plane
outputs stay byte-identical. The prior `contained_repaint` fixture has 96
opaque pixels; the earlier 48-pixel evidence statement was incorrect and has
been corrected in [WP-50 evidence](windows-wp50-evidence.md).

The three-host generated-golden manifest is now **55/55** with the same
SHA-256 `704bbc30231e66f8ed562de8bfbde6d798eb9441fe8f88ac2ba65283b39eeec0`
on macOS, Linux ARM64, and Windows ARM64. Full Release CTest passed 177/177,
177/177, and 164/164 respectively. The Linux build was a fresh 382-action
current-source build; Mac and Windows recompiled the changed generator.
Exact source/output hashes, build times, raw-pixel counts, and retained logs
are in [WP-50 evidence](windows-wp50-evidence.md). This expands the portable
matrix; WP-37 remains open for the unrepresented states, true live pixel
mouse on a capable host, degraded-host routes, and release gates.

## Live held Sixel resize sequence, ConPTY size, and survivor input — 2026-09-25

The named Preview GUI passed a guarded held mouse resize of a second Sixel
window: eight shrink moves, eight restore moves, then 16 rapid alternating
moves before release. The post-release VM capture shows the raster clipped to
the narrowed frame. Its private PowerShell child directly reported **35×18**
through `mode con`. Closing that frame removed only its PowerShell and private
OpenConsole processes. The original Terminal 1 cmd remained and printed a
new `echo stormsurvivor` command after focus, proving input still worked.
Exact PID guards, timestamps, RGB samples, screenshots, and guest cleanup are
in [WP-50 evidence](windows-wp50-evidence.md). This extends the observed live
Windows resize and resource-recovery matrix; intermediate-frame raster states,
remaining cell/pixel scenarios, degraded-host rows, and WP-38 audit remain
open.


## Three-host raster-shadow matrix and live Preview pixels — 2026-09-25

The scene/presenter now applies higher-window shadows to retained child
rasters. Three new paired cell/RGBA states verify one shadow, coincident
shadow union without double dimming, and exact restoration. Together with
the corrected overlap/repaint planes, the generated-golden manifest is
**61/61** with SHA-256
6e1d2351bb2cc836f8d9fdb81eda97ed2e25f90018a74b2be124841445fb5814
on macOS, Linux ARM64, and Windows ARM64. Fresh all-target hosted CTest
passed **177/177**, **177/177**, and **164/164** respectively; a separate
macOS ASan/UBSan all-target build passed **177/177**. Mandatory provenance
and scene-budget gates passed. The verified new Windows Preview GUI PID 6628
showed a Sixel raster dim under an open File-menu shadow at (220,175) from
RGB (208,208,57) to (104,104,29), then restore to the original value after
Escape. Source/output timestamps, hashes, build timing, screenshots, and
exact pixel comparison are recorded in
[WP-50 evidence](windows-wp50-evidence.md).

This closes the specific portable raster-shadow and one live Preview
shadow/restore proof. WP-37 and M7 remain **Open** for the remaining
cell/pixel and degraded-host matrix, a pixel-mouse-capable host, and WP-38's
global release audit.


## Console Host degraded input and strict live gesture — 2026-09-25

A fresh MSVC ARM64 Release build fixed the live mouse fixture to require a
complete left press/release instead of any two events. The full Windows hosted
suite passed **164/164**. In the signed-in desktop, that corrected fixture ran
under the system Console Host, not Windows Terminal. It reported SGR cell
mouse, no pixel coordinates or Sixel capability, and recorded Move, Move,
left Down, left Up at cell (31,3) after one guarded click. The fixture exited
0; the sole verified Preview ckVision GUI survived. Exact hashes, PIDs,
timestamps, logs, visual capture, and disposable-task cleanup are in
[WP-50 evidence](windows-wp50-evidence.md). The earlier Move + Down result
from the old count-based fixture is expressly excluded.

This passes one real Console Host degraded-input row. WP-37 and M7 remain
**Open** for the distinct plain-VT ConPTY row without enhanced input, true
pixel mouse on a capable host, other cell/pixel scenes, and the release gates.


## Plain-VT ConPTY row and Sixel demo prompt repair — 2026-09-25

A new case in the real Windows private-ConPTY integration suite with no
capability replies verifies the outer terminal's legacy keyboard and SGR
cell-mouse degradation, with no pixel mouse, metric, or Sixel; injected SGR
Down/Up travels through ConPTY and is decoded by the child. This closes that
**test-harness plain-VT ConPTY row** while keeping the separate live-host and
true pixel-mouse rows open. The same update moved the Sixel Demo's interactive
shell prompt below the image. The POSIX real-child regression verifies the
picture remains opaque after shell input; the fresh Windows Preview GUI PID
5388 visibly renders an uninterrupted Sixel image with its PowerShell prompt
below. Full hosted CTest passed **177/177 macOS, 177/177 Linux ARM64, and
164/164 Windows ARM64**, with the two provenance gates and generated goldens
green. Source/output hashes, build freshness, exact pixel sample, process
lineage, and retained logs are in [WP-50 evidence](windows-wp50-evidence.md).
WP-37 and M7 remain **Open** for the remaining matrix and WP-38 audit.


## Concurrent Windows ConPTY graphics lifecycle — 2026-09-25

The Windows-only real-ConPTY suite now verifies two simultaneous Sixel child
processes, private red/green raster snapshots with the pinned modern runtime,
independent close/survivor input, and allocation of a replacement graphics
child. The same test verifies the deterministic no-raster fallback on inbox
ConPTY. The modern source suite passed **17/17** and full hosted Windows CTest
passed **164/164** with provenance and generated-golden gates. Exact source and
binary hashes, build timing, and retained logs are in
[WP-50 evidence](windows-wp50-evidence.md). This extends adapter/resource
coverage; WP-37 still needs live pixel mouse and the complete cross-platform
cell/pixel and release matrices.


## Live Preview Sixel move and parent uncover — 2026-09-25

A guarded held title-bar drag moved a contained Sixel window in the sole
verified Windows Terminal Preview GUI by 72×38 screen pixels. Across actual
uncompressed VM captures, **57,186 raster-interior RGB pixels** matched at
the translated location with zero differences, and the vacated region exposed
the unchanged Terminal 1 background. The Windows child and private host
survived. The first absolute-cursor attempt was excluded because its frame
did not move. Exact hashes, gesture/process reports, screenshots, and pixel
comparison are in [WP-50 evidence](windows-wp50-evidence.md). This adds one
live move/graphics/parent-uncover row; WP-37 remains **In progress** for the
other cell/pixel scenarios, pixel-mouse-capable host, and release matrix.


## Optional pixel-mouse host feasibility probe — 2026-09-25

Windows Terminal Stable and Preview on the named ARM64 VM did not advertise
SGR pixel-mouse mode in the earlier live probes. To look for a host that can
exercise that row, the VM downloaded the official WezTerm Windows x64 portable
release `20240203-110809-5046fc22` directly onto its PRO-BLADE-backed disk.
The archive matched its published SHA-256
`57e5d03b585303d81e8b8e96d1230362852eb39aca92b3b29c7a42cfb82f9ac4`,
and `wezterm.exe --version` succeeded under Windows-on-ARM emulation. A
limited signed-in `cktest` task then launched the GUI with the fixture, with a
plain `cmd.exe`, and with configuration skipped. Each GUI launch returned exit
code 0 within three seconds; no WezTerm, child shell, or fixture process
remained. The fixture produced no capability log in the final trials. This
establishes only that this x64 build did not provide a usable GUI host in this
VM configuration; it is **not** pixel-mouse acceptance or a ckVision failure.
The one-off task, VM scratch, and extracted runtime were removed after the
probe, leaving the sole verified ckVision GUI PID 5388 running. The launch
record and cleanup report are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-live-move-20260925T133636Z`. A suitable
Windows pixel-mouse-capable host remains required for the live row.


## Live Windows Sixel clear and reallocation — 2026-09-25

The verified Windows Terminal Preview GUI passed a real child `cls` that
removed all 46,280 sampled image pixels, with 78,480 sampled pixels outside the raster unchanged.
Closing that window released its PowerShell and private OpenConsole while the
other child survived; a File-menu gesture allocated a fresh private child/host
and restored 46,020 raster pixels exactly against an earlier live Windows
capture. The process lineage, exact pixel rectangles/hashes, guarded Session 1
input reports, and actual VM screenshots are in
[WP-50 evidence](windows-wp50-evidence.md). This adds live graphics clear and
resource recovery to the named-host matrix; true pixel mouse and the remaining
scene/input/degradation combinations still require acceptance.


## Live partial-raster crop correction — 2026-09-25

The named Windows Terminal Preview run exposed a persistent source-sampling
seam when a covering contained window was shrunk over a Sixel image:
204/36,640 far exposed pixels differed from the pre-overlap raster. The
presenter now samples the partial slice from the full image anchor, and a
decoded-Sixel regression failed before and passed after that correction.
The fresh Windows ARM64 Release build passed 164/164 hosted suites. In the
sole verified, newly launched GUI, the same live overlap route matched
36,640/36,640 exposed image pixels; closing the covering window restored
59,640/59,640 image and 89,846/89,846 right-of-picture child-content pixels. Exact executable,
source, screenshot, process, build, and CTest hashes and the three-host
177/177 macOS, 177/177 Linux, 164/164 Windows validation are in
[WP-50 evidence](windows-wp50-evidence.md). WP-37 and M7 remain **Open**
for the remaining live matrix and WP-38 audit.


## Live held-resize intermediate states and exact recovery — 2026-09-25

The named Preview GUI's Sixel child passed a guarded held resize with
shrink, restore, and 16 rapid alternating moves. Actual intermediate
framebuffer captures show the [sixel] cell fallback throughout the held
gesture, matching the documented raster-pacing policy in docs/graphics.md.
After release, 59,640/59,640 image and 89,846/89,846
right-of-picture child-content RGB pixels matched the pre-gesture capture. The same private PowerShell and
OpenConsole PIDs survived; the child accepted echo stormok and the image
still matched pixel for pixel. Exact phase times, screenshots, hashes,
process lineage, and cleanup are in
[WP-50 evidence](windows-wp50-evidence.md). This resolves the earlier
uncertainty about intermediate resize frames: they intentionally use the
cell fallback, then restore the raster on release. WP-37 and M7 remain
**Open** for the remaining matrix, especially true live pixel mouse and
WP-38 audit.

## Native Windows verified pixel-mouse path — 2026-09-25

The Windows-only private-ConPTY case now proves that a real WindowsTerminal
child promotes SGR pixel mouse only after DECRPM 1016 and XTWINOPS 16 replies,
then delivers left down, motion, and release with both exact pixel and
derived cell coordinates. The exact case and focused suite passed on the
named ARM64 VM; the strict Release all-target build completed without
warnings and the full hosted CTest suite passed 164/164. The runner
SHA-256 is
FDEE89F940658D576A5013A6B75E4C1DF57500A3EBF95D3536AF3B8D5F8922B0.
Source, fixture, build timestamps, case output, and retained logs are in
[WP-50 evidence](windows-wp50-evidence.md). Windows Terminal Stable and
Preview still have no observed live mode-1016 support, consistent with
Microsoft's open feature request:
https://github.com/microsoft/terminal/issues/18591
The real GUI pixel-mouse route and the remaining platform/release matrix
remain open; WP-38 follows their acceptance.

## Windows abrupt-child isolation checkpoint — 2026-09-25

The native private-ConPTY suite now verifies that a deliberately terminated
Sixel child exits with status 37 while its green graphics peer keeps its
raster and accepts input. A replacement red graphics child allocates and
works independently. The same case passes under inbox ConPTY's text
fallback. The exact case, all 19 optional modern-runtime cases, and the
full strict Windows ARM64 Release CTest matrix (164/164) passed on runner
SHA-256 74FC3A8DF475A1E7EAC20BA4F988C46CA13848082A90BB3862C88D66C4DA1CFF.
The source/output identity, build timing, and retained logs are in
[WP-50 evidence](windows-wp50-evidence.md). This is private-host abrupt
termination evidence; actual fault and live GUI recovery are still open.
WP-37's broader live matrix and WP-38 audit remain open.

## Live Preview abrupt-child recovery and peer input — 2026-09-25

A guarded live test terminated the contained Sixel Demo 2 PowerShell
without closing the ckVision GUI or Terminal 1. The GUI displayed
[terminal exited] while retaining the prior Sixel picture exactly
(59,640/59,640 sampled image pixels); its old private host exited.
Closing that window exposed Terminal 1, and the File menu allocated
a new PowerShell/private-OpenConsole pair with a raster matching the
baseline at 59,640/59,640 pixels. Right-of-picture child content
matched at 89,846/89,846 after reopening, while three actual outer
desktop strips matched at 23,120/23,120 throughout. The original cmd accepted echo livepeer
and displayed the output and a fresh prompt. The final Sixel Demo 4
frame retained the same exact raster in the sole original GUI PID 5828.
The raw VM captures, process PIDs, guarded actions, exact comparison,
and cleaned one-off task are in
[WP-50 evidence](windows-wp50-evidence.md). This is live forced-child
loss and recovery, not a fault exception. Live fault-triggered recovery,
true pixel mouse on a capable host, and the remaining matrix stay open.


## Live Windows fatal-runtime child and peer recovery — 2026-09-25

In the sole verified Preview GUI PID 5828, the interactive Sixel
PowerShell PID 4160 executed
[Environment]::FailFast("CKVISION_LIVE_FAULT_20260925").
.NET Runtime Event 1025 recorded the exact call, Application Error Event
1000 identified PID 4160 and exception code 0x80131623, and Windows Error
Reporting Event 1001 recorded the fatal report. The child and private
OpenConsole PID 6676 exited; GUI PID 5828 and original cmd PID 8996
survived. The GUI displayed [terminal exited] and retained all
59,640/59,640 sampled image pixels. Closing that window exposed the peer.
The File menu allocated new PowerShell PID 7788 and private OpenConsole
PID 3428. The replacement Sixel image matched 59,640/59,640 pixels;
the original cmd accepted echo faultpeer and showed its output and
fresh prompt. The final image again matched all 59,640 pixels. A
right-of-picture child-content sample matched 89,846/89,846 pixels after
reopening; three genuine outside-window desktop strips matched
23,120/23,120 pixels through every phase.

The live VM framebuffer captures, exact comparison JSON, Windows event
records, guarded Session 1 reports, executable identity, and cleanup are
in [WP-50 evidence](windows-wp50-evidence.md) and its external-drive
evidence directory. This closes one live fatal-runtime recovery route,
while other crash classes, true pixel mouse, and the remaining WP-37
cross-platform acceptance matrix remain open.


## Native Windows resource-limit recovery checkpoint — 2026-09-25

A real private-ConPTY child now exceeds a deliberately small Sixel payload
limit, then sends ordinary text and a valid Sixel image. The pinned modern
runtime reports the limit, drops the oversized picture, and renders the
following red picture. A second green graphics child retains its own
raster and accepts input. The inbox ConPTY passes the same lifecycle
under its text fallback. The exact inbox case, modern suite **20/20**,
and full Windows ARM64 CTest matrix **164/164** pass on runner SHA-256
1708F2C6A3DAEDE7CCEADE54A72F07E7DECF5B165A4E360518A978419137FE30.
[WP-50 evidence](windows-wp50-evidence.md) records source/output hashes,
build timing, logs, and the scope of this native test. The live
resource-limit and remaining cell/pixel/degraded-host scenarios remain
open for WP-37.


## Live Windows graphics resource-limit and peer recovery — 2026-09-25

In the sole signed-in Preview GUI PID 5828, a contained PowerShell wrote
4,202,496 Sixel payload body bytes against the default 4,194,304-byte
limit. A guarded pause allowed a real framebuffer capture of the visible
limit diagnostic, ordinary text after the rejected DCS, and no visible raster.
The same child then rendered a valid small red Sixel, retained it across
a Window-menu switch, and returned to its prompt. The original cmd peer
accepted echo limitpeer and displayed a fresh prompt. Closing the test
window removed its PowerShell/private-OpenConsole pair; File → New Sixel
Demo allocated a fresh pair and restored the original multicolor image
at 59,640/59,640 exact sampled pixels. Three actual outer-desktop strips
matched at 23,120/23,120 RGB pixels in every captured phase.

The command guard, payload report, raw VM framebuffers, exact comparison,
PIDs, executable identity, and cleanup are in
[WP-50 evidence](windows-wp50-evidence.md). This extends WP-37's live
graphics, degraded-payload, and resource-recovery rows. True live
pixel mouse and the remaining full scene/input/host matrix remain open.

## Alternate Windows pixel-mouse host probe — 2026-09-25

An official MSYS2 ARM64 Mintty was installed in a disposable directory on
the external-drive VM. A native ckVision `WindowsTerminal` fixture under
Mintty's ConPTY bridge recorded `mouse_protocol=2`, `pixel_mouse=0`,
`cell_pixels=7x14`, and `sixel=0`. Its later timeout recorded no mouse
events because no click was delivered before expiry. This is a negative
capability result for that exact host bridge, not a live gesture test or a
claim about Mintty outside ConPTY. The fixture record is retained under
`/Volumes/PRO-BLADE/tmp/ckvision-pixelhost-20260925`; the disposable
MSYS2 install, guest scratch, and scheduled task were removed. This did not
close the true pixel-mouse host row. The VM now runs without a foreground
QEMU display while Windows code builds and native tests continue.


## Three-host two-child golden checkpoint — 2026-09-25

The new WP-50 `contained_pair_*` golden triplet exercises two simultaneous
contained rasters, individual peer close with survivor retention, and fresh
replacement allocation. Each stage has a raw pixel-plane file and a cell dump,
with exact red/green/blue counts and content-rectangle no-spill assertions in
the generator. macOS, Linux ARM64, and Windows ARM64 each regenerated and
matched **67/67** pinned bytes with the same manifest SHA-256
`4964c83b9e06e42e64f605957d1a9bc7e029d56c206bc628ba75325b0d8dd85b`.
The full macOS and Linux suites passed **177/177** each; Windows ran the
changed generator's targeted golden gate after its earlier **164/164** full
suite. [WP-50 evidence](windows-wp50-evidence.md) records source/output
identity, build timing, and logs. This closes the portable two-child
raster-isolation golden row. True pixel mouse, other degraded-host and
cell/pixel scenarios, and the release audit remain open.


## Windows private-child resize-storm checkpoint — 2026-09-25

The WP-50 native Windows adapter now passes a concurrent red/green child
resize-storm test. Each child reports its final private console extent after
five interleaved resizes; distinct graphics identities/colors remain intact
under pinned modern ConPTY, while inbox ConPTY retains the specified text
fallback. Both children accept isolated input and exit cleanly. Inbox and
modern ConPTY suites pass **21/21** each; hosted Windows CTest passes
**164/164**. [WP-50 evidence](windows-wp50-evidence.md) records the source
and output hashes, timing, and exact logs. This is native transport coverage;
the remaining live GUI resize, pixel-mouse, and degraded-host rows stay open.


## Fresh Linux Clang sanitizer and fuzz corpus matrix — 2026-09-25

A fresh ARM64 Linux build from the committed worktree snapshot used Debian
Clang 19.1.7, Debug, `CKVISION_SANITIZE=address,undefined`, and
`CKVISION_BUILD_FUZZERS=ON`. The source snapshot included the WP-50 ConPTY
test changes and 67-file golden manifest; the portable generator and manifest
SHA-256 values were
`99c0e87b29143755001f3a02417eb75a62217166ef2f1e1ba63a4313c30d7c08`
and `b52fe52ac7de0caaceea7cf6e462fad39b6791bf130b77a3ff624e53f7614abf`.
The build ran 17:54:23–17:57:33 UTC (190 seconds) in
`/home/klukas.guest/ckvision-wp37-san-fuzz-20260925/build`, completing all
403 logged compile/link steps with no warnings. The prior comparable GCC
ASan/UBSan clean build took 235 seconds; the compiler, source set, and fuzz
targets differ, so elapsed time alone is not a freshness claim. The
instrumented test runner SHA-256 is
`16ea0be70db11aa8fa2862afc0ec0514f8e8dc8315c681741e203532e8fee458`
and the terminal-plane generator SHA-256 is
`17358116a484ab481a7db92cc495a9ade39b95714064c277d32ad87433230088`.
Full CTest ran 17:57:33–17:59:19 UTC and passed **186/186** in 106.09
seconds, including all nine bounded fuzz corpora, `generated_golden_bytes`,
and both mandatory provenance gates, with no sanitizer diagnostic. Exact
configure/build/test logs, timing, and hashes are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-wp37-san-fuzz-20260925`. This advances the
portable sanitizer/fuzz gate; it does not instrument Windows-only ConPTY
source or close live Windows host acceptance.

## Supplementary Unicode in live Windows editor input — 2026-09-25

A new live editor pass on the named Windows 11 ARM64 VM exposed a Windows input
transport defect while exercising WP-37's Unicode editor row. In Windows
Terminal Preview 1.25.1912.0, the `cktest` session-1 clipboard held the exact
UTF-16 sequence `03A9 4E2D D83D DE00` for `Ω中😀`. The pre-fix editor
rendered the emoji as two replacement glyphs, and copying the selected line
returned `03A9 4E2D FFFD FFFD`. The then-current `ReadFile`/CP_UTF8 VT input
route was the cause: the live `outer-live-clipboard` fixture exported a known
27-byte UTF-8 payload with `F09F9880`, but its paste event was 29 bytes and
ended `EFBFBD EFBFBD` (`matched=0`). A bounded diagnostic `ReadConsoleW`
fixture on the same host and clipboard returned the intact `D83D DE00` code
units. The diagnostic fixture was removed after the observation; its byte log
is retained with the validation records. Microsoft's
[ReadConsole documentation](https://learn.microsoft.com/en-us/windows/console/readconsole)
describes the wide-character console input path.

D-020 and the Windows architecture description were updated before code. The
backend now reads the same VT character stream with `ReadConsoleW`, combines
surrogate pairs across reads, replaces only malformed lone surrogates, and
encodes scalars as UTF-8 for the unchanged shared `InputDecoder`. It still uses
console records only to consume non-character events and observe resize. The
Windows-only live clipboard fixture now includes `✓-😀` and logs received
bytes. The automated ConPTY bracketed-paste test now sends `Ω中😀` as well as
ASCII text and requires an exact paste-marked event.

The fixed fixture, running under Preview in the real `cktest` desktop, received
all 27 bytes including `F09F9880` (`matched=1`, `END matched=1`). The rebuilt
editor executable SHA-256 was
`0E9866EED17B484EC14F5A3C59668F03AC36533579267FD36ACAF3CA72831887`,
launched as PID 1540 under Preview PID 4684. Its actual window displayed a
single emoji glyph. Pasting ` live-Ω中😀 ckVision`, selecting the line, and
copying it back returned the exact original UTF-16 units. Save cleared the
dirty marker; switching to the JSON sample and reopening the YAML sample
restored the edited line, and a second copy after reopen again returned
`D83D DE00` with `ExactMatch=true`. The final editor and Terminal processes
exited. The three captured window states have SHA-256 values
`d1e65e24d528bacb71bc3f4df8e70d568ce6917a6eb3766d242b3090ded75905`
(paste), `ed29e2102d98c09ce24b65b729988084fae05300cd1f61a3e12a877dbd96e44d`
(JSON), and `bf7cff5fcbff195336140b5584cca0a963fddcd2152dc7d5e716516a01c8018c`
(reopened YAML).

The final ARM64 Release `ALL_BUILD` ran 18:28:27–18:28:38 UTC in 10.79 seconds
with zero compiler or linker diagnostics. Its log explicitly recompiles
`windows_terminal_child.cpp` and `test_windows_terminal_subsession.cpp`; the
Windows backend and header were compiled into `cvision.lib` in the preceding
18:23:50–18:23:56 UTC build, after their source timestamps, and reused unchanged.
Final source SHA-256 values were `31BD959F9ACD911491914EED2A7AC89F6965517071CAF2E540CB492532331E78`
(backend), `9DEE9C518D32460E5288E9D13CE39A919A089A0A7F0A95A93C08CB71D2807FCF`
(header), `D4D1B0CC93A2BCDB676FC7A7732A58DDC294B1AD206CC20858E24A53BD96524B`
(fixture), and `CE3621C1BD4F820B57B90EE48B65C5A48D4E45E8CCA4EBEA33A89A703AD262BE`
(ConPTY test). Final `cvision.lib`, test runner, and fixture SHA-256 values were
`E40233F34D0EB91B4C57624DAD44B921FD9DEAF06BB8DD29AC403CB215377D96`,
`36DF800727A91ED3D7EA4ACAD39F0AA79AAEC3E1C32D38D47152E3AE3930242B`, and
`084A1B72A8793395901B7DBD6610B8BE6E0F8EE6728D720BBA8A7031E77433C8`.
The full Windows Release CTest passed **164/164** in 47.23 seconds; the exact
new Unicode ConPTY case passed separately **1/1**. Both Mac provenance gates
passed **2/2**. The previous macOS/Linux matrix remains the latest full run for
those targets; this patch changes Windows-only implementation and tests.
Build reports, logs, fixture byte records, and window captures are under
`/Volumes/PRO-BLADE/tmp/ckvision-wp50-vnc-20260925`.

This closes the observed supplementary-Unicode editor defect and the exercised
live editor input/search/save/reopen row. WP-37 and WP-50 remain open for the
remaining host capability, graphics/mouse, isolation, and release matrix rows;
none of those rows is inferred from this test.
