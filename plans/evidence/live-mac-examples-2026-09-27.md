<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Real Mac Terminal example sessions — 2026-09-27

These are agent-directed sessions in the actual macOS Terminal 2.15 window,
not `HeadlessTerminal` fixtures or a PTY driver. The host was macOS 27.0
(26A428), Mac13,1. The tracked source revision was
`c9638edd205ccdf334a446c93a5f61dfe75af296`; executable code was unchanged
from the verified Release build at `f08b2ae`. Before launch, the build graph
reported no work and the current Mac suite had passed 218/218, including the
mandatory provenance gates. No other ckVision example process was running.
Each app was launched by its absolute path in a new Terminal window with
`TMPDIR` set to the unique `/Volumes/PRO-BLADE/tmp` task directory. The PID's
command resolved to that exact executable. The actual window was captured and
inspected before the session result was claimed. Each app exited; its temporary
Terminal window was closed afterward.

## Root Dialog

- Executable:
  `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/release-build/examples/ckvision_rootdialog`;
  SHA-256 `7b51d67f1683ab001fa66278efa7dd11b2b8145f955d8ed760a8e40412d735a5`;
  build timestamp `2026-09-27T18:34:36Z`.
- PID `86237` ran in Terminal window `106692` at 80×24 cells. The live initial
  frame showed an undecorated full-screen form, Name and Email input lines,
  and OK, Cancel and About buttons.
- Typing `Ada` visibly changed Name. Enter with Email empty kept the process
  alive, highlighted Email as invalid, focused it, and displayed “An email
  address needs an @.” The [actual Terminal window capture](rootdialog-live-mac-veto-2026-09-27.png)
  has SHA-256 `a9ce126027d3138def541bc7424b41a8a46247a62d5a7fdc21050f7f74b5208b`.
- Typing `ada@example.com` and pressing Enter exited the process; Terminal's
  restored screen showed `name=Ada` and `email=ada@example.com`. A process
  lookup found no remaining Root Dialog instance.

## Echo

- Executable:
  `/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/release-build/examples/ckvision_echo`;
  SHA-256 `dc3f867725800eae9f9dc14c618fb462d9f67a4be472048c2c3bb5c930545a6c`.
- PID `87348` ran in Terminal window `106731`. The initial frame displayed
  decoded host capability, resize and focus events. Real keyboard input
  produced `A`, Tab, Enter and Shift+Tab transcript entries. Resizing the
  Terminal window from 80×24 to 172×42 produced a resize event; actual mouse
  motion during that resize produced SGR cell-coordinate events. The
  [actual Terminal window capture](echo-live-mac-2026-09-27.png) has SHA-256
  `dbc2d278ac79f1a42193588210460cfc3fbefddfc9c26d363110f25a4a0ecb8a`.
- This Terminal profile translated Option+X to `»` rather than Alt+X. Sending
  the documented Escape-plus-`x` bytes to that exact Terminal tab produced
  `Alt+"x"` in the transcript and exited. Terminal also sent a trailing Enter.
  A process lookup found no remaining Echo instance.
- The 28-line transcript was written by the real binary; SHA-256
  `85b6108ed7b6366f37ee8458fd21fb0448f23d9cbcc1bdea50dbf9789e353e19`.
  Its event sequence was:

```text
     1  host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on
     2  resize  80x24
     3  focus   gained
     4  host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on
     5  host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on
     6  focus   gained
     7  key     press   "A"  U+0041
     8  key     press   Tab
     9  key     press   Enter
    10  key     press   Shift+Tab
    11  resize  172x42
    12  host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on
    13  host    keyboard legacy, mouse SGR, focus reports on, bracketed paste on
    14  key     press   "»"  U+00BB
    15  mouse   move    none        cell 68,36
    16  mouse   move    none        cell 68,36
    17  mouse   move    none        cell 68,37
    18  mouse   move    none        cell 69,37
    19  mouse   move    none        cell 70,38
    20  mouse   move    none        cell 72,39
    21  mouse   move    none        cell 74,40
    22  mouse   move    none        cell 77,41
    23  focus   lost
    24  focus   gained
    25  key     press   "z"  U+007A
    26  key     press   Esc
    27  key     press   Alt+"x"  U+0078
    28  key     press   Enter
```

These sessions establish a real Mac Terminal path for both examples. The
literal ROADMAP obligations still call for Linux sessions, and owner review
is separate from this agent-directed evidence.
