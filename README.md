# ckVision

**ckVision** is a full-featured, windowed terminal-UI framework for modern
terminals, currently in its planning phase — overlapping movable windows,
drop-down menus with mnemonics and shadows, modal dialogs, complete
keyboard and mouse interaction, themes, first-class Sixel graphics with
pixel-precise mouse input, and a damage-tracked rendering pipeline that
repaints only what changed.

It is designed in C++20 with **zero mandatory dependencies**, targets
**macOS, Linux, and Windows (Windows Terminal / ConPTY)**, and is built to
be **easy to use, extensible, deterministic, and fast**.

ckVision is inspired by the interaction grammar of the classic desktop-in-a-
terminal applications of the early 1990s — the standard set by Turbo Vision —
and re-imagines it on modern grounds: modern C++, modern terminals, modern
library architecture. It shares no code and no API with any prior framework.

The library name is `cvision` (CMake target `ckvision::cvision`).

## Status

**M0 through M6 complete** — every layer through the widget catalog is
implemented and adversarially tested (not just happy paths), reviewed,
and green under both Release and ASan/UBSan builds.

- **M0-M2**: build skeleton; core primitives (geometry, Color/Style/
  Cell/Image, keys/events, the Unicode services in
  [docs/text-width.md](docs/text-width.md)); the scene layer
  (`Surface`/`Painter`/`Compositor` with damage tracking, junction
  merging, shadow occlusion, raster occlusion slicing).
- **M3 (term)**: the `Terminal` interface and `Capabilities` model; an
  incremental input decoder (legacy/kitty/modifyOtherKeys keys, SGR and
  SGR-Pixels mouse, sanitized bracketed paste, focus events, OSC 10/11
  and DECRPM probe responses — coverage documented in
  [docs/input-decoder.md](docs/input-decoder.md)); a `Presenter` (cell
  diffing, D-019 width-safe cursor re-addressing, color-depth
  degradation, synchronized output, OSC-safe emission, cropped Sixel
  raster overlay); a correct public-protocol Sixel encoder; headless,
  POSIX (real termios/signals/session-registry, PTY-tested), and
  record/replay backends.
- **M4 (ui core)**: `View` (the ui tree's base class), `Application`
  (focus traversal, positional/focused/command-keymap event dispatch,
  mouse input capture, timers, F1 context-help routing, the embeddable
  `step()`/`wake()` loop), the flat interned-role `Theme` system,
  `Row`/`Column` layout, the internal clipboard, `HistoryRegistry`,
  declarative dialog descriptors with validated materialization, and
  the four minimal controls (Label, StaticText, Button, InputLine).
- **M5 (windows/menus)**: `Window` (frame chrome, move/resize, the
  vetoable close protocol, zoom), `Desktop` (z-order, activation,
  cycling, tile/cascade, popups), dialog accept-validation veto with
  Esc-cancel, message boxes with every standard button set, and the
  `MenuBar`/`DropdownMenu`/context-menu system with light-dismiss.
- **M6 (widget completion)**: `Scrollbar`, `ScrollViewport`,
  `CheckGroup`/`RadioGroup`, `ListView`, `TreeView`, `TextView`,
  `Memo`, `Table`, `ImageView`, `Canvas`; InputLine completion
  (validators, input masks, password echo, history cycling); the
  Dark/Light/Mono theme schemes; the window-list dialog, file open/
  save dialog and directory picker (both on the injected
  `FileSystem`), and the Help viewer.

Independent review passes across M4 and M5 found and fixed seven real
bugs (three use-after-frees, three correctness issues, one z-order/
lifecycle desync), each landed with a regression test verified under
ASan/UBSan.

## Documents

| Document | Content |
|---|---|
| [VISION.md](VISION.md) | Mission, non-negotiable capabilities, design principles, non-goals |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Target architecture: layers, rendering pipeline, event/command system, backends |
| [ROADMAP.md](ROADMAP.md) | Development milestones with exit criteria |
| [DECISIONS.md](DECISIONS.md) | Architecture decision records (settled and open) |
| [plans/widgets.md](plans/widgets.md) | Widget catalog: capability baseline and beyond-baseline features |
| [AGENTS.md](AGENTS.md) | Engineering standard for working in this repository |

## Copyright

Copyright 2026 Dr. Christian Klukas. All rights reserved.
The open-source license under which ckVision will be published is decided at
first public release (see DECISIONS.md, D-OPEN-1).
