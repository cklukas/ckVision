<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Live Gallery in iTerm 3.6.11 — 2026-09-27

The actual `ckvision_gallery` process ran in a new iTerm window on macOS 27.0
(26A428), Mac13,1. This was an agent-directed interactive session in a real
terminal, not a headless golden or a PTY byte driver. Source revision:
`fa8400e9a1328626fe3bf19c63f64d765cf726aa`. The executable code was
unchanged since the verified Release build at `f08b2ae`, for which the current
Mac CTest suite passed 218/218, including the provenance gates. Before launch,
no ckVision example process was running. The exact process was PID `92775`:

```text
/Volumes/PRO-BLADE/tmp/ckvision-independent-closeout-HZg0Xoga/release-build/examples/ckvision_gallery
```

Its SHA-256 was
`5af648740bede946a0b5ae6b6445f9a879c2cd1bea4fbb5e9a9bfc1c10f8ffc2`,
and its build timestamp was `2026-09-27T18:34:30Z`, after its current source
inputs. `TMPDIR` named the unique task directory on `/Volumes/PRO-BLADE/tmp`.
The iTerm window id was `106802`; every capture below targets that actual
window while the verified process was running.

## Picture, scrolling and occlusion

1. The [initial window](gallery-iterm-initial.png) showed the actual Sixel
   gradient inside `Sixel Demo`, with Controls overlapping to its right.
2. A native macOS wheel event over the picture scrolled it visibly: the
   [wheel frame](gallery-iterm-wheel.png) exposed a lower band of the gradient.
   The temporary CoreGraphics event helper used
   `CGEventCreateScrollWheelEvent` at the picture coordinates. It was a
   one-file, zero-warning helper built in one second under its own unique
   `/Volumes/PRO-BLADE/tmp` subdirectory; its source SHA-256 was
   `58bed68ca3a8ced178d430640eeb4400b61de40c4411babee8c48e0f6faa6ac4`.
   There is no documented comparable build-duration baseline for that helper.
3. After a real click focused the picture viewport, PageDown moved the image
   again in the [page frame](gallery-iterm-page-focused.png). PageDown before
   that focus change left the picture in place, as expected for a key delivered
   to another focused control.
4. View → Scheme opened across the scrolled picture. The
   [submenu frame](gallery-iterm-scheme.png) shows the picture sliced away
   beneath both menus, and the shadow over its remaining pixels. After the
   menu was dismissed, the [restored frame](gallery-iterm-restored.png) shows
   the picture intact with no visible residual menu pixels.

## Dual-space mouse through `PosixTerminal`

Gallery's File → Terminal Report opened in the same live process. Its
[actual iTerm capture](gallery-iterm-report.png) reports:

```text
ckVision             0.1.8
Cell grid            80x25 cells
Effective cell       22x48 px (reported)
Mouse protocol       SGR (modern)
Pixel mouse          yes
Mouse events seen    994
Last mouse event     cell=(72,15) pixel=(802,378)
```

Thus a real iTerm 3.6.11 report traversed ckVision's POSIX backend and reached
the Gallery with both cell and pixel coordinates. This closes the literal
interactive dual-space Mac obligation in M3. The report modal closed with Esc;
Gallery exited with Alt+X, its PID was gone, and the temporary iTerm window was
closed.

## Capture identities

| Frame | SHA-256 |
|---|---|
| [Initial](gallery-iterm-initial.png) | `91250d56cfd940b64434b60328f1e9ac0dfad93e390f5afb3068525545e1b2fd` |
| [Wheel](gallery-iterm-wheel.png) | `0e08bd6fb902ee3d9f840f702a6b47bcf95f29dcc8b1fdeae13aa8a97300a910` |
| [PageDown](gallery-iterm-page-focused.png) | `3d786459d8151bfca0814a9d0d319831fb617532ce8b60a59e91670658c457e8` |
| [Menu](gallery-iterm-scheme.png) | `b73e922f368f363eb79d08ad54a9b3a05e6f6a7f862024fd864c0774a0c68ea7` |
| [Restored](gallery-iterm-restored.png) | `bb34700ce4954b78b6d1637bdebfa38a2dd5516c7b2c54ce2a67fb8bdc3f232c` |
| [Terminal Report](gallery-iterm-report.png) | `a73d4068986c8fc9356c70c809b66ddc9ae1266c8bd096331941871b619f9f62` |

The image-demo ROADMAP row still needs an interactive Linux run. The separate
project-owner release acceptance and classic-look sign-off remain open.
