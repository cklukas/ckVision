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

**M3 (term) complete**, on top of M0's build skeleton, M1's core
primitives (geometry, Color/Style/Cell/Image, keys/events, the Unicode
services in [docs/text-width.md](docs/text-width.md)), and M2's scene
layer (`Surface`/`Painter`/`Compositor` with damage tracking, junction
merging, shadow occlusion, and raster occlusion slicing). The term
layer now provides: the `Terminal` interface and `Capabilities` model;
an incremental input decoder (legacy/kitty/modifyOtherKeys keys, SGR
and SGR-Pixels mouse, sanitized bracketed paste, focus events, OSC
10/11 and DECRPM probe responses — coverage documented in
[docs/input-decoder.md](docs/input-decoder.md)); a `Presenter` (cell
diffing, D-019 width-safe cursor re-addressing, color-depth
degradation, synchronized output, OSC-safe emission, cropped Sixel
raster overlay); a correct public-protocol Sixel encoder; headless,
POSIX (real termios/signals/session-registry, PTY-tested), and
record/replay backends. Implementation continues along the roadmap
milestones.

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
