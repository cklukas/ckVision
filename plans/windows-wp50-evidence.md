# WP-50 named Windows evidence — 2026-09-24

## Host and build

The named host is the external-drive Lima VM `ckvision-win11` on
`/Volumes/PRO-BLADE/lima/ckvision-win11`, running ARM64 Windows 11 25H2
(build 26200) with 4 vCPUs and 8 GiB RAM. The native toolchain is Visual
Studio Build Tools 2022 17.14.41, CMake, and Windows SDK 10.0.26100.0.
Windows Terminal 1.24.11911.0 was installed and verified as the registered
ARM64 package. The prior `winget upgrade` had reported success while leaving
1.18 registered; the official MSIX bundle was installed with `Add-AppxPackage`
after its SHA-256 matched the package manifest.
The current source and Release build are on the VM disk at
`C:\Users\klukas\ckvision-wp50-20260924a\src` and `...\build`.
This host is available for Windows development, but an SSH-launched process
is not interactive Windows Terminal acceptance.

The Windows adapter uses a private ConPTY, named overlapped pipes, a child
process job, and a session-owned close operation. A strict `/W4 /WX` native
ARM64 build of `cvision_tests` and `windows_terminal_child` succeeded. The
`ctest -C Release --output-on-failure -L conpty` suite passes its eight cases:
private output and exit status; input and simultaneous-session isolation;
resize and alternate buffer; explicit argv and environment; explicit
no-graphics degradation; bounded flood tail; bounded close of a child that
ignores Control-C; and launch failure without native wait sources.

The initial byte-identical source manifest covered 791 files and had SHA-256
`2a2fee81dd95fbab47c2f85721244f1c178c33c1a3cbd6fae4474256c650d4c4`.
The native Release build validation started at 2026-09-24 06:53:26.525 UTC
and ended at 06:53:36.545 UTC (10.020 seconds) with exit code 0. It was an
incremental no-change build; the earlier strict rebuild compiled the adapter,
fixture, and test source. The exact verified runner is
`C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe`,
SHA-256 `AC40E2590AB1CDCCA8BEE1E598504E02DE037B9CC7F634F6889CCF52AF2D890D`.
Its UTC write time, 06:49:03.335, follows the adapter (06:36:37.794) and
test source (06:48:48.711); the child fixture executable at 06:45:02.054
follows its source at 06:44:37.880. This ordering, the exact 791-file guest
source comparison, and the successful native build establish freshness; the
10-second elapsed time alone is not used as proof. The ConPTY CTest suite
then passed in 2.78 seconds total with no failures.

## Open Windows graphics and interactive rows

An extra named-host probe used the same child fixture with the declared
Sixel profile and a one-column red Sixel DCS (`ESC P q #0;2;100;0;0 ~ ESC \\`).
The child enabled Windows VT output processing and printed a text marker on
the next row. The marker arrived, but the terminal snapshot contained zero
rasters and zero parser diagnostics. Moving the marker away from the image
cell did not change the result. Installing Windows Terminal 1.24.11911.0 did
not change this SSH-created ConPTY path. This is evidence of an unresolved
transport/capability gap, not evidence that ckVision's Sixel decoder accepted
the sequence. The passing no-graphics case deliberately declares `sixel=false`
and asserts that no raster is emitted.

Microsoft documents Sixel support in [Windows Terminal 1.22 and its newer
console host](https://github.com/microsoft/terminal/discussions/17809).
Its [default-terminal design](https://github.com/microsoft/terminal/blob/main/doc/specs/%23492%20-%20Default%20Terminal/spec.md)
distinguishes the inbox console host from the updated `OpenConsole.exe`
shipped with Windows Terminal. The observed SSH-created ConPTY behavior must
be resolved at the platform adapter/capability boundary before advertising
nested Windows graphics. No new mandatory dependency or vendor code is
approved by this evidence record.

WP-50 still requires the Windows Terminal interactive example, nested ckVision,
live outer-scene Sixel containment, error/resource recovery, and matching portable
goldens. WP-37 now has an initial outer Windows backend and green hosted Release
suites on all three platforms, but still requires interactive and remaining M7
matrix gates; WP-38 still requires the independent release audit. Neither
package is Done.

## Optional modern ConPTY implementation evidence

Microsoft's published `Microsoft.Windows.Console.ConPTY` 1.24.260710001
package was downloaded into the VM probe workspace; the package SHA-256 was
`175640566A3B59C4B132070EE96C2C77E5AB7EDD2E92732A5EB3610BBF63D90E`.
Its ARM64 `conpty.dll` SHA-256 was
`DB3D173640B172BAFD42D5B541B638A9AEEC1C7D0E40DD636BF02822A32C912C`.
The package's own deployment layout places `conpty.dll` next to the app and
`OpenConsole.exe` under `arm64/`. An isolated probe using the same adapter
logic but the modern package returned `exit=0`, `rasters=1`, and the expected
text marker. The inbox probe returned `exit=0`, `rasters=0`, and the marker.

D-073 implements app-local modern-runtime discovery in the Windows adapter
without a mandatory external build or runtime dependency. The adapter uses
inbox ConPTY and reports an effective `sixel=false` profile if the modern
runtime is absent. A strict native ARM64 `/W4 /WX` rebuild and the `conpty`
CTest suite passed on the inbox path. The **same rebuilt runner bytes** were
copied beside the optional modern runtime; all nine Windows child-session
cases passed there too. The runner SHA-256 was
`2F4FFA1976547835F55DEA9E02D402F9279891A37221C2FDB379FACD3282AC0C`.
A separate probe linked against the rebuilt project library, with no patched
adapter source, reported `exit=0`, `rasters=1`, and the marker. This proves
that the production adapter selected and used the modern app-local runtime.

These native checks do not yet prove child pixels are confined through a live
outer Windows Terminal scene, nor do they close the nested ckVision, GUI
interaction, three-platform golden, or release-audit rows. WP-50 remains In
progress; WP-37 and WP-38 remain open in dependency order.

## Automated modern-host gate and bounded-close repair

The Windows CI workflow now runs the inbox `conpty` CTest suite and a separate
pinned modern-runtime script (`tools/ci/test_conpty_modern.ps1`). The latter
verifies the official NuGet package SHA-256, copies the **same built test
runner** beside the package's architecture-matched `conpty.dll` and
`OpenConsole.exe`, requires an effective Sixel profile, runs all nine child
session cases, and removes its temporary package workspace. The script was
executed end to end on the named ARM64 VM: nine cases passed and no package
workspace remained. The library was separately probed in three deployment
layouts: modern (`effective_sixel=1`, one raster, exit 0), inbox
(`effective_sixel=0`, no raster, exit 0), and malformed app-local DLL
(launch Failed with `unable to load app-local conpty.dll (Win32 193)`).

The first strengthened inbox suite revealed a real bounded-close problem. A
probe measured a nominal one-second grace taking 14.746 seconds under the VM:
100 requests to `Sleep(10)` were not a duration bound. The Windows adapter now
uses `GetTickCount64` as a monotonic grace deadline before job escalation.
Three direct repeats of the previously failing case passed; the full inbox
suite passed in 4.64 seconds. This changes the transport timing policy, not
the deterministic terminal model.

The strict ARM64 Release rebuild after that fix started at 2026-09-24
07:27:30.045 UTC and ended at 07:28:07.418 UTC (37.373 seconds), with zero
compiler warnings or errors. It recompiled
`src/term/windows_terminal_subsession.cpp` and linked the current runner at
`C:/Users/klukas/ckvision-wp50-20260924a/build/tests/Release/cvision_tests.exe`.
That runner's SHA-256 is
`B3085F8B9FBF2043F20F8C1ACB55F41026FA79F68B161E6F66CCF6B4C470EF50`;
its UTC write time, 07:27:57.916, follows the adapter source at 07:27:03.334
and test source at 07:20:16.496. The fixture executable at 06:45:02.054
follows its unchanged source at 06:44:37.880. The immediately prior native
incremental test-source rebuild took 47.912 seconds; the two builds are
comparable VM compiles, and their logs name the recompiled inputs. Output
identity and timestamp ordering, not duration alone, establish freshness.

## Full hosted matrix follow-up

The current strict Windows Release build includes both the ConPTY child adapter
and the outer VT backend. Its full hosted CTest run passed 163/163 entries,
including the inbox ConPTY suite, after the Windows portability fixes recorded
in [windows-wp37-evidence.md](windows-wp37-evidence.md). The current test runner
was written at 08:37:00 UTC after the corrected spin source and has SHA-256
4FE2055E575D54500E1E878BDA638E49B834101DA7411A10CD8E5B27B8AA01EC.
The macOS and Linux hosted suites each passed 176/176 entries. These runs do
not replace the still-open live Windows Terminal containment, nested ckVision,
Sixel, resource-recovery, and matching-golden acceptance rows.

## Current-source Release refresh

After the Windows Debug Dock resize repair and build portability fixes, the
strict ARM64 Windows Release build was refreshed 10:03:51–10:04:19 UTC. The
complete hosted CTest suite passed **163/163** at 10:04:51–10:05:50 UTC,
including the inbox ConPTY child-session cases. The current runner was written
at 10:04:19 UTC, SHA-256
6E4A7B243980AD708AA92A622678AF57B47D88ED127872A6BC67F54E4FE1BA9B.
The older runner hashes above are preserved as chronological evidence, not
current executable identities. The Mac and Linux current-source Release suites
also passed 176/176 each. Full logs, source matching, and sanitizer details
are in [windows-wp37-evidence.md](windows-wp37-evidence.md). Live Windows
Terminal containment and nested-session acceptance remain open.

## Current private-host and GUI boundary

The refreshed Windows Release runner at 11:04:14 UTC has SHA-256
`D1FB2963210152F18F5B9483A5C261AFBD5F3C7B605184E5F72F45B976FCA2F1`.
Its focused Windows ConPTY suite passed all 13 internal cases, including four
new outer-terminal probe, paste, and resize cases; the full hosted Release
matrix passed 163/163 at 11:05:15–11:06:40 UTC. See the
[WP-37 evidence](windows-wp37-evidence.md) for exact source/output hashes,
build timing, and transfer correction. These private-host cases exercise
bounded protocol behavior, but live Windows Terminal child containment,
nested ckVision, Sixel graphics, and the three-platform golden matrix remain
the next WP-50 acceptance work.

## Redirected-handle follow-up

The current Windows Release runner has SHA-256
`40EC057EAD5E68170BCB449EA8996F058BACDBA9519375F493FF8CF16EC556C0`.
The ConPTY-labeled suite now contains 14 passing cases, adding a redirected
non-console-handle refusal diagnostic to the prior 13. The full hosted
Windows Release matrix passed 163/163 at 11:21:35–11:22:55 UTC. The added
case strengthens WP-37's refusal evidence; WP-50's interactive Windows
Terminal child and graphics acceptance remains open.

## Generated-golden and current Windows build checkpoint

A repaired, fully audited 798-file guest source tree produced a new strict
ARM64 Windows Release build at
`C:\Users\klukas\ckvision-wp50-20260924a\build`. The full hosted suite
passed **164/164**, including the private ConPTY suite and the new
`generated_golden_bytes` CTest gate. Its six generators produced 16 fixture
files whose bytes matched the pinned oracles and the Mac/Linux generated
manifest SHA-256
`03fab0fe422463bdfbcbd11bfce8c08e09ad2d0b5a3cc39734199d2caea52ac4`.
The current runner SHA-256 is
`E171A15407109953063BDBF315803831773C8E049675916BBA82CDF7FA040EEC`;
its 12:40:28 UTC write time follows the current source. Full build timing,
the VM recovery and source audit, and the 177/177 Mac/Linux results are
recorded in [WP-37 evidence](windows-wp37-evidence.md). This bounded symbolic
fixture set advances cross-platform equality; live Windows Terminal child
containment, graphics, and the full WP-50 matrix remain open.

## Native nested ckVision application checkpoint — 2026-09-25

The shared `terminal_nested_child` fixture now builds on Windows as well as
POSIX. Its Windows branch attaches a real `WindowsTerminal` and
`Application` to the child ConPTY, composes a labelled frame with a focused
`InputLine`, changes the label after receiving `go`, and exits normally after
`q`. The new native regression launches that executable through
`Application::launch_terminal_subsession`, places its `TerminalView` at
`{2,2,40,12}` inside an 80×24 headless parent, and sends both text events
through the parent's focused `Application` dispatch. It observes both the
initial `NESTED-CKVISION` and `NESTED-INPUT-OK` frames, compares every parent
cell outside the view against its pre-input baseline, and requires child exit
code zero. This exercises a separately launched nested ckVision app and its
private ConPTY transport; it remains a headless parent-host check.

On the named Windows 11 ARM64 VM, the inbox ConPTY suite passed **15/15**
internal cases, including the nested application. The pinned optional modern
ConPTY runtime passed the same **15/15** with byte-identical copied runner
bytes and its effective Sixel assertion. The full strict MSVC Release hosted
suite passed **164/164** at 06:01:20–06:01:37 UTC. The same shared fixture
passed the Mac POSIX integration suite; the full Mac and Linux Release suites
passed **177/177** each. Linux's build was clean (380 Ninja steps, zero
compiler diagnostics); its full CTest run took 9.42 s. Mac full CTest took
21.21 s; Windows full CTest took 16.64 s. Both provenance gates passed in
the full suites.

The current source SHA-256 values are `A18D2191F31648551985D954CE3A4D5E4684AA58F14378C01D9110247FFA01C3`
for `tests/CMakeLists.txt`, `D39952738A3894027FDC35799F8AA334AF840580166854606D4DE6D4A259A359`
for `tests/terminal_nested_child.cpp`, and
`E288874209D10EA286A795905C72CF06B5040E11AA55D2B3CB770397193BDC24`
for `tests/test_windows_terminal_subsession.cpp`; the guest copies of those
files matched. The final Windows runner is
`C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe`,
written at 06:00:53 UTC after the final test source (06:00:32 UTC), SHA-256
`1488732F9426514844CE42C8C6BF9307A9990A798F93DD831010626AA0A762FD`.
The nested child executable was written at 05:48:44 UTC after its source and
has SHA-256 `F16CC83347715255332227682662A0F4630F8A05DDC9DE39BD33C3616B565919`.
The strict final Windows incremental build ran 06:00:46–06:00:53 UTC
(7.21 s), recompiling the changed Windows test and relinking the runner with
zero warnings. Its short duration reflects reuse of the current library,
fixture, and other test objects. The first full runner refresh after adding
the fixture compiled the changed target graph for 280.36 s but stopped on a
test-only call to `exit_code()` through the base interface; that assertion
was corrected to use `TerminalStatus::exit_code` before the successful build.

The Mac incremental target build ran 05:46:02–05:46:42 UTC, recompiling 177
test objects because the new fixture target path changes a runner compile
definition; its runner SHA-256 is
`3740975F7BD020D709F4D83DFE48041E28FD590800835A510F7C6BFE0585833D`.
Linux configured a fresh source copy at 05:57:41 UTC and completed its
all-target strict build at 05:58:47 UTC (65.85 s, 380/380 Ninja steps). Its
runner SHA-256 is
`F3F30E8318B5B4D3BCDABE4FDafb6f4a8CCDC8E04D7BDE300CACA293D5A8B9B5`.
The earlier clean Linux build took 97.45 s for the same step count; the full
current input list, matched source hashes, and output timestamps establish
freshness despite the different elapsed time. All logs are preserved under
`/Volumes/PRO-BLADE/tmp/ckvision-winwp-20260925.6cmZo5`; the 232 MB Linux
scratch build was removed after validation.

At this earlier checkpoint, Windows `quser` still reported no signed-in
desktop session. Live GUI work and the complete cross-platform cell/pixel
goldens remained open. The next checkpoint below supersedes that GUI status.


## Live Windows Terminal Preview GUI and paste recovery — 2026-09-25

With the owner's explicit approval, the external-drive VM now has a dedicated
standard local `cktest` account. Microsoft's signed ARM64 Sysinternals
Autologon configures it for automatic console sign-in; a restart yielded
`cktest` on active console session 1 and `explorer.exe` in that session.
`AutoAdminLogon=1` names `LIMA\cktest`, and the Winlogon registry has no
plaintext `DefaultPassword` value. The generated account credential was
passed directly to the setup process and was not retained in a Mac temporary
file. This is a dedicated test account; the existing `klukas` account was not
changed. Windows Terminal Preview 1.25.1912.0 was registered in `cktest`'s
interactive session from the official bundle (SHA-256
`7C866975C4CE6B191F02FC0460ED3E31BDE53AA417716992B62164B4B1F13C4E`).
The package-specific `wt.exe` alias launched the actual Preview executable at
`C:\Program Files\WindowsApps\Microsoft.WindowsTerminalPreview_1.25.1912.0_arm64__8wekyb3d8bbwe\WindowsTerminal.exe`.

The first live session ran the hash-verified `ckvision_terminal.exe` in that
Preview window, with the process path resolved to
`C:\Users\cktest\Documents\ckvision-test\bin\ckvision_terminal.exe`.
The guest's actual QEMU display was captured, not an isolated render.
Keyboard `dir` executed inside the contained Windows shell. The same shell
launched the separately built nested ckVision fixture (SHA-256
`F16CC83347715255332227682662A0F4630F8A05DDC9DE39BD33C3616B565919`):
its `NESTED-CKVISION` frame appeared inside the parent window, `go` changed
the label to `NESTED-INPUT-OK`, and `q` returned to the shell. Captures are
`windows-nested-live-initial.jpg`, `windows-nested-live-ack.jpg`, and
`windows-nested-live-exit.jpg` in the external task evidence directory below.

A first Sixel demo child on inbox ConPTY displayed only its PowerShell prompt,
consistent with its effective no-graphics profile. The official pinned
`Microsoft.Windows.Console.ConPTY` 1.24.260710001 ARM64 package (SHA-256
`175640566A3B59C4B132070EE96C2C77E5AB7EDD2E92732A5EB3610BBF63D90E`)
was then staged in the example's app-local layout: `conpty.dll` SHA-256
`DB3D173640B172BAFD42D5B541B638A9AEEC1C7D0E40DD636BF02822A32C912C`,
plus `arm64/OpenConsole.exe`. The next Sixel demo in the same Preview window
rendered the six-color raster inside its child window, with no pixels outside
its boundary in the captured display (`windows-sixel-modern.jpg`). The earlier
fallback capture is `windows-sixel-demo.jpg`.

The live Ctrl+V trial exposed a protocol edge: Terminal Preview emits kitty
release events immediately after a bracketed-paste end marker. The decoder's
D-040 quiet guard had conservatively treated those release bytes as pasted
text, so the contained `cmd.exe` visibly received escape fragments. The
corrected decoder consumes a candidate tail only when every sequence parses
as a kitty key release; a press, ordinary text, or incomplete sequence stays
in sanitized paste recovery. `docs/input-decoder.md` and D-040 now state that
boundary, and two focused regressions cover the observed release stream and
an adjacent press. The intermediate Windows runner SHA-256 was
`956FE71A35995E2A41E7BBE7F56BF69F9FB147B7E91C7042491CEEE28C466715`;
its 78 decoder and 15 inbox ConPTY cases pass. The pinned modern ConPTY suite
also passes 15/15 with that runner. The Windows Release rebuild of the runner
and GUI example ran 06:46:24–06:50:21 UTC (237.55 seconds) from the refreshed
798-file worktree snapshot, recompiling the library and test graph; no
warnings appeared. That intermediate GUI executable SHA-256 was
`B95690B7B971562D5C8BF1C6F39A4AA63367E1687B04789C4308A44EE76353A3`,
written at 06:50:21 UTC after the changed decoder source. The previous GUI
process was closed before this binary was copied and launched.

The intermediate Preview session, process IDs and hashes are recorded in
`windows-live-session.json`: Preview PID 8632 hosted ckVision PID 3232 in
console session 1. A repeat Ctrl+V inserted exactly
`cd Documents\ckvision-test\bin` with no escape suffix; Enter changed the
contained shell's working directory to that path. Captures
`windows-paste-fix-typed.jpg` and `windows-paste-fix-executed.jpg` show the
result. Snapping the real Terminal window to half width and restoring it kept
the contained shell responsive and rendered within the resized scene
(`windows-live-snap-half.jpg`, `windows-live-resize-maximized.jpg`).

The current clean Linux ARM64 build of the same source snapshot completed
380/380 Ninja steps in 96.02 seconds, with zero warnings and 177/177 hosted
CTest suites; the comparable earlier clean builds took 65.85 and 97.45
seconds, and the compiled-input list and fresh output hashes account for the
duration. macOS passed the 78-case decoder suite and full 177/177 hosted
CTest suites. Logs and GUI captures are under
`/Volumes/PRO-BLADE/tmp/ckvision-winwp-20260925.6cmZo5`. The Linux scratch
build was removed after its logs were preserved.

These are live keyboard, paste, nested application, resize, and Sixel
containment checkpoints. WP-50 remains In progress for the remaining
mouse/graphics, simultaneous-session, degraded-host, resource-recovery,
complete three-platform cell/pixel golden, and release-matrix acceptance
rows. WP-37 and M7 remain open; WP-38 follows their acceptance.

## Clean Windows build and verified live Preview checkpoint — 2026-09-25

The refreshed 798-file source snapshot had preserved file modification times.
An incremental `ALL_BUILD` after that extraction exited successfully in 82.34
seconds, yet the full hosted Windows run failed 13 of 164 suites, including
unrelated UI failures and access violations. This was not accepted as a current
build. Before rebuilding, the old `C:\Users\klukas\ckvision-wp50-20260924a\build`
was identified as 168,245,420 bytes with no process running from it; its
Visual Studio 17 2022 ARM64, examples/testing/warnings-as-errors configuration
was preserved. The directory was removed and configured from scratch. Fresh
configure ran 07:09:41–07:10:06 UTC (24.99 seconds); Release `ALL_BUILD` ran
07:10:06–07:21:04 UTC (657.50 seconds). Its 421-line build log has no compiler
or linker warning/error diagnostics. The earlier 237.55-second run was a
targeted incremental build, so it is not a comparable clean duration baseline.
Input hashes and exact output paths/timestamps are in
`windows-clean-inputs.json` and `windows-clean-build-report.json` under the
external evidence directory.

The fresh `cvision_tests.exe` SHA-256 is
`1C634843CF20AB24F99B45C14F5029AC71A894B782F55857AA7BD7E80379CB45`,
written 07:21:03 UTC; the fresh `ckvision_terminal.exe` SHA-256 is
`7EBD965F9163D17B849A68DAC794D40C68691D6BD917EB49C33CA56F8109297E`,
written 07:15:46 UTC after the clean build began. Hosted Release CTest passed
164/164 in 30.14 seconds, including both mandatory provenance gates. The
focused decoder suite passed 78/78; the inbox and hash-pinned modern ConPTY
suites passed 15/15 each from the fresh runner. The 13 incremental failures
therefore disappeared with a clean build; this evidence supports stale object
reuse as their cause, without using that inference to waive a gate.

The prior GUI was closed through its File menu before the fresh example and
fixture were copied into `cktest`'s profile with matching SHA-256 hashes. The
actual VM display then showed Windows Terminal Preview PID 2840 hosting the
fresh ckVision PID 5628, both in console session 1, with the exact executable
paths in `windows-clean-live-session.json`. The captured initial window is
`windows-clean-live-initial.jpg`. A console-session clipboard fixture supplied
`cd Documents\ckvision-test\bin`; Ctrl+V visibly inserted exactly those bytes
without escape suffix (`windows-clean-live-paste-2.jpg`). The freshly built
nested fixture SHA-256
`97161A984C20A451FE348838592D6E9A39E42CED78691110A9CE1B3EF1FBAC1E`
then appeared as `NESTED-CKVISION`, changed to `NESTED-INPUT-OK` on `go`, and
returned to the shell on `q` (`windows-clean-nested-{initial,ack,exit}.jpg`).
The fresh GUI's Sixel Demo 2 rendered its six-color raster within the child
window under the pinned modern ConPTY runtime
(`windows-clean-live-sixel-2.jpg`). These are captures of the real running
Windows Terminal Preview window, not fixture-only renders.

The VM's dedicated standard `cktest` account and LSA-backed autologon remain
available for follow-on live work. WP-50 and WP-37 remain In progress: pixel mouse, complete simultaneous-session
isolation and lifecycle coverage, degraded-host cases, three-platform cell/pixel
goldens, and the remaining release matrix are still unaccepted. The clean build/test logs, JSON records, and live captures
are retained at `/Volumes/PRO-BLADE/tmp/ckvision-winwp-20260925.6cmZo5`.

## Live guest mouse, concurrent sessions, and teardown — 2026-09-25

A follow-on mouse probe resolved a coordinate mismatch in the earlier harness:
the QEMU framebuffer is 800×600, while evidence JPEGs were enlarged to
1280×960. A `cktest` interactive-session task set the pointer to guest pixel
(120,136), identified Windows Terminal as the target window process (PID
2840), and sent a left down/up pair. The real GUI opened ckVision's File
menu (`windows-clean-live-file-click.jpg`). This proves a host mouse event
reached the outer backend and activated a cell-based menu control. A later
click intended for the New Terminal popup did not produce a new session, so
that selection is not counted as a passing mouse case; pixel mouse and other
mouse routes remain open.

Keyboard File → New Terminal opened Terminal 3 while Terminal 1 and Sixel
Demo 2 remained alive. The Session 1 process snapshot contained three
app-local `arm64\OpenConsole.exe` hosts (PIDs 5060, 6056, 9384), two
`cmd.exe` children (2032, 6060), and the Sixel demo's `powershell.exe`
(10056), all parented by the one ckVision PID 5628. The outer Preview
OpenConsole host was separate. Window → Next returned to the undisturbed
Sixel raster (`windows-clean-live-next-session.jpg`). This is a concrete
concurrent-session checkpoint, while per-window input and scrollback
isolation still need their full live matrix.

File → Quit closed the one ckVision instance. A process snapshot during its
bounded teardown showed children leaving; the settled display was the
Windows desktop, and the follow-up process query found no ckVision, Preview,
or private child host/shell process from that run. The same hash-verified GUI
was then relaunched through the `cktest` interactive task as ckVision PID
8680 under Preview PID 4104; the real display again showed a usable Terminal
1 shell (`windows-clean-live-relaunch.jpg`). This verifies application-wide
close and immediate resource reuse for the observed scenario. Individual
window close, forced child failure, and degraded-host recovery remain open.


## VM idle policy, per-window recovery, and live no-graphics fallback — 2026-09-25

The owner approved changing this VM's auto-lock and power-saving behavior.
The dedicated `LIMA\cktest` account now has screen saver, secure screen saver,
and screen-saver timeout disabled. The active VM power scheme has AC and DC
display, sleep, and hibernate timeouts set to never. A reboot verified that
`cktest` signed into active console session 1 automatically with those values
still in force. Winlogon has `AutoAdminLogon=1` and no plaintext
`DefaultPassword`; the existing `klukas` account was not changed. Before,
after, and post-reboot records are `windows-idle-settings-before.json`,
`windows-idle-login-settings.json`, and
`windows-idle-login-reboot-verification.json` under
`/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g`.

In the fresh hash-verified Preview GUI, PID 8996 under Preview PID 3148,
Terminal 1 owned cmd PID 7476 and private app-local OpenConsole PID 7484.
File → New Terminal added cmd PID 6196 and OpenConsole PID 2140. Closing only
that second window removed 6196 and 2140 while the original pair and GUI
remained. The original terminal then accepted `echo surviving` and displayed
`surviving`. Reopening produced a new cmd PID 6412 and OpenConsole PID 1964;
typing `exit 37` displayed `[terminal exited]`, and those two new child
processes departed without ending Terminal 1 or the GUI. The process snapshots
are `windows-children-{before-close,two-terminals,after-close,after-reopen,after-error-exit}.json`;
the relevant real-GUI captures are `windows-after-individual-close.jpg`,
`windows-survivor-after-qmp-focus.jpg`, `windows-reopened-second.jpg`, and
`windows-nonzero-child-exit.jpg`. These prove the observed per-window process
ownership and recovery path; they do not establish the full isolation matrix.

For a real degraded outer host, the verified same GUI binary was launched
directly by `C:\Windows\System32\conhost.exe` for `cktest`, with no Preview
or second ckVision instance. Console Host PID 872 parented ckVision PID 7744.
File → New Sixel Demo started an app-local modern ConPTY host and PowerShell
child, yet the actual Console Host display showed the contained text fallback
`[sixel]` instead of a raster. The visible fallback and process identities are
`windows-consolehost-sixel-fallback.jpg` and
`windows-consolehost-sixel-processes.json`. File → Quit removed the GUI and
its private children; `windows-consolehost-after-quit.json` is empty. The
one-off scheduled tasks were removed, and the exact verified executable was
relaunched in Windows Terminal Preview as PID 1812 under Preview PID 2000;
`windows-preview-restored.json` and `windows-preview-restored.jpg` record its
path, hash, session, and visible Terminal 1. No global default-terminal
setting was changed.

WP-50 remains **In progress**. Live per-window close, child nonzero exit,
reopen, and one Console Host fallback now have evidence. Pixel-mouse routes,
complete input/scrollback isolation, other failure modes, paired cell/pixel
golden equality, and the wider release matrix are still open.


## Isolated private-host failure and fresh allocation — 2026-09-25

From a verified Preview GUI, PID 1812 under Preview PID 2000, Terminal 1
owned cmd PID 7208 and app-local OpenConsole PID 6204. Terminal 2 added cmd
PID 1108 and its distinct private host PID 7424. The host's parent, path,
and session were checked before only PID 7424 was terminated to simulate a
child-host failure. The contained Terminal 2 showed `[terminal exited]`;
its cmd PID 1108 disappeared, while Terminal 1's cmd/host pair and the parent
GUI remained. Terminal 1 then executed `echo host-survived` and displayed the
result. File → New Terminal allocated cmd PID 4748 and new app-local host PID
2608; its real window showed a usable prompt. The process records are
`windows-host-failure-before.json`, `windows-host-failure-two-terminals.json`,
`windows-one-host-failure.json`, and `windows-host-failure-reopened.json`.
The visible sequence is `windows-host-failure-before-kill.jpg`,
`windows-one-host-failure.jpg`,
`windows-host-failure-survivor-command-3.jpg`, and
`windows-host-failure-reopened.jpg`, all in
`/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g`.

File → Quit removed this trial's GUI and private children. The temporary
focus task was removed, and the same verified executable was relaunched as
GUI PID 2904 under Preview PID 2888, with one live Terminal 1 on the actual
VM display (`windows-host-failure-after-quit.json`,
`windows-host-failure-final-relaunch.json`, and
`windows-host-failure-final-relaunch.jpg`). This passes the observed isolated
private-host failure/recovery path. WP-50 remains **In progress** for the
full mouse, isolation, golden, and release matrices.


## Live Windows mouse capability checkpoint — 2026-09-25

The same named VM ran the hash-verified gallery under Windows Terminal Stable.
A real pointer click activated **Greet** and displayed its modal over a
partially occluded Sixel image; dismissing the modal redrew the image cleanly.
A Windows-only outer-host fixture then recorded SGR mouse press/release events
from both Stable 1.24 and Preview 1.25. Each reported cell coordinates and
`pixel_mouse=0`, while Sixel remained enabled. The exact process, click,
capability, build, and 164/164 CTest records are in
[WP-37 evidence](windows-wp37-evidence.md). This passes the observed live
cell-mouse route. The WP-50 pixel-mouse and full pixel-golden rows remain open;
the installed hosts do not currently advertise mode 1016.

## Live two-session input, scrollback, and menu drag — 2026-09-25

The signed-in `LIMA\cktest` desktop kept one hash-verified ckVision GUI
(PID 5744, SHA-256
`7ebd965f9163d17b849a68dac794d40c68691d6bd917eb49c33ca56f8109297e`)
under Windows Terminal Preview 1.25.1912.0 (PID 4592). The original
Terminal 1 owned cmd PID 3788 and private app-local OpenConsole PID 8492.
File → New Terminal opened Terminal 2 with separate cmd PID 3640 and private
OpenConsole PID 2284. The outer Preview OpenConsole PID 5064 was distinct.
`windows-isolation-after-enter.json` records this process tree, and
`windows-isolation-terminal2-real.jpg` shows the second user-facing window.

Real VM keyboard input sent `echo twomarker` to Terminal 2; its output appeared
there. Window → Next revealed Terminal 1's unchanged prompt with no
`twomarker`. After focusing its view, `echo onemarker` appeared only in
Terminal 1. Switching back restored Terminal 2 with `twomarker` and no
`onemarker`. The screenshots are `windows-isolation-two-marker.jpg`,
`windows-isolation-terminal1-unchanged.jpg`,
`windows-isolation-one-marker.jpg`, and
`windows-isolation-two-restored.jpg`.

Terminal 2's `help` output filled its private history. PageUp changed its
visible top rows from the output tail to `REN`/`RENAME` and related earlier
entries. Terminal 1 remained on its own `onemarker` prompt during a window
switch; returning to Terminal 2 restored that same history viewport. Typing
`echo backlive` in Terminal 2 returned it to the live edge and displayed the
result. `windows-isolation-two-help.jpg`,
`windows-isolation-two-pageup.jpg`,
`windows-isolation-one-while-two-scrolled.jpg`,
`windows-isolation-two-scroll-restored.jpg`, and
`windows-isolation-two-backlive.jpg` show the sequence. This passes the
observed two-session input/buffer/scrollback isolation and return-to-live
routes; it does not establish every mouse, paste, alternate-buffer, or
resize-storm combination.

Closing only Terminal 2 removed cmd PID 3640 and private host PID 2284. The
GUI, Terminal 1 cmd PID 3788, and its private host PID 8492 remained; Terminal
1 then executed `echo stillone` visibly. The before/after process records are
`windows-isolation-before-close.json` and
`windows-isolation-after-close.json`; the visible survivor is
`windows-isolation-survivor-final.jpg`.

A separate pointer trial used the documented File-menu
press–drag–release gesture, from guest screen coordinate (70,85) to (120,122).
The `cktest` Session 1 task checked that both points targeted Preview PID 4592
before releasing. It opened Terminal 3 with fresh cmd PID 7484 and private
host PID 7096 (`windows-isolation-after-drag.jpg` and
`windows-isolation-after-drag.json`). Closing that window removed only those
two processes. A separate second click on the already-open popup had closed
it without activating the item; this is not counted as a menu-command pass
because the specified interaction is press–drag–release. The final
`windows-live-isolation-validation.json` records the executable hash, guarded
pointer results, drag result, and surviving process tree. All records and
screenshots are under `/Volumes/PRO-BLADE/tmp/ckvision-nextwp-20260925.wL916g`.
The one-off interactive tasks and scripts were removed after the trial.
The VM settings were rechecked after the contained-golden run: cktest autologon
remains enabled without a plaintext Winlogon password; screen saver is off;
AC/DC display, sleep, and hibernate timeouts remain zero (never). The single
verified GUI remained PID 5744.

WP-50 remains **In progress** for the rest of the named Windows lifecycle,
mouse/paste/alternate-buffer/resize combinations, complete child-raster
containment goldens, and remaining degraded-host rows.

## Contained child pixel-plane checkpoint — 2026-09-25

The portable raw-byte generator now drives a child `TerminalEmulator` through a
real `TerminalView`, retained `Window`, `ApplicationShell`, outer `Presenter`,
and `VirtualDisplay`. Its eight paired cell/RGBA captures cover initial Sixel,
parent move, parent resize, higher-window overlap, child clear, repaint, parent
close through `Window::close()`, and NoGraphics fallback. The generator asserts
that every opaque pixel stays in the child content rect, that no image pixel
survives beneath the higher window's interior, and that clear, close, and
NoGraphics produce zero raster pixels. The NoGraphics cell dump contains
`[sixel]`; its outer output has no Sixel DCS. The decoded plane is 120×72
pixels: 240 opaque pixels at initial/move/resize, 48 after overlap, zero after
clear, 96 after repaint behind the occluder, and zero after close/fallback.

This adds 16 named raw outputs to the three-host `generated_golden_bytes` gate.
The final **45/45** cell/pixel outputs match byte for byte on macOS AppleClang,
Linux ARM64 GCC 14.2, and Windows ARM64 MSVC 2022 `/W4 /WX`. The aggregate
manifest is
`611b95747c089cbe8332afd151d03a2b6cc6b204f24ca78669301984ff633cea`.
The Windows generator source and checker SHA-256 values match the Mac source:
`173a334875c5bcca9f8396bb8e3332a91366e560abd218432dbeb7777f1ff7d6`
and
`967e3deba8b295a63f685b55497ab0511aea8638f75c2f8e677e76df2e433eac`.
The Windows generator was rebuilt from those inputs at 09:52:26–09:52:32 UTC
(6.40 seconds); the executable timestamp is 09:52:32.500 UTC and its SHA-256
is `c23e8831bda6978ec3021d7b23f6bc73fe5a9dd64856ac2eb18f26016fcd540c`.

These captures use the real cross-platform scene/presenter path with a
simulated child session. They establish byte-identical contained composition;
the named live Windows Terminal Sixel and fallback observations are recorded
above. Additional live graphics/input combinations and degraded-host states
remain in WP-50.

## Live alternate buffer during host resize and ConPTY size change — 2026-09-25

Windows Terminal Preview 1.25.1912.0 continued to host the single verified
`ckvision_terminal.exe` (PID 5744, SHA-256
`7ebd965f9163d17b849a68dac794d40c68691d6bd917eb49c33ca56f8109297e`).
Terminal 1's private cmd PID 3788 and app-local OpenConsole PID 8492 were
unchanged. The `windows_terminal_child.exe` copied into `cktest`'s test `bin`
matched the rebuilt guest fixture SHA-256
`e8aa4900acfda6391990c625e9468c748a7e11c5b9cfa42848e9bf6b12c09d8a`.
A guarded, hidden task in interactive session 1 sent keys only after confirming
foreground Preview PID 4592; an initial task that opened its own Terminal
window was rejected by that guard without sending input.

The fixture's `alternate` mode ran as PID 5996 under the private cmd. The
actual VM screenshot `alternate-active.jpg` shows `ALT-SCREEN` confined to
Terminal 1, with the parent menu and frame intact and primary-screen markers
absent. The host window changed from rectangle `(18,26,838,646)` to
`(18,26,718,566)` while PID 5996 stayed alive; `alternate-host-resized.jpg`
shows the alternate screen still contained after redraw. Sending a line caused
`PRIMARY-DONE` and the cmd prompt to reappear. The prior `onemarker` and
`stillone` primary-screen content returned in `alternate-exited.jpg`, proving
that the child alternate buffer did not replace the parent shell's primary
screen. This is one observed live alternate-screen/outer-host-resize route.

A separate live `resize` fixture first reported `SIZE:70x18`. Growing the host
alone left Terminal 1 at 70×18: its default `DesktopGrowPolicy::None` keeps the
window's shrunken geometry. That unchanged pair is not counted as child-resize
proof. The next fixture, PID 7740, again reported `SIZE:70x18`; shrinking the
host from `(18,26,838,646)` to `(18,26,668,496)` visibly clamped Terminal 1.
After input, the fixture reported `SIZE:64x17` and exited to the same cmd
prompt (`size-2-before.jpg`, `size-2-shrunk.jpg`, `size-2-after.jpg`). Thus the
live private ConPTY received a real 70×18 → 64×17 child-size change, while the
parent remained responsive. The outer host rectangle was restored to
`(18,26,838,646)`. As specified by the window grow policy, the contained
window stayed at its clamped size after host growth. Two attempted grip drags
did not visibly change its bounds, so those attempts are not counted as a
mouse-resize pass.

The final process check found exactly one ckVision GUI (PID 5744), its original
cmd child still alive, and zero fixture processes. Guard records, verified
process trees, exact host rectangles, and actual 800×600 VM screenshots are
retained under `/Volumes/PRO-BLADE/tmp/ckvision-live-alt-P4rjDIhZ`. The
one-off interactive task and command files are removed after this checkpoint.
WP-50 remains open for additional alternate-buffer, paste, graphics, mouse,
error, and degraded-host combinations.

## Live nested held-motion checkpoint — 2026-09-25

The Windows-only `windows_terminal_child` fixture now has a bounded
`outer-live-drag` mode that records mouse action, button, and coordinates and
requires a left-button down, held move, and up. Its source SHA-256 is
`bf6c59d382c1b4c29892c29b77b59862b1a9ee9927ad222af7b6286a473f7bb0`;
the identical guest source was rebuilt with MSVC `/W4 /WX` at
10:26:14–10:26:38 UTC (23.97 s), compiling the changed fixture and relinking
the executable with no warnings. The exact Release output is
`C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\windows_terminal_child.exe`,
written at 10:26:37.615 UTC after its source, SHA-256
`65e7cc238eddfda88fa505b4583077ae1f4614c44bf29ee5a03e4101169a94112`.
That verified binary was copied into the signed-in `cktest` session for this
test. The source/output hashes, compiled-input log, and timestamps establish
freshness; elapsed time alone does not.

The same single GUI PID 5744 under Preview PID 4592 launched the fixture inside
Terminal 1's private cmd PID 3788. A guarded interactive-session task checked
the foreground and pointer-target PIDs before sending input. The first input
method set the cursor position in steps and then sent zero-distance move
events. The fixture recorded down and up but no held motion and returned its
intended failure code 107. This established a limitation of that injection
method, not a ckVision motion failure. A second trial sent relative move
events while the left button was held. The fixture then recorded down at
cell (23,9), four held moves at (25,10), (29,11), (33,11), and (37,12), and up
at (37,12). Its final record was `mouse_events=6 down=1 held_move=1 up=1`,
so it exited successfully. The active capability profile was SGR cell mouse
(`mouse_protocol=2`, `pixel_mouse=0`, cell pixels 10×20) with Sixel enabled.
The actual post-run VM screenshot confirms the same ckVision window and
returned cmd prompt. The event record and screenshot are under
`/Volumes/PRO-BLADE/tmp/ckvision-live-mouse-n6MuYtYb`.

This passes one observed live held-motion route through the outer Windows
Terminal, ckVision's `TerminalView`, and a nested `WindowsTerminal` child.
It does not establish a parent-window grip resize or pixel-mouse support;
those remain open.
The one-off scheduled task, guest scratch, copied fixture, and command file
were removed after saving the records. The verified GUI remained PID 5744.
A final settings read showed `cktest` autologon enabled, its screen saver off,
and AC/DC display, standby, and hibernate timeout indices all zero.

## Live mouse-grip resize of the contained ConPTY — 2026-09-25

The same single verified GUI remained `ckvision_terminal.exe` PID 5744 under
Windows Terminal Preview PID 4592, with Terminal 1's cmd PID 3788. The parent
window had stayed at its host-clamped size after the earlier host restore;
its private ConPTY had reported 64×17 cells. The actual VM screenshot
`before.jpg` shows the original frame and cmd prompt before this trial.

A one-off hidden task ran only in the signed-in `LIMA\cktest` interactive
session. It checked that Preview PID 4592 was both foreground and the target
under the bottom-right resize grip at screen point (614,443), and that the
single ckVision GUI was PID 5744. The first script did not produce a result;
an explicit interactive mouse-button release was sent and confirmed before
the corrected drag. The corrected task sent a left-button press, four signed
relative held moves, then a release in a `finally` block. Its recorded pointer
positions were (599,435), (583,426), (567,417), and (551,407); it reported
`Sent=true`, `Released=true`, and no error. The live frame visibly contracted
in `after.jpg` while its top-left corner and the parent menu stayed fixed.

A separately guarded `mode con` command then ran in the same private cmd PID
3788. The actual GUI screenshot `mode.jpg` shows `Lines: 15` and
`Columns: 57`, followed by a responsive cmd prompt. Compared with the prior
64×17 ConPTY observation, this is a real **64×17 → 57×15** child resize from a
mouse drag on the parent window's grip. The output remained inside the new
frame; the rest of the parent scene and Terminal Preview remained intact.
The task reports and before/after screenshots are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-grip-ocaTyOkN`.

This passes the observed live parent-grip mouse-resize route that earlier
attempts did not establish. Resize storms, simultaneous graphics during a
gesture, and true pixel-mouse mode remain open.
The one-off task and guest scripts were removed; the single verified GUI
remained PID 5744 with its original cmd PID 3788.

## Live Sixel clipping during mouse resize and individual close — 2026-09-25

The same single hash-verified GUI PID 5744 remained under Windows Terminal
Preview PID 4592. A guarded File-menu press–drag–release from screen (70,85)
to (120,143) opened `Sixel Demo 4` with PowerShell PID 8256 and a separate
app-local OpenConsole PID 3616; Terminal 1's cmd PID 3788 and private host PID
8492 remained. After its child emitted Sixel, the actual VM screenshot
`sixel-before.jpg` shows the six-color raster inside the new window and over
part of Terminal 1. The menu, frame, and parent scene were intact.

A second interactive-session task checked foreground and target Preview PID
4592, sole GUI PID 5744, and child PID 8256 before pressing the Sixel window's
bottom-right grip at (728,480). It sent ten signed relative left-button moves
and released the button in a `finally` block. The final pointer was (284,480);
the task reported `Sent=true`, `Released=true`, and no error. In
`sixel-after.jpg`, the narrowed right frame cuts the raster, and Terminal 1
is visible immediately to the right. The uncompressed VM screenshots confirm
representative pixels: (100,170) and (250,170) retain their raster RGB values
(219,61,61) and (61,199,89), while (300,170), (350,170), and (400,170) change
from raster colors to black (0,0,0) in the newly exposed area. This is one
observed live Sixel clip during a mouse-driven parent resize, without raster
spill across the new right edge.

A third guarded task clicked that Sixel window's own close control at (87,123).
Five seconds later, both its PowerShell PID 8256 and private OpenConsole PID
3616 were gone; the GUI PID 5744, Terminal 1 cmd PID 3788, and its private
host PID 8492 remained. `sixel-closed.jpg` shows Terminal 1 and its cmd prompt
with the Sixel window and raster removed. Exact task reports, process trees,
raw VM screenshots, and review JPEGs are under
`/Volumes/PRO-BLADE/tmp/ckvision-sixel-resize-USZWDX9d`.

This passes one live Windows Sixel resize/clip/close/resource-reuse sequence.
The other graphics states, resize storms, and complete cross-platform pixel
matrix remain open.
The one-off scheduled task and guest scripts were removed after capture;
the original GUI PID 5744 and cmd PID 3788 remained alive.

## Bounded child-observed ConPTY resize gate — 2026-09-25

A prior loaded four-way Windows CTest run missed `SIZE:57x10` in the ConPTY
resize case because the test sent input immediately after requesting a resize
and the fixture sampled its console size only once. The test fixture now has an
optional expected-size form of `resize`: after reading the input trigger, it
checks `GetConsoleScreenBufferInfo` until the child console itself reports the
requested columns and lines or an eight-second monotonic deadline expires.
It prints the observed size and exits nonzero on timeout. The original
no-argument live fixture behavior remains available. The native test launches
that expected-size form, asserts child exit zero, and still requires the exact
`SIZE:57x10` output. This verifies eventual delivery of the requested
ConPTY extent without relying on the ordering of two unrelated asynchronous
operations. No production adapter behavior changed.

The Mac and guest fixture/test source SHA-256 values matched:
`1f11690869db061a06121d05719eb23d0d2b823ee4843df8cb1e11a212683400`
for `tests/windows_terminal_child.cpp` and
`3135bd101b6306063aecc27960d4178e33cea1dd79cbe6d1ccefbaf9086531a8`
for `tests/test_windows_terminal_subsession.cpp`. The strict MSVC ARM64
Release build at 10:57:54.859–10:58:06.839 UTC took 11.98 s and compiled
both changed files with no warnings or errors. Its log shows the fixture and
runner relinks; the shorter elapsed time than the previous loaded fixture
build reflects reuse of unchanged library/example objects and a lighter VM
load, not a skipped source. The exact outputs, both written after the latest
source at 10:57:22.467 UTC, are:

- `C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\windows_terminal_child.exe`, written 10:57:59.443 UTC, SHA-256 `ac0a187dd2a0a0b58a8af45f80519e1ad41d58e0d097cef9e78862349a374f01`.
- `C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe`, written 10:58:05.657 UTC, SHA-256 `1d9952e9f5b972f08a3ee1cd49ceb62e99acb94b5a0a56de6f8c717efa8b55887`.

The native ConPTY CTest suite passed ten consecutive runs in 26.44 s at
10:58:47–10:59:14 UTC. The complete four-way Windows hosted suite then passed
**164/164** in 17.46 s at 10:59:36–10:59:53 UTC, including the 3.29-second
ConPTY suite, 13.88-second `scene_budget_gate`, `generated_golden_bytes`,
`provenance_check`, and `provenance_checker_self_test`. The original failed
run remains in the record; these checks establish the current gate behavior,
not an unbounded guarantee against extreme VM contention. Build and CTest
logs are retained under `/Volumes/PRO-BLADE/tmp/ckvision-resize-gate-ZdUnbZ4Q`.
The guest scratch and temporary scripts were removed after copying the logs;
the current verified Windows build and single GUI PID 5744 remain.

## Three-host contained resize-sequence pixel goldens — 2026-09-25

The portable `generate_terminal_plane_goldens` now drives a second, separate
child `TerminalEmulator` through a real `TerminalView`, retained `Window`,
`ApplicationShell`, Presenter, and `VirtualDisplay` after the earlier
initial/move/overlap/clear/repaint/close captures. Its five new paired
cell/RGBA states are initial, clipped shrink, move, restore, and a burst of
three bounds changes before one presented frame. Each stage rejects opaque
pixels outside the current child content rectangle and requires an exact
opaque-pixel count: **240, 168, 240, 240, 240**. The restored and coalesced
burst RGBA *and* cell dump match the earlier enlarged state byte for byte.
The original 29 terminal-plane outputs remain byte-identical to their pinned
oracles; ten new files were added. The pinned prior `contained_repaint.rgba`
contains **96** opaque pixels, correcting this document's earlier erroneous
48-pixel statement. The generator now asserts that existing count too.

The full `generated_golden_bytes` gate compares **55/55** outputs: 16 symbolic
scene dumps, 20 raw cell dumps, and 19 complete decoded RGBA planes. All three
hosts reported manifest SHA-256
`704bbc30231e66f8ed562de8bfbde6d798eb9441fe8f88ac2ba65283b39eeec0`.
The current generator and checker source SHA-256 values are
`9f3f91d0fe7539601e6bfbf60a73e2366a507561c98bc4620bf1efd1cd16b246`
and
`6590364555c7ac2bbf194d1d87cadf93f867065c4d611f94cd15f42ac2bdd282`.
A verified 838-file current-worktree source archive had SHA-256
`4e53a3379e76f68a5b7e8eb82205e249d67924ae8fb402c8859f2ab34a44edde`
before transfer to Linux. The guest archive and the two changed source hashes
matched; Windows separately verified its two changed source hashes and ten
new fixtures before compiling.

| Host | Current generator output | Build and full hosted suite | Raw-byte gate |
|---|---|---|---|
| macOS AppleClang | `/Volumes/PRO-BLADE/tmp/ckvision-wp37.JWwvqn/mac-build/tools/docgen/generate_terminal_plane_goldens`, written 11:09:02 UTC, SHA-256 `f798a8eb0d362b0b6f67b8b039ca2c7ef7fbf18f667ae96f050ae13fdabe15f0` | final no-change all-target Release build 11:17:58–11:18:00 UTC; full CTest **177/177** in 18.99 s, zero build diagnostics | **55/55** |
| Linux ARM64 GCC 14.2 | `/var/tmp/ckvision-storm-goldens-20260925/build/tools/docgen/generate_terminal_plane_goldens`, written 11:16:25 UTC, SHA-256 `1d06d836c2c41e245e23b44ef9197056259df8d3ecdbdc85cf4d32637048b4aa` | clean all-target Release build 11:14:30–11:16:27 UTC (117 s, 382/382 actions, zero diagnostics); full CTest **177/177** in 9.83 s | **55/55** |
| Windows 11 ARM64 MSVC 2022 `/W4 /WX` | `C:\Users\klukas\ckvision-wp50-20260924a\build\tools\docgen\Release\generate_terminal_plane_goldens.exe`, written 11:12:37.977 UTC, SHA-256 `7ea685ca178680708d921e5e8dc70a9e51ff1001bc46e3149d1afec914453c42` | strict generator recompile 11:12:31–11:12:38 UTC (6.35 s, zero diagnostics); full four-way CTest **164/164** in 21.73 s | **55/55** |

The Linux clean build took longer than the earlier 68–97 s checkpoints but
its retained log names all 382 compile/link actions, the source archive was
verified, and the generated bytes matched on every host. The Mac final
all-target run reused the already recompiled generator; its compile log and
output timestamp follow the changed source. The Windows build time matches
the preceding comparable generator rebuild (6.40 s). Full build, test, and
byte-comparison logs are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-storm-goldens-XyvuJtE7`. The transferred
archive, intermediate generated trees, Windows guest scratch, and Linux guest
archive/log scratch were removed after preservation. The Linux current-source
build and the current Mac/Windows verified builds remain for the open matrix.

These captures prove the named portable scene and raster transitions. They use
a simulated child session; a real live ConPTY resize *storm*, unrepresented
graphics states, true pixel mouse, and remaining degraded-host matrix rows
remain open.

## Live held Sixel resize sequence and surviving session — 2026-09-25

The signed-in Windows Terminal Preview session kept the same SHA-256-verified
GUI PID 5744 and original Terminal 1 cmd PID 3788/private OpenConsole PID
8492. A guarded File-menu gesture opened `Sixel Demo 5` with PowerShell PID
7768 and its own app-local OpenConsole PID 4132. The VM capture
`sixel-open.jpg` shows the six-color raster inside the new frame.

A guarded interactive-session task checked foreground Preview PID 4592, the
sole GUI PID 5744, and child PID 7768. It held the Sixel frame's bottom-right
grip from screen (728,480), sent eight shrinking moves, eight restoring moves,
then a rapid six-shrink/four-restore/six-shrink sequence with 85 ms between
moves, and released the button in a `finally` block. Its reports record
pointer (376,480) at 11:27:07 UTC, (744,480) at 11:27:15, and final
(376,480) at 11:27:23, with `Sent=true`, `Released=true`, and no error.
The mid-gesture `stage1.jpg` was captured after the restoration stage had
already begun: it shows the held-gesture `[sixel]` fallback in the wide frame
and is **not** evidence of the first shrink stage. In `final.jpg` after
release, the raster returned and was clipped at the narrow right edge. In the
uncompressed VM captures, RGB at (100,170), (250,170), (350,170), and
(375,170) retained the red, green, blue, and purple raster values; (400,170)
changed from purple to black outside the new frame. The parent scene and
original Terminal 1 remained visible and intact.

A guarded `mode con` command in that Sixel child reported **35 columns × 18
lines** in `mode.jpg`, directly confirming the private ConPTY's final size.
Closing only the Sixel frame at (87,123) removed PowerShell PID 7768 and
private OpenConsole PID 4132 while leaving GUI PID 5744, cmd PID 3788, and
private OpenConsole PID 8492. After focusing Terminal 1, `echo
stormsurvivor` printed `stormsurvivor` and returned to its cmd prompt in
`survivor-focused.jpg`. Thus the original live session accepted input after
the graphics child closed. Task reports, process snapshots, raw VM screenshots,
review JPEGs, and cleanup proof are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-live-storm-SaEZtAds`. The one-off scheduled
task and guest scripts/scratch were removed; the single verified GUI remains.

This passes one live held resize sequence with observed final ConPTY size,
post-release Sixel clipping, individual resource closure, and survivor input.
The screenshots do not establish raster behavior at every intermediate move.
WP-50 remains **In progress** for the other named interaction combinations,
full live pixel-golden matrix, pixel-mouse-capable host, degraded-host rows,
and release audit.


## Raster-shadow implementation and live Windows Preview check — 2026-09-25

The parent scene now splits visible RasterSlice spans at higher-window
shadow footprints after body occlusion. A raster pixel is dimmed once where
shadows overlap; Sixel encoding includes the shadow bit in its cache key and
halves the RGB channels while retaining alpha. Three new paired real
TerminalView cell/RGBA states cover shadow, coincident-shadow union, and
unshadowed restoration. Each starts from 240 opaque child pixels; a single
shadow dims 48 while 192 remain ordinary. The union state has identical
cell and RGBA bytes to the single-shadow state, and removing both shadows
restores the earlier raster bytes. The prior overlap and repaint RGBA fixtures
now reflect the required shadow behavior. The Sixel percent-palette
round trip may display a half-intensity channel one level above the integer
source value; exact generated bytes are pinned.

The fresh current-source macOS Release all-target build completed
11:45:13–11:45:30 UTC with zero warnings and passed **177/177** hosted CTest
entries. A separate fresh macOS Debug ASan/UBSan all-target build completed
12:16:45–12:18:36 UTC, then passed **177/177** with both sanitizers set to
halt on error; its disposable build tree was removed after recording the
result. The Linux ARM64 GCC 14 clean all-target build completed
11:49:06–12:13:11 UTC, all 382 actions without warnings, and passed
**177/177**. The unusually long 24-minute Raspberry Pi build was inspected:
the log compiled all 382 actions while the board reported 83°C and
get_throttled=0xf0008; this was not treated as a cache-freshness shortcut.
The Windows 11 ARM64 MSVC /W4 /WX clean all-target build completed
12:12:48–12:24:38 UTC after normalizing ZIP-extracted source timestamps,
without warnings or errors, and passed **164/164** hosted entries. Both
mandatory provenance gates and the generated-golden lane passed on all three
hosts. The **61/61** file manifest had the same SHA-256
6e1d2351bb2cc836f8d9fdb81eda97ed2e25f90018a74b2be124841445fb5814
on macOS, Linux ARM64, and Windows ARM64. Build/test logs and fingerprints
are retained under
/Volumes/PRO-BLADE/tmp/ckvision-shadow-1790336438805.

For the live check, the old GUI exited before the new verified executable
was launched. The only ckVision GUI was PID **6628** under Windows Terminal
Preview PID **6516**, with SHA-256
517F7F05F9FDCCB2677566CC91046C2403D5D666123365AC6424D295DA87EC8A.
Its app-local ConPTY hosts and cmd.exe/Sixel PowerShell children appeared
under that PID. The uncompressed QMP capture new-sixel.ppm shows the
six-color child raster in the actual on-screen Sixel Demo frame. A guarded
interactive-session click opened File (menu-new.json: session 1,
foreground/target 6516, GUI 6628, Sent=true). In
new-sixel-menu.ppm, the menu shadow crossed the yellow raster: pixel
(220,175) changed RGB **(208,208,57) → (104,104,29)**, while an
uncovered raster pixel (240,210) stayed **(171,171,46)**. Escape closed
the menu (escape-menu.json, Sent=true), and
new-sixel-restored.ppm matched the original at every pixel in the
(200,135)–(231,228) shadowed raster sample region. The wider image has
136 changed pixels from the pointer overlay outside that comparison region.
This is direct user-facing evidence of shadow and restoration for this one
live configuration, alongside the portable raw-byte matrix.

The dedicated cktest desktop remains signed in through automatic login.
Its ScreenSaveActive is 0; no machine inactivity timeout is configured;
both AC and DC display, sleep, and hibernate idle indices are 0. The VM and
its disk stay on PRO-BLADE. WP-50 remains **In progress** for true
pixel-mouse-capable host evidence, remaining degraded-input routes, other
live interaction/graphics combinations, and the full release matrix.


## Live Console Host cell-mouse degradation with strict gesture proof — 2026-09-25

The actual Windows Console Host hosted a separately launched Windows-only
terminal fixture in the signed-in cktest desktop. The first fixture stopped
after any two mouse events. Its initial run recorded Move + Down and exited
zero before an Up, so that result is **not** accepted as click evidence. The
fixture now waits for a complete left Down/Up gesture. Its drag mode also
requires held left-button motion before Up; a partial drag can no longer end
the run as a passing gesture.

The corrected current-source fixture SHA-256 is
9A3069523BEC01755B30834D3B7B2EF2CB4382AF1EADC6DD57666AF781AC0B29,
built from source SHA-256
760623B56285268190C50009241D682A664188A7497BFB597A74F3FBDC43822C.
The MSVC ARM64 Release target compiled windows_terminal_child.cpp and linked
the executable 12:52:37–12:52:42 UTC (4.66 s), after the transferred source
timestamp 12:52:36. The prior same-target incremental builds took 8.26 and
23.97 s; the shorter run was checked against the compile/link log, changed
output hash, and write timestamp. The full hosted Windows CTest suite passed
**164/164** in 23.89 s, including the generated-golden and both provenance
gates.

The signed-in fixture PID 4652 was directly parented by system Console Host
PID 1760, with no Windows Terminal process in that lineage. A guarded
interactive-session pointer task verified the fixture hash, parent path,
foreground and target PID 4652, then sent one content click at screen
(260,170). Its report says Sent=true and no error. The fixture reported
SGR cell-mouse protocol 2, pixel_mouse=0, cell_pixels=0×0, and sixel=0.
It recorded two ordinary Move events followed by left Down and left Up at
cell (31,3), each without pixel coordinates, then END down=1 up=1 and
exit code 0. The Console Host window closed and the prior verified
ckVision GUI PID 6628 remained visible and unchanged. The one-off tasks
and guest scratch were removed after preserving the input log, process
record, build/CTest logs, and screenshot under
/Volumes/PRO-BLADE/tmp/ckvision-console-input.jcxR7t.

This passes the observed **Console Host degraded cell-mouse route**. It does
not prove SGR pixel mouse on a capable host or the distinct plain-VT ConPTY
without enhanced input row; WP-50 remains **In progress**.


## Plain-VT ConPTY and Sixel prompt-overwrite correction — 2026-09-25

A separate Windows integration case launches the Windows outer terminal fixture
inside a **real private ConPTY** with `TerminalQueryPolicy::NoResponse`. After
capability-probe expiry the fixture requires legacy keyboard, SGR cell mouse,
no pixel mouse, no cell-pixel metric, and no Sixel. The harness sends SGR left
Down/Up bytes through that ConPTY; the child decodes both and writes
`PLAIN-VT-MOUSE-OK`. This passes the plain-VT/no-enhanced-reply ConPTY
integration row. The input is harness-injected, so this is not a second live
GUI host observation. It does not satisfy the true pixel-mouse row.

The user's captured stripe across the top of the Sixel Demo was caused by the
interactive shell starting at the Sixel origin. Its prompt overwrote cells in
the raster's first row; the presenter drew the invalidated raster pixels black.
The demo now sends a documented VT cursor-position command after its Sixel
bytes, placing the shell on the first row below its 10-cell-high picture
(Windows row 11, POSIX row 13 after a two-row introduction). The POSIX
real-child regression sends a command through the shell, verifies the echoed
marker appears below the raster, and checks every source raster pixel remains
opaque. The public embedded-terminal guide documents the prompt placement.
This repairs the demo scenario; general text-over-raster transparency is a
separate composition concern and is not claimed as closed here.

The updated macOS Release example and runner rebuilt from source hashes
`e13d07a363dda2ac33bb6c2ed5d5bb2c2ba333680a95e5961c5b5b5d6ea2b331`
and `53767c5337ce3b052ac4a8fcd360439a998940a51f792b3bd20ff13a409fe49f`
during 13:08:21–13:08:26 UTC; output SHA-256 values were
`18ec34aa65193ab67fa3315a635a12ad5009a6b160a746f0d822b11694dd7fd0`
and `6171219ed47faf9778b13b2001247b38836185f3aa1dc25eaeaad26c4c7ca7c7`.
Full macOS CTest passed **177/177**. The Linux ARM64 Release all-target
incremental build took 13:10:28–13:10:45 UTC (17 s); its runner and example
hashes were `567f04d1207cc29b63e4568224f83bcd78883ef1d283c846a5e6c28f7454a976`
and `915f8da7a41c034150ed4d07da4e5bc46bf4faa90a59e95a2c09bcfd3ab20bad`.
Full Linux CTest passed **177/177**. Build logs show the changed example/test
inputs compiled and linked. The Windows MSVC ARM64 Release all-target build
took 13:10:06–13:10:22 UTC (16.69 s), after the transferred source timestamps,
and rebuilt the example, runner, and fixture to SHA-256
`031cbc6afb6812074fb2ea9cd5e059452ff56c12a8e53de712e90d64f5df0a79`,
`ba33075ea09f988b4de29b256c2fb38931b5a50decd022f715d74d476e4f4110`,
and `4957f21ad164901f7f909de69d5b7a3254dfdc4607b71a92dbffc0bca311f74d`.
Full hosted Windows CTest passed **164/164** (the new case is inside the
existing `suite_test_windows_terminal_subsession` CTest suite). Both mandatory
provenance gates and the generated-golden gate passed on all three hosts.
The comparable recent incremental build logs were inspected to confirm the
short builds recompiled/copied the changed inputs and wrote new output hashes.

The old GUI PID 6628 exited before launching the fresh, sole GUI PID **5388**
from the verified copied executable under Windows Terminal Preview PID **3872**.
Its executable SHA-256 matched the new Release build. A guarded interactive
cktest task verified PID/hash/foreground ownership before using File → New
Sixel Demo (Sent=true, Released=true). The resulting actual VM capture
`new-sixel.ppm` shows `Sixel Demo 2` with the complete six-color raster and
`PS C:\Users\cktest>` beginning below it. The uncompressed 800×600 capture's
raster interior at x=64–419, y=134–300 contains **zero pure-black pixels**;
the previous user screenshot's first-row black band is absent. The screenshot
SHA-256 is `df673ed8a46a0ed3a3527efbbfae792d5ed559a534e73f88f39659bf425f11e3`.
The sole GUI's direct children included two app-local OpenConsole hosts,
`cmd.exe`, and the Sixel PowerShell. The one-off interactive task and guest
scratch were deleted; the verified GUI remains available for further live
testing. Exact build/CTest logs, live process and gesture reports, and PPM/JPEG
captures are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-plainvt.9rBFuH`.

WP-50 remains **In progress** for a pixel-mouse-capable host, broader live
interaction/degradation matrix, and release gates. WP-37 and WP-38 remain open.


## Concurrent ConPTY graphics ownership and reallocation — 2026-09-25

A new Windows-only real-ConPTY regression starts two private children at the
same time. The red and green fixtures each emit their own Sixel and wait for
input. The test proves different child PIDs and private text/raster snapshots,
closes red, verifies its process is signaled, verifies green still has its
original green raster, sends green a command and observes its exit, then
launches a replacement red child and checks its raster and input. Under inbox
ConPTY the same lifecycle runs with the expected no-raster text fallback;
under the pinned app-local modern ConPTY both child rasters are decoded and
asserted to be RGB (255,0,0) and (0,255,0), each with alpha 255. The new case passes
individually on both hosts; the full modern integration source suite passes
**17/17** cases. The expected lifecycle and containment derive from
[the embedded-terminal delivery target](embedded-terminal-delivery-target.md)
and D-073's declared modern-runtime boundary; the Sixel sample uses this
repository's own fixture and decoder behavior. No outside implementation
source was used. This is Windows adapter/process isolation evidence; it is
not a new live GUI-host interaction or a substitute for the remaining
contained-window pixel matrix.

The source hashes transferred to the VM were
`044b24bd6019d8ba2772c2b56b597463f0a932cf3d58569664fc7795bab9cec7`
for `tests/windows_terminal_child.cpp` and
`541752638834ba48d5d91b7f3cf6c66ac71e78feac92262d69dac7b836758c6d`
for `tests/test_windows_terminal_subsession.cpp`. The strict ARM64 MSVC
Release build of `cvision_tests` and `windows_terminal_child` ran
13:30:53–13:31:10 UTC (17.61 s) with no warnings; its log explicitly
compiles both changed sources and links both outputs. That duration is
comparable to the previous 16.69-second all-target incremental build; the
changed output hashes and post-source write times establish freshness.
The fresh runner SHA-256 is
`4df0515fb6f015097fd6ee3e0e843050e1144e8b8cb3108553db089ef1fd1604`;
the fixture SHA-256 is
`61901b709a0e3fca1375f9de54522eb38838848ae843ed810978bcba7f0c5fdb`.
The modern test copied those **same runner bytes** beside the already pinned
ConPTY DLL SHA-256
`db3d173640b172bafd42d5b541b638a9aeec1c7d0e40dd636bf02822a32c912c`
and modern OpenConsole SHA-256
`ed7622fd0d3bedc9ab9f122f5e58edf0def9e7999224f52dd395ba9f54edbe09`.
The inbox integration suite and the full hosted Windows Release CTest run
passed; the latter was **164/164** in 27.43 s, including generated goldens,
`provenance_check`, and `provenance_checker_self_test`.

The previously verified live GUI PID 5388 remained the sole ckVision app and
was not rebuilt or relaunched for this test-only change. Build, case, suite,
and full-test logs plus source/output fingerprints are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-concurrent-20260925T132905Z`.
WP-50 and WP-37 remain **In progress**; true pixel mouse, further live
interaction combinations, the complete contained pixel matrix, and the
WP-38 release audit remain open.


## Live contained Sixel move with exact pixel translation — 2026-09-25

The sole verified Windows Terminal Preview GUI PID 5388 (executable SHA-256
`031cbc6afb6812074fb2ea9cd5e059452ff56c12a8e53de712e90d64f5df0a79`)
remained under Preview PID 3872, with Sixel PowerShell child PID 788 and its
app-local private OpenConsole. The actual desktop initially showed the full
Sixel Demo 2 image. A guarded cktest Session 1 pointer task shrank only that
contained frame from its bottom-right grip; the uncompressed `resized.ppm`
still showed its image within the narrower frame. A first title-bar attempt
using absolute cursor positioning did not move the frame. Its transient black
content redrew back to the original image without intervention; that attempt
is **not** counted as move acceptance. A second guarded held drag using
relative mouse motion did move the contained frame by **72 screen pixels
right and 38 down**. Its task report recorded the intended foreground and
target Preview PID 3872, GUI PID 5388, PowerShell PID 788, `Sent=true`,
`Released=true`, and no error. Both child and GUI survived afterward.

Exact uncompressed QMP captures give the graphics proof. In `resized.ppm`
(SHA-256 `90da1705335dc23794c8c6a65e72434c4cb73122e63449c07d01b6e37c866609`),
the child raster interior x=66–418, y=138–299 matched every one of its
**57,186 RGB pixels** at x+72, y+38 in `relative-moved.ppm` (SHA-256
`35c2daf14abfe49fc3a0dda38d8bd99dc6821f4e4cf767f3fb2434baf6c59c80`):
zero mismatches. The former image point (90,220) changed from RGB
(125,33,33) to the underlying Terminal 1 black (0,0,0), while its translated
point (162,258) retained RGB (125,33,33). An unaffected parent point
(600,220) stayed black. A wider comparison including y=136 had only four
mouse-pointer-overlaid differences, so the pointer was excluded from the
exact interior proof. The source image, frame, and uncovered parent scene
therefore follow the live move in this one named-host configuration.

The actual before/resized/moved PPMs, preview JPEGs, exact comparison JSON,
guarded gesture reports, and surviving process tree are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-live-move-20260925T133636Z`.
The disposable scheduled task and guest scratch were removed, leaving the
single verified GUI running. This passes the observed **live Sixel move and
parent uncover** route. Other interaction combinations, pixel-mouse-capable
host behavior, the complete WP-37 matrix, and WP-38 audit remain open.


## Live Sixel clear, close, and fresh private-host allocation — 2026-09-25

The sole verified Windows Terminal Preview GUI PID 5388 (executable SHA-256
`031cbc6afb6812074fb2ea9cd5e059452ff56c12a8e53de712e90d64f5df0a79`)
continued under Preview PID 3872. Its moved `Sixel Demo 2` window held
PowerShell PID 788 and app-local OpenConsole PID 5856; Terminal 1 held cmd PID
2168 and separate OpenConsole PID 5880. A guarded `LIMA\cktest` Session 1
task verified foreground and target Preview PID, GUI PID/hash, and the Sixel
child lineage, clicked inside the child, and sent `cls` to its interactive
PowerShell. The action report recorded `Sent=true` and no error. The actual
VM screen then showed the PowerShell prompt at the top of the now-empty
contained window.

The uncompressed 800×600 VM captures give a direct containment check. In the
former raster interior x=135–490, y=210–339, **all 46,280 pixels** changed
from nonblack image colors to RGB (0,0,0); no colored raster pixels remained
in that region. Two regions outside the raster, x=62–120/y=136–299 and
x=521–726/y=136–469, preserved **78,480/78,480 exact RGB pixels**. The
`before.ppm` and `after.ppm` SHA-256 values are
`bce0b66c7990a9de05be697a59e2a82424415c077b3e4119d11b21ba6f447d56`
and `13feb8217faa602ec384c8626d3cb4868eee50500bba6bb4936c13d51190ad41`.
The point and region comparison is in `pixel-comparison.json`.

A second guarded click closed that cleared window. The actual VM capture
`closed.ppm` (SHA-256
`b21df1f87038e86fdf465ae09a055844bdf265cb74c169c53796c44a17e83588`)
showed Terminal 1 and its unchanged cmd prompt. Process inspection confirmed
PowerShell PID 788 and its private OpenConsole PID 5856 were gone, while
Terminal 1's cmd PID 2168 and OpenConsole PID 5880 stayed. A guarded File-menu
pointer gesture then opened `Sixel Demo 3` in the **same** GUI PID 5388, with
new PowerShell PID 1656 and new app-local OpenConsole PID 568. Its actual
`reopened.ppm` (SHA-256
`9ac01a37be86cbab32a04c62e7ec8f58e79c9ddc6ef6f9283349cbd43bdb8170`)
shows the complete raster and prompt below it. Its raster interior
x=65–418/y=170–299 matches **46,020/46,020 exact RGB pixels** of the prior
known-good live Windows capture `new-sixel.ppm` (SHA-256
`df673ed8a46a0ed3a3527efbbfae792d5ed559a534e73f88f39659bf425f11e3`);
`reopen-pixel-comparison.json` records the check.

The one-off task and guest scratch were removed. `cleanup.json` confirms the
sole GUI and both live children/hosts, with the old child/host absent. All
reports and captures are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-live-clear-20260925T143000Z`. This passes
the observed **live child clear → close → fresh Sixel allocation** route. The
remaining WP-50/WP-37 interaction and degraded-host rows are still open.


## Live partial Sixel repaint and global pixel sampling — 2026-09-25

A contained Sixel image partly exposed by shrinking an overlapping Terminal
window retained one-pixel-wide color-boundary seams after repaint. In the
former verified GUI PID 5388, the stable overlap capture differed from the
baseline at **204 of 36,640** pixels in the far exposed region
x=190–418/y=140–299. The covering window changed all 1,800 sampled covered
pixels; closing it restored **58,764/58,764** raster pixels and
**68,804/68,804** sampled pixels to the right of the raster exactly. Old captures and comparisons
are retained under /Volumes/PRO-BLADE/tmp/ckvision-live-occlusion-20260925.

The compositor's slice geometry was correct. Presenter::emit_raster_slices
rounded each partial source crop to image pixels and scaled that fragment
independently. With a fractional image-pixel-to-cell ratio, this samples a
neighboring source pixel at a crop boundary. The presenter now samples every
visible target pixel in the full image anchor's coordinate system, so a
partial repaint uses the same source pixel as the unoccluded image. This
follows the scene's full-anchor/visible-rectangle contract. No behavior was
taken from another implementation. The decoded 10×10 Sixel gradient test
occlusion_crop_preserves_full_image_pixel_sampling compares an 8×8 visible
portion to the full image's decoded raster. It failed before the correction
(mismatches == 0) and passed afterward.

The current source SHA-256 values are: presenter.cpp
F773346A726E751DCF53ACB4E0BAFDD1BF9126EFF34BE6B91129CE6713EB8BCD;
test_presenter.cpp
8F582C103018588E5D0AE9DFD430E848C87A67FC4F1E1A64A3FF5551463C022F;
and test_posix_terminal_subsession.cpp
5133FA6E2E0AE72625A4756F0ED714339F279BCF4F8E7A41F60AD4B282A2381B.
The last change bounds child-exit polling by five seconds of elapsed time;
a fixed count of instantly ready PTY polls could finish before child exit.
The Linux 34-case POSIX suite and subsequent full run passed.

| Host | Release build and freshness | Full CTest |
|---|---|---|
| macOS | All-target incremental build 14:41:24–14:41:27 UTC (3.162 s); changed presenter and test objects compiled in the preceding build, then current runner SHA-256 183163383B47668033CF68A57EF09765B7FA1C04A52FD0C5C74168A29B67E6B2 | **177/177**, 19.402 s; provenance and generated goldens passed |
| Linux rpi4 | All-target build 14:31:57–14:32:30 UTC (33 s, 49 actions including presenter/test compilation and dependent relinks); final POSIX test relink 14:40 UTC, runner SHA-256 A7219B2F0D8F34F30818187DCDAB08F51ED636C6122A99179A7AE6A1CB3C6627 | **177/177**, 29.75 s; provenance and generated goldens passed |
| Windows ARM64 VM | All-target native build 14:43:10–14:43:39 UTC (28.74 s); log compiles presenter/test and relinks example and runner after source transfer; example SHA-256 0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1, runner SHA-256 16BD0466A2A983A568A7FFC74FB2520E1984221634CEE41D771EA2CD992718AB | **164/164**, 33.73 s; provenance and generated goldens passed |

The Linux 33-second build exceeded the earlier roughly 17-second incremental
runs because a library rebuild relinked dozens of dependent targets; its 49-action
log confirms the intended inputs and outputs. The Windows 28.74-second
build compiled the changed presenter and test and relinked their dependents.
Elapsed time alone is not freshness proof. All three logs show successful
builds without compiler warnings. Mac, Linux, and Windows build/CTest logs
and metadata, red/green regression output, and live images/reports are
retained under /Volumes/PRO-BLADE/tmp/ckvision-crop-seam-20260925.

Before starting the corrected GUI, PID 5388 and its private children exited.
The exact newly built executable was copied to the cktest Documents
ckvision-test/bin directory with the SHA-256 above and launched under
Windows Terminal Preview. The sole GUI PID 5828, Preview PID 7560, and its
actual window were inspected. A real File-menu gesture opened Sixel Demo 2
(PowerShell PID 1976, private OpenConsole PID 8500), then Terminal 3
(cmd PID 1020, private OpenConsole PID 3876) over it. A real bottom-right
grip resize exposed the raster. Against the pointer-free baseline capture
SHA-256 DEADDBBB476A23AD9592F48DAADF731D3CF8C858FB8745D8D8F0A3176377AC82,
the overlap capture SHA-256
ECABB09EA27E8117EC783827CB977B769314AA79468499F9D1C98AEA7F42BDC6
had **0 mismatches across 36,640** exposed far-region RGB pixels
x=216–444/y=166–325. All 1,800 sampled pixels under the covering window
changed, and 375 of 875 sampled near-shadow pixels changed as expected.
Closing Terminal 3 removed its cmd and private host while Sixel Demo 2
and Terminal 1 survived. The restored raster matched **59,640/59,640**
pixels and right-of-picture child content **89,846/89,846** pixels; the restored PPM
SHA-256 is
FB0D6FE890430070D7B077C7DD0ACF18F78B59BD34C0BB6F28AD4FF60F2557A9.
The exact rectangles, guarded UI actions, process tree, and captures are in
overlap-comparison.json, restoration-comparison.json, and other retained
files in the same evidence directory. The one-off Windows task and guest
UI scratch were removed; the sole corrected GUI remains running.
This passes the observed live partial-raster repaint, occlusion, shadow,
close, and exact restoration route. WP-50 and WP-37 remain open for the
remaining live matrix, including true pixel mouse; WP-38 audit follows.


## Live held-resize raster policy, exact recovery, and survivor input — 2026-09-25

The sole verified Windows Terminal Preview GUI PID 5828, executable SHA-256
0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1,
was still running in the signed-in cktest desktop with Sixel Demo 2.
A one-off interactive Session 1 task checked foreground and hit-test host PID
7560, GUI lineage/hash, PowerShell child PID 1976, and its app-local
OpenConsole PID 8500 before sending a held bottom-right resize grip gesture.
It moved the pointer from (754,506) to (654,458) in two shrink steps, back
to (753,505) in two restore steps, then sent 16 rapid alternating moves
before release. Phase reports and four actual VM framebuffer captures were
taken while the button remained down.

The shrunken and restored-held captures show the child window's [sixel]
cell fallback instead of raster pixels. This is the intended Window gesture
policy documented in docs/graphics.md: it pauses all window rasters during
a move or resize to avoid making the outer host decode an image at every
pointer position. The retained child Sixel state is not cleared. The
restore-held and burst-held PPMs have identical SHA-256
A558D99D9F3653DF666F9788B6E71449BB4CDE12420568574ABDD26BA45DF6C3,
showing no visible difference between those two held states. The shrunken capture SHA-256 is
A51F711783C15DB93DA17A4844C1D613724558D2394C4088600C364F9C99F27F.

After button release, the image returned in the same GUI and private child.
Against the pre-gesture 800×600 capture SHA-256
3A38949547AA9F994BDFE98510FFE5AFBBF612B5041E7D24CC2AEB5C168F8113,
the released image interior x=90–444/y=161–328 matched **59,640/59,640**
RGB pixels. Right-of-picture child content x=471–739/y=166–499
matched **89,846/89,846**. The released PPM SHA-256 is
2FFF6C42968B67F4EA957DB65A6F4F9F3B8230932A56B5E7606D17EB3CEF6025.
While the grip was held, all 59,640 image-interior pixels differed from
the colorful baseline as the cell fallback appeared. This is direct live
evidence of the documented pause-then-redraw contract, rather than a
claim that every intermediate frame retained a visible raster.

A second guarded Session 1 action clicked the surviving Sixel child and
typed echo stormok into its interactive PowerShell. The actual screen showed
the command, output stormok, and a fresh prompt below the restored image.
The post-command PPM SHA-256
0958F62248D9B813A7B697A0DE7CF8FA8989B9C18F29E6770F10ABE650AE41E5
still matched **59,640/59,640** image pixels and **89,846/89,846**
right-of-picture child-content pixels against the original
pre-gesture capture. The final process inspection found exactly one GUI PID 5828, original cmd PID 8996
and private OpenConsole PID 1608, and the same Sixel PowerShell PID 1976
and private OpenConsole PID 8500. The one-off scheduled task and both guest
scratch directories were removed; the verified GUI and both children remain.

The uncompressed PPMs, preview JPEGs, phase/guard/action reports, exact
comparison JSON, and cleanup report are retained under
/Volumes/PRO-BLADE/tmp/ckvision-live-resize-storm-20260925.
This passes the observed live held-resize fallback, exact restoration,
and child-survivor input route. WP-50 and WP-37 remain open for the
remaining cell/pixel and degraded-host matrix, true pixel-mouse-capable
host behavior, and release gates.

## Native Windows pixel-mouse ConPTY path — 2026-09-25

The Windows-only private-ConPTY integration suite now launches a real
WindowsTerminal in its child fixture and exercises its verified pixel-mouse
path end to end. The test supplies published DECRPM mode-1016 and XTWINOPS
cell-size replies through the private ConPTY, in reverse order. The child
requires SGR mouse capability, active pixel mode, and a 10×20 cell-pixel
metric before accepting input. It then requires a left-button down at pixel
30,4, movement and release at pixel 31,4, with all three events also mapped
to cell 3,0. The injected SGR coordinates fit inside the 48×8 cell grid so
ordinary cell-mode parsing cannot accidentally satisfy the assertion. The
exact case passed, as did the focused Windows suite and full 164/164 Release
CTest run, including both provenance gates and generated-golden bytes.

The strict ARM64 Release all-target build ran 15:23:58–15:24:21 UTC
(22.78 seconds) in the existing guest build. Its log names both changed test
sources as compiled and both output targets as linked, with no warnings. The
new runner was written at 15:24:20 UTC with SHA-256
FDEE89F940658D576A5013A6B75E4C1DF57500A3EBF95D3536AF3B8D5F8922B0;
the child fixture was written at 15:24:09 UTC with SHA-256
64C34AD3F4BA04CBA155A97A0AF9339E77B8BB6D0E163515A74006DD9259DA78.
Both follow their transferred source timestamps, and both hashes changed
from the prior outputs. The 22.78-second duration is comparable to the
recent 16–28-second native incremental builds; the compile log and output
identity, rather than elapsed time, establish freshness. The full CTest ran
15:27:36–15:28:18 UTC (41.95 seconds), exit code zero. Build, focused,
exact-case, and full-test logs and JSON are retained under
/Volumes/PRO-BLADE/tmp/ckvision-pixel-conpty-20260925.

This proves ckVision's native Windows pixel-coordinate conversion when a
private host supplies the required protocol evidence. The installed Windows
Terminal Stable and Preview hosts have not advertised mode 1016 in the live
VM probes; Microsoft's mode-1016 feature request remains open:
https://github.com/microsoft/terminal/issues/18591
This test therefore does not close the live Windows Terminal pixel-mouse
interaction or the remaining WP-50/WP-37 matrix rows.

## Abrupt Windows child loss with surviving graphics peer — 2026-09-25

The Windows private-ConPTY integration suite now exercises a child that exits
without running C++ destructors or terminal cleanup. Two independent children
first produce red and green Sixel pictures under the optional modern ConPTY
runtime. The red child then calls the documented Win32 TerminateProcess API
on itself with exit status 37. The parent observes that exact status and a
signaled process handle; the green peer retains its own raster and accepts
new input. Closing the exited session leaves no native wait sources. A new
red child allocates a separate private ConPTY, produces the expected raster,
and accepts input while the green peer remains active. The same case passes
on inbox ConPTY, where both sessions have the intended no-raster fallback.
This is controlled abrupt termination, not a test of a hardware fault or
Windows Error Reporting.

The final native ARM64 all-target Release build ran
15:39:23–15:39:37 UTC (13.48 seconds) in the current guest build. The log
explicitly compiles tests/windows_terminal_child.cpp and
tests/test_windows_terminal_subsession.cpp and links both test outputs,
without compiler warnings. Their source SHA-256 values on Mac and guest are
34439A6D60BE8CC37D2D974C46A5D0244F59A36951761CBCBA2BD0092898C5B7
and
4795A04829D4585AB3E5DB0E13540D323616F07F557FF70E4D8E2B1B9C2262B0.
The new child fixture was written at 15:39:29 UTC with SHA-256
ED50DEAB31F47B26966EA7770F1452FE9C0C83388F5F8F270BB14E6319E3A417;
the runner followed at 15:39:36 UTC with SHA-256
74FC3A8DF475A1E7EAC20BA4F988C46CA13848082A90BB3862C88D66C4DA1CFF.
Both outputs follow the transferred source and build start, differ from
their previous bytes, and the log names the compiled inputs. The shorter
duration than the previous 19.5-second two-source build is not used as
freshness evidence by itself.

The exact new case passed. The pinned official modern-ConPTY gate used a
byte-identical copy of that runner and passed all 19 Windows integration
cases in 18.43 seconds, including the live decoded red/green raster checks.
The full hosted Windows CTest passed 164/164 at 15:40:35–15:40:59 UTC
(24.68 seconds), including the inbox ConPTY suite, generated goldens,
provenance_check, and provenance_checker_self_test. Build, source/output
hashes, exact-case, modern-runtime, and full-test logs and metadata are
retained under /Volumes/PRO-BLADE/tmp/ckvision-conpty-crash-20260925.
The sole Windows Terminal Preview GUI remained PID 5828 with unchanged
executable SHA-256
0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1;
this test did not launch or inspect a new GUI frame.

The controlled abrupt-loss, peer-survival, and replacement route advances
WP-50's resource-recovery matrix. Actual fault injection, live GUI crash
recovery, true pixel-mouse interaction, and the remaining degraded-host and
cross-platform scene combinations still require acceptance.

## Live Preview child loss, raster retention, close, and recovery — 2026-09-25

The sole signed-in Windows Terminal Preview GUI remained PID 5828 under
Preview PID 7560, executing
C:\Users\cktest\Documents\ckvision-test\bin\ckvision_terminal.exe with
SHA-256
0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1.
Before the action, Sixel Demo 2 used PowerShell PID 1976 and app-local
OpenConsole PID 8500; Terminal 1 used cmd PID 8996 and private OpenConsole
PID 1608. The actual 800×600 VM framebuffer was captured before any change.
A guarded Stop-Process -Force terminated only PowerShell PID 1976 at
15:45:07 UTC. The parent GUI and Terminal 1 remained alive; the dead
PowerShell and its private OpenConsole exited. The actual GUI frame added
[terminal exited] below the Sixel image.

The raw PPM comparison shows that all 59,640 RGB pixels inside the existing
image rectangle x=90–444/y=161–328 remained identical after that abrupt
child loss. All 89,846 sampled pixels in the right-of-picture child
content x=471–739/y=166–499 also remained identical. Retaining the exited view's
last raster is the observed behavior; this is not evidence that a child
process still owns the raster. A guarded Session 1 click at (105,149)
closed the exited window. Terminal 1 became visible, and all 59,640
sampled pixels in the former image rectangle changed. In the
right-of-picture sample, 173 pixels changed at x=471–506/y=166–191 as
underlying Terminal 1 banner text was exposed; the other 89,673 sampled pixels stayed
unchanged.

The same verified GUI's File menu was opened by a real pointer click and
its New Sixel Demo item selected. Fresh PowerShell PID 4160 and app-local
OpenConsole PID 6676 appeared under GUI PID 5828; the original cmd and
private host survived. The new Sixel Demo 4 produced an image matching
the pre-termination raster at **59,640/59,640** sampled RGB pixels, with
the right-of-picture content at **89,846/89,846**. Through the real
Window → Next menu, Terminal 1 returned to the front. Its original cmd PID 8996 accepted
a new echo livepeer command, printed livepeer, and showed a fresh prompt.
Window → Next returned Sixel Demo 4 to the front; the final frame still
matched the original image and right-of-picture content at those
same exact counts. Both routes preserved **23,120/23,120** pixels in
three desktop strips outside the child window; the strip coordinates and
per-phase counts are in comparison.json.

The five uncompressed framebuffer captures (baseline, terminated, closed,
reopened, final), JPEG previews, exact comparison.json, guarded Session 1
action reports, process lineage, and cleanup result are retained under
/Volumes/PRO-BLADE/tmp/ckvision-live-abrupt-20260925. The capture SHA-256
values are listed in comparison.json. The one-off scheduled task and guest
scratch were removed. The final process tree had exactly the original
cmd/OpenConsole pair and new PowerShell/OpenConsole pair under the same
GUI PID 5828; the old PowerShell and host were absent. This passes the
observed live forced-child-loss, retained-last-frame, individual close,
fresh graphics allocation, and surviving peer-input route. The
subsequent FailFast test below covers one fatal runtime path. The live
pixel-mouse and remaining degraded-host matrix remain separate acceptance work.


## Live Preview fatal-runtime child recovery — 2026-09-25

The sole signed-in Windows Terminal Preview GUI stayed PID 5828 under Preview
PID 7560, running the verified
C:\Users\cktest\Documents\ckvision-test\bin\ckvision_terminal.exe
(SHA-256 0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1).
Sixel Demo 4 used PowerShell PID 4160 and private OpenConsole PID 6676;
Terminal 1 used cmd PID 8996 and private OpenConsole PID 1608. A guarded
Session 1 action pasted
[Environment]::FailFast("CKVISION_LIVE_FAULT_20260925")
into the interactive Sixel child, restored the prior VM clipboard, and then
pressed Enter. This deliberately exercised a fatal .NET runtime path in the
actual child process. Microsoft's [FailFast reference](https://learn.microsoft.com/en-us/dotnet/api/system.environment.failfast?view=netframework-4.8.1)
describes immediate process termination without normal cleanup.

Windows Application events establish the fault mode: .NET Runtime Event ID
1025 at 16:05:06.374 UTC recorded the exact FailFast call and marker;
Application Error Event ID 1000 at 16:05:06.650 UTC named PowerShell PID
0x1040 (4160) with exception code 0x80131623; Windows Error Reporting
Event ID 1001 at 16:05:09.586 UTC recorded a CLR20r3 FatalError report.
The guarded action observed PID 4160 exit but did not obtain a numeric
process exit code. Its private OpenConsole PID 6676 also exited. The GUI
and cmd peer stayed alive. The actual 800×600 framebuffer displayed
[terminal exited] while preserving the previous Sixel image at
**59,640/59,640** exact RGB pixels (x=90–444/y=161–328).

A guarded click closed the exited child. The existing Terminal 1 was
uncovered. A real File-menu New Sixel Demo gesture allocated PowerShell
PID 7788 and private OpenConsole PID 3428 under the same GUI. The new image
matched the baseline at **59,640/59,640** pixels. Window → Next brought
the original cmd forward; it accepted echo faultpeer, printed faultpeer,
and returned to a prompt. Window → Next restored Sixel Demo 5, whose
image again matched the baseline at **59,640/59,640** pixels. The
right-of-picture content inside the child window matched **89,846/89,846**
pixels in the reopened and final frames. Three genuine desktop strips
outside the child window (x=60–75/y=160–499, x=785–798/y=160–499,
x=80–759/y=521–539) matched **23,120/23,120** pixels in every phase,
including faulted, closed, reopened, and final.

The six raw VM framebuffers, per-frame SHA-256 hashes, comparison code and
JSON, Windows event records, guarded UI reports, process lineage, and
cleanup report are retained in
/Volumes/PRO-BLADE/tmp/ckvision-live-failfast-20260925. The one-off
scheduled task and guest scratch were removed; the VM clipboard was
restored. The final sole GUI still had exactly the original cmd/OpenConsole
pair and the new PowerShell/OpenConsole pair. This passes the observed
**live fatal-runtime child → retained frame → close → replacement allocation
→ surviving peer input** route. It does not cover every crash mechanism;
true live pixel mouse and the remaining host/scene matrix keep WP-50 open.


## Native ConPTY Sixel payload-limit recovery with a graphics peer — 2026-09-25

A new Windows child fixture deliberately sends a 512-byte Sixel payload
through its private ConPTY while its session is configured with a 128-byte
graphics-payload limit. In the pinned modern runtime, the contained
terminal reports the configured Sixel limit, rejects the oversized picture
without retaining a raster, then parses ordinary text and a following
valid red Sixel image from that same child. A concurrent private child
keeps its green raster throughout and accepts peer-ok input after the
limited child exits. The inbox ConPTY exercises the same lifecycle with
its declared text fallback; it has no raster, and both children still
accept input and exit normally. This checks resource-limit recovery across
the real Windows transport, not only the isolated parser.

The changed sources are tests/windows_terminal_child.cpp (SHA-256
72E2B7A4796F6EC726DF78A5FBBF80E3BC55022F0A5DCD5E852D05651C238120)
and tests/test_windows_terminal_subsession.cpp (SHA-256
241C5DECD17EC70C00D7DEB2B2F0FF316D17F3ED61CBF942D16ED91971609233).
The strict ARM64 Release all-target build ran from 16:26:19.199 to
16:26:29.772 UTC (10.57 s), compared with the preceding comparable
13.48 s build. Its log shows both changed sources compiled and both
test executables linked after the source timestamps, with no compiler
warnings. The output runner is
C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe
(SHA-256 1708F2C6A3DAEDE7CCEADE54A72F07E7DECF5B165A4E360518A978419137FE30);
the child fixture executable has SHA-256
43AF8436BE75B6144A30C728574E0180408058A9034E933A6567CAE204C59BC0.
The exact inbox case passes, the pinned modern ConPTY suite passes
**20/20**, and the full hosted Windows ARM64 CTest matrix passes
**164/164** in 16.99 s, including documentation and provenance checks.
Source/output fingerprints, UTC build times, exact-case, modern-suite,
and full-suite logs/results are retained under
/Volumes/PRO-BLADE/tmp/ckvision-wp50-limit-20260925. The guest
staging directory was removed after copying those records; cleanup.json
confirms it is absent and identifies the remaining GUI and children.

The signed-in user-facing ckVision GUI remained the sole PID 5828 during
this runner-only build and validation; its executable hash stayed
0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1.
This adds native resource-limit and peer-isolation evidence to WP-50.
A live user-facing resource-limit injection, true pixel mouse on a capable
outer host, and the remaining scenario matrix remain open.


## Live Preview Sixel payload-limit recovery and normal-demo restoration — 2026-09-25

The sole signed-in Windows Terminal Preview GUI remained PID 5828 under
Preview PID 7560, running
C:\Users\cktest\Documents\ckvision-test\bin\ckvision_terminal.exe
(SHA-256 0151BC49C6A52A0158DC90B30E555072BB686DCDCE55F9791BD8C8F771ABFBF1).
Its active Sixel Demo 5 used PowerShell PID 7788 and private OpenConsole
PID 3428; Terminal 1 retained cmd PID 8996 and private OpenConsole PID
1608. A guarded Session 1 task pasted an exact PowerShell script invocation
into Demo 5's interactive prompt and restored the prior VM clipboard.
The before-execution framebuffer shows the pasted command in that window.

The script's child PowerShell PID 7804 cleared its own terminal, then wrote
a Sixel DCS with **4,202,496 payload body bytes** plus a 15-byte head.
This exceeds the public subsession's default **4,194,304-byte** graphics
payload limit. It wrote ordinary text after the DCS and waited for a
file handshake. The actual 800×600 VM framebuffer captured at that
handshake showed LIVE-LIMIT-START and LIMIT-RETURN, no visible raster,
and the visible diagnostic
[terminal: child Sixel payload exceeded configured limit].
The rejected frame contained zero pure-red pixels in the later red-image
sample and differed from the original colorful raster at all
**59,640/59,640** sampled interior pixels (x=90–444/y=161–328).
This is a deliberately injected boundary condition, not an arbitrary
external application crash.

After the rejected frame was saved, the handshake released the same
script to send a valid one-column red Sixel and VALID-RED text.
The actual recovered framebuffer showed that image, its marker, and a
fresh interactive PowerShell prompt. A 30×100 sample at x=80–109/y=160–259
contained **171 pure-red pixels**, versus zero in the rejected frame.
Window → Next then brought the original cmd to the front; it accepted
echo limitpeer, printed limitpeer, and returned to a prompt. Returning
to Sixel Demo 5 preserved the red raster at the same 171 sampled pixels.
Three genuine outside-window desktop strips
(x=60–75/y=160–499, x=785–798/y=160–499,
x=80–759/y=521–539) matched the original framebuffer at
**23,120/23,120 exact RGB pixels in every captured phase**.

A guarded click closed only the tested Sixel window, removing PowerShell
PID 7788 and its private OpenConsole PID 3428 while cmd PID 8996 and its
host survived. The same GUI's File menu allocated Sixel Demo 6 with fresh
PowerShell PID 1568 and private OpenConsole PID 5672. Its normal
multicolor raster matched the original Demo 5 at **59,640/59,640**
interior pixels; the right-of-picture child content matched
**89,846/89,846** pixels. The actual final framebuffer was visually
inspected. The one-off scheduled task, guest staging, and cktest scratch
were removed; cleanup.json confirms the sole GUI, original cmd pair,
new Sixel pair, and restored clipboard.

Raw PPMs and JPEG previews for baseline, pasted command, rejected
payload, valid red image, peer input, returned red image, close, and
final normal demo are retained with comparison.py/comparison.json,
phase/payload/action reports, and cleanup.json under
/Volumes/PRO-BLADE/tmp/ckvision-live-limit-20260925. This passes the
observed **live over-limit Sixel → parser recovery → valid image →
surviving peer input → individual close → fresh graphics allocation**
route. It samples specific outside-window strips; the complete
cell/pixel scene matrix and true pixel mouse on a capable host remain
open for WP-50/WP-37.

## Windows ConPTY input-transport failure visibility — 2026-09-25

Source review found that `WindowsTerminalSubsession::pump_write()` closed its
input pipe after a failed `WriteFile` or overlapped completion without changing
the session's `Ready` state or reporting the Win32 error. The adapter now
clears the unwritable queues, closes that channel, and marks the individual
session `Failed` with a diagnostic when its root child is still running. A
child that has already exited keeps the normal exit path. This is a WP-50
transport robustness correction, not a claim of live host fault injection.
A proposed `FreeConsole` fixture did not break the ConPTY input pipe and was
removed rather than counted as coverage for this condition.

The changed adapter source and header SHA-256 values are
2824B3701C38A1A2837E15872BED5CBF71A06AF78DA2E5783C689D53E5999C4F
and 615383EE1F309ACB5AA33497F11ABA494CCB1B339AF82FD09D531A67C5DE030E.
The external-drive ARM64 Release build recompiled `windows_terminal_subsession.cpp`
and linked `cvision.lib` at 17:09:54 UTC (SHA-256
F9E45D0E0057B5CDCAC354AA16AA0DD6BB318584DFA5018DC9D0CA219B298261),
then linked the test runner at 17:12:07 UTC (SHA-256
796E5927D3CBF159CAF6EF3E208A23911261AEE440B39AB66D417C19F6E75D9E).
The all-target build completed with exit code 0 and no warnings after
MSBuild node reuse was disabled. The inbox ConPTY suite passed **20/20**;
the official hash-pinned modern ConPTY suite passed **20/20** from the same
runner bytes. Hosted Windows CTest passed **164/164** in 23.24 seconds,
including `provenance_check` and `provenance_checker_self_test`.
Build/test records and logs are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-wp50-input-failure-20260925`; the VM's
one-off task scratch was removed after copying them. No ckVision GUI or
fixture remained running. The VM now runs headlessly to leave the Mac desktop free. WP-50
remains open for true pixel-mouse host evidence and the remaining specified
matrix; WP-37 and WP-38 follow.


## Portable two-child raster isolation and replacement — 2026-09-25

WP-50 now pins three stages from real `ApplicationShell` windows with independent
`TerminalView` and `TerminalEmulator` instances: red and green child rasters
together, green surviving red's individual close, and green alongside a newly
allocated blue child. The generator asserts exact opaque color counts
(240 red + 240 green; 240 green; 240 green + 240 blue), rejects every opaque
pixel outside the active child content rectangles, and writes paired raw RGBA
and cell dumps for each stage. All six fixtures are in the
`generated_golden_bytes` manifest. This portable check complements the native
ConPTY peer tests; it does not claim to drive a live Windows GUI.

The changed generator and manifest SHA-256 values are
`99c0e87b29143755001f3a02417eb75a62217166ef2f1e1ba63a4313c30d7c08`
and `b52fe52ac7de0caaceea7cf6e462fad39b6791bf130b77a3ff624e53f7614abf`.
The macOS Release generator rebuild recompiled the changed source in two
seconds, then `generated_golden_bytes` passed **67/67**; the full macOS CTest
passed **177/177** in 19.49 seconds. The Windows ARM64 Release generator
rebuild ran from 17:29:52 to 17:29:56 UTC, compiled the changed source, and
produced SHA-256
`955FC465945D8FFAB4B87D4B9C425C24FA2F82D309D11D9B759DB573139177BB`;
its targeted golden gate passed **67/67**. The prior Windows all-target build
and full CTest passed **164/164** after the adapter change, before this
generator-only change. The fresh Linux ARM64 Release all-target build ran
from 17:32:48 to 17:34:46 UTC (382 compile/link steps), producing generator
SHA-256 `d93eb5b4374ab91a127c7d9242b1e930509fcef98c25d58cc53adeee39e46650`;
full Linux CTest passed **177/177** in 11.30 seconds. The 118-second Linux
build is within the previously observed 68–261-second clean-build range;
its log shows all 382 steps and no warnings. All three golden gates report
**67/67** and the identical manifest SHA-256
`4964c83b9e06e42e64f605957d1a9bc7e029d56c206bc628ba75325b0d8dd85b`.
Build/test logs and Windows structured results are retained under
`/Volumes/PRO-BLADE/tmp/ckvision-dual-golden-20260925`. True pixel-mouse
host evidence and the remaining WP-50/WP-37 scenarios remain open.


## Native concurrent ConPTY resize storms — 2026-09-25

A new Windows native case launches two distinct private ConPTY children with
red and green Sixel output, applies five interleaved resize steps to each,
then sends separate input. The fixture checks its own final console size
(55×10 and 44×9) using `GetConsoleScreenBufferInfo`, rather than accepting
only the adapter's requested size. The test checks both retained ready markers,
valid final cursor positions, separate raster identities/colors when modern
ConPTY provides graphics, inbox text fallback otherwise, distinct input
echoes, and clean child exits. This covers the native two-child resize-storm
row; the full user-facing GUI resize matrix remains open.

The changed child fixture and test source SHA-256 values are
`78B9DA90384118A5DC8B89A7D96426BE5716803FDBB720CF804A306634EEF44C`
and `9F7C26EFFCB71068FF050C82852C62A0FA5AB457A7A7D8B30E4BE00CD74F9354`.
The Windows ARM64 Release `cvision_tests` target rebuilt from 17:43:25 to
17:43:32 UTC in 7.61 seconds, close to the prior 7.07-second comparable
rebuild. Its log shows both changed sources compiled and no warnings. The
verified runner,
`C:\Users\klukas\ckvision-wp50-20260924a\build\tests\Release\cvision_tests.exe`,
was written at 17:43:32 UTC with SHA-256
`B23F47D4000DE75D460E9BB5788960C62765E9D2DEEB8D87C55903E48BFCF2B6`;
the child fixture SHA-256 is
`91C5C82E641AD788979CB156C63918A0167C0FDF4D97B45429022FB1F5E1A3AD`.
The new case passed alone, inbox ConPTY passed **21/21**, and the same runner
bytes with the hash-pinned modern ConPTY package
`175640566A3B59C4B132070EE96C2C77E5AB7EDD2E92732A5EB3610BBF63D90E`
passed **21/21**. Full hosted Windows CTest passed **164/164** in 14.21
seconds, including `provenance_check` and `provenance_checker_self_test`.
Source/output records, build/test logs, and the exact task scripts are retained
under `/Volumes/PRO-BLADE/tmp/ckvision-wp50-resize-storm-20260925`. No
ckVision GUI or test process was running before the build. The VM remained
headless. True live pixel mouse, remaining GUI/degraded-host rows, and the
WP-37/WP-38 release matrix remain open.

## Child raster snapshot ownership — 2026-09-26

`ARCHITECTURE.md` §4 requires private child rasters to pass through the
emulator, `TerminalView`, and the scene compositor. A captured raster image is
therefore a stable frame value while the child continues writing. The live
emulator and its `TerminalRaster` own two references to the pixel buffer; a
snapshot or retained scene region adds another. The prior copy-on-write flag
only remembered sharing with the Sixel decode cache. After the first text
write detached that cache, a later write could erase pixels in an already
captured image.

`a_captured_child_picture_keeps_its_pixels_after_later_text_damage` reproduces
that second-write mutation: it failed against the previous code and passes
when `damage_rasters()` detaches a buffer with an external owner. The public
child-session protocol and the 93 golden files are unchanged. This repairs
snapshot/scene ownership; it does **not** claim that re-encoding transparent
holes as opaque Sixel pixels is solved.

The changed source and regression hashes are
`7442d29f085a9c524e1ddf45f6703081773b93410ba77afa042158346300f90e`
and `97eb94512c93305263936aa8fbcbfe49e8471b82fc82b2ddbfd46fc56da7e083`.
The Release builds completed with zero warnings on macOS (22:35:29–22:35:33
UTC for the changed test runner, followed by an all-target no-change build),
Linux ARM64 (22:36:47–22:36:54 UTC), and Windows ARM64
(22:37:13.710–22:37:26.222 UTC). Full CTest passed **177/177**, **177/177**,
and **164/164** respectively; the generated-byte gate and both provenance
gates passed on each host. Current build logs and exact output hashes are in
`/Volumes/PRO-BLADE/tmp/ckvision-wp50-raster-cow-20260926/validation.md`.
The remaining host and release rows retain their recorded status.
