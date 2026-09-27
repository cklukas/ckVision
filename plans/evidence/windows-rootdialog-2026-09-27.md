<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Windows Root Dialog: clean build and live form — 2026-09-27

The M4 form application was already covered headlessly on Windows, but its
interactive executable was guarded by `if(UNIX)` in `examples/CMakeLists.txt`,
and its entry point constructed only `PosixTerminal`. The correction uses the
same `WindowsClock`/`WindowsTerminal` construction as ckVision's own Gallery
and Terminal examples. `RootDialogApp` and its headless scripts are unchanged.
The behavior derives from ckVision's Root Dialog specification and its
existing terminal backend; no outside implementation source was consulted.

## Current-source build

The named Windows 11 ARM64 VM (`ckvision-win11`, build 26200.9457) received a
SHA-256-checked archive of `5383ce3` and the three pending changes to
`examples/rootdialog/main.cpp`, `examples/CMakeLists.txt`, and
`docs/example-apps.md`. Their SHA-256 values before the build were
`0d69def897ef806b26d97c2fa95dddd3d2ffac2df425f2c2076d6eba4b86b031`,
`2c237da86f6811dc7f56c0189ff1e6e1d73a7b90123ba9dca7b64e777ae325b8`,
and `0f7b8334405de47c645c86f3d37472983e200a2d3dc0d9019ed4bb193d1b6818`
respectively. The VM verified those same hashes. `CMakeLists.txt` remained
`e3bc0e9d5608e27ced484124c092e63d9221bd364491b4d883b7831ec12d25f0`.

The isolated Windows task workspace was
`C:\Users\klukas\ckvision-closeout-20260927-5383ce3`, with its own `tmp`
directory set as `TEMP`, `TMP`, and `TMPDIR`. No ckVision app was running before
the build. An earlier incomplete build from the uncorrected archive was
stopped, checked for active compiler processes, and removed before the
current-source clean configure and build.

Visual Studio 2022 ARM64 Release configured from 20:15:20Z to 20:15:34Z.
The all-target `/W4 /WX` build ran from 20:15:34Z to 20:28:12Z, **758.1
seconds**, with zero warning lines. Previously documented comparable clean
Windows ARM64 builds ranged from about 522 to 1,457 seconds; the log compiled
the expected library, example, generator and test inputs. The exact current
output was
`C:\Users\klukas\ckvision-closeout-20260927-5383ce3\build\examples\Release\ckvision_rootdialog.exe`,
written at 20:20:13Z after the changed source was staged. Its SHA-256 was
`1e703be1d86479a8b847e9aeac0320585a8979c0990391f40dff30a11581640c`.
The full Windows CTest suite ran from 20:28:12Z to 20:29:02Z:
**204/204 passed** in 49.30 seconds, including `provenance_check` and
`provenance_checker_self_test`. The matching Mac and Linux current-source
Release builds passed **218/218** CTest tests each, including both provenance
gates, after their Root Dialog entry points rebuilt.

## Actual signed-in desktop

The verified Windows executable was copied byte for byte to the dedicated
`LIMA\cktest` test account's unique temporary stage. A one-off interactive
scheduled task launched it in the signed-in console session through modern
Windows Console Host. The exact live lineage was Console Host PID `3948`,
`cmd.exe` PID `7088`, and `ckvision_rootdialog.exe` PID `5388`; the last
resolved to the hash-verified staged path. QMP captures targeted the VM's
actual user-facing framebuffer, not a headless render.

The [initial frame](windows-rootdialog-initial.png) showed the undecorated
form. Once the Console Host title bar was focused with distinct native
pointer press and release events, keyboard input entered `name`, Tab moved to
Email, and Enter [vetoed the empty Email](windows-rootdialog-veto.png). QMP
keyboard input then entered [`a@b`](windows-rootdialog-filled.png), which
cleared the invalid-field mark. Enter accepted; the
[restored console](windows-rootdialog-accepted.png) showed exactly:

```text
name=name
email=a@b
exit=0
```

The ckVision process exited normally. Its `cmd.exe` and Console Host were
closed by exact PID, and the one-off scheduled task and disposable public
stage were removed. A final process check found no Root Dialog instance.

| Capture | SHA-256 |
|---|---|
| [Initial](windows-rootdialog-initial.png) | `464cf7bf25ce16bbd9f0e36005b635206d72d258c53bc8d9af6eaf5cdb46c3fc` |
| [Validation veto](windows-rootdialog-veto.png) | `25b675d580e9613410dbbfda25da0e885a16a9019bc1d444dbb16865fc11cd65` |
| [Filled form](windows-rootdialog-filled.png) | `33d5d519d3c3766dd520ca5a1ab907ea413f87319734a5ce3adc903be2e25fed` |
| [Accepted output](windows-rootdialog-accepted.png) | `05b713f3665a3276e68349090e7d563c3f6d33e8a5d2d4ac4c59d77b91d48846` |

This closes the literal M4 interactive run on Windows. Product-owner visual
acceptance and release approval remain separate open rows.
