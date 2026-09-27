<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Live Linux examples and Release suite — 2026-09-27

Debian 13.7 ARM64 ran ckVision revision
`43a636d1f3d6f632a8804eaa0d7987191ba5716b` in the `ckdev` VM. The
repository was mounted from the Mac, but all generated Linux build files and
logs lived in the unique remote workspace
`/home/klukas.guest/ckvision-closeout-20260927.r6LbIT`. The remote `TMPDIR`
pointed there. The Mac-side `limactl` and capture commands used a separate
unique task directory under `/Volumes/PRO-BLADE/tmp`.

## Fresh Linux build and tests

The strict Release configuration built examples and tests with warnings as
errors. The clean build ran from 19:30:58Z to 19:34:18Z (200 seconds), with
393 C++ compile actions and no warning or error diagnostics. Comparable
documented Linux clean builds took 153–269 seconds. The exact output was
`/home/klukas.guest/ckvision-closeout-20260927.r6LbIT/build`; Gallery's
executable was stamped 19:32:33Z, after its source inputs. The full CTest
suite ran from 19:34:35Z to 19:34:48Z: **218/218 passed** in 13.08 seconds,
including `provenance_check` and `provenance_checker_self_test`. The build
used GCC 14.2. The checked executable SHA-256 values were:

| Example | SHA-256 |
|---|---|
| Echo | `7f0b259e055bbe55054fb38d96aff816d9f5377c009c3cb673a82e1f219a4101` |
| Gallery | `935bcdf6ea0ffcc03a942b6771be873890ed11e1eb67e0e1d97befdffaf3b090` |
| Root Dialog | `7cc0c541fff1176a8f5b2302a11bd242ed5df14ad2c8565eff758c54ae9993ea` |

These are current-version Linux Release results. Windows current-version
results and the separate sanitizer/fuzz gates remain separate obligations.

## Echo and Root Dialog

Each Linux binary ran in its own actual Mac Terminal window over an
interactive `limactl shell ckdev` connection. The process was verified by
remote PID and absolute executable path before input; no other ckVision
example ran concurrently.

- Echo, PID `910647`, Terminal window `107243`: native keyboard `L`, Tab,
  Enter and Shift+Tab, a real terminal resize from 80×24 to 157×40, focus
  transitions, and Alt+X produced an 18-line decoded transcript. The
  [actual window](echo-live-linux-input.png) was inspected while the process
  ran. The transcript ended with `Alt+"x"` and Enter; the PID then exited.
- Root Dialog, PID `910695`, Terminal window `107259`: the
  [actual window after veto](rootdialog-live-linux-veto.png) showed an empty
  Email rejected and focus returned to that field. Entering `Lin` and
  `lin@example.com` then accepting exited the process and returned
  `name=Lin` and `email=lin@example.com` in the Terminal window.

Both temporary Terminal windows were closed after their processes exited.

## Gallery in iTerm

The verified Linux Gallery binary ran as remote PID `910710` in iTerm 3.6.11
window `107273`. The [initial live Sixel image](gallery-linux-initial.png)
appeared in the picture viewport. A native CoreGraphics wheel event over
that viewport exposed a lower portion of the gradient in the
[wheel capture](gallery-linux-wheel-plus.png). The temporary event helper was
built with zero warnings in a unique Mac task subdirectory; its source SHA-256
was `58bed68ca3a8ced178d430640eeb4400b61de40c4411babee8c48e0f6faa6ac4`.
The first wheel direction moved toward the top limit and showed no new image
rows; the opposite direction produced the captured scroll.

Selecting the Sixel Demo window, moving to its viewport tab stop and pressing
PageDown exposed the lower part of the image in the
[PageDown capture](gallery-linux-page-tab.png). View → Scheme then covered
the scrolled picture in the [menu capture](gallery-linux-menu-scrolled.png).
Two Esc presses dismissed its submenu and parent menu; the
[restored capture](gallery-linux-restored.png) showed the same image area
without residual menu pixels. The [Terminal Report](gallery-linux-report.png)
in that live process reported SGR mouse, pixel mouse enabled, and a last
event with `cell=(72,23) pixel=(794,596)`. This also proves the remote Linux
POSIX backend receives both coordinate spaces from the actual terminal host.
Gallery exited with Alt+X, PID `910710` disappeared, and its iTerm window
was closed.

The captures show an agent-directed interactive run on the named Linux host.
They do not constitute the product owner's release approval.

## Capture identities

| Frame | SHA-256 |
|---|---|
| [Echo](echo-live-linux-input.png) | `eb40ed9923330c52371bc5e5c62cee192366c4e2048c9db337bc797361d2f127` |
| [Root Dialog veto](rootdialog-live-linux-veto.png) | `b87d667a114102bc5e7ebee56655e1d41cc99e108aa2c07515a09ed9941f4a06` |
| [Gallery initial](gallery-linux-initial.png) | `59d5617aefce34092f48895fdc0a563bcf8d8bbe5aa1891fe72cd55c14798683` |
| [Gallery wheel](gallery-linux-wheel-plus.png) | `e0b533c3f70d5f41fb9d7cf34377c8eb4d960672e37c65bc3b49c80d514a8baa` |
| [Gallery PageDown](gallery-linux-page-tab.png) | `58e7d1c1c7fef8a014950a7cec9f3f427ab33540314204a28ceaf4ffc5ec3edc` |
| [Gallery menu](gallery-linux-menu-scrolled.png) | `f32e867fad502091012929a4c5307e0facaaeee455d56f6556112824115ad16f` |
| [Gallery restored](gallery-linux-restored.png) | `d0611c113fa7b81b05a7cc1490bf2d7fa35ba7bb08577e325c9df51cc63275e2` |
| [Gallery report](gallery-linux-report.png) | `6b61ad65395aa23ce881e3c8d61a65f58df883ba61a6fb3c506b96dbcfb6b543` |
