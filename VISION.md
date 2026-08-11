# ckVision — Vision

## Mission

Build the definitive windowed TUI framework for modern terminals: a library
with the full interaction depth of the classic 1990s desktop-in-a-terminal
environments — windows, menus, dialogs, complete keyboard discipline — and
the engineering quality of a modern, professionally architected C++ library.

Nothing currently occupies this spot. The classic frameworks carry 1990
APIs and global state. Most modern TUI libraries
are either immediate-mode toolkits that cannot express overlapping windows
and modal dialogs, low-level rendering layers without a widget system, or
widget sets that stop short of real menu bars, focus discipline, and
window management. ckVision is the full deal, rebuilt properly.

## Non-negotiable capabilities

These are table stakes, not stretch goals. A release that lacks any of them
is not a release. They define the floor — the classic framework baseline
plus ckVision's own additions — and each must work completely with both
keyboard and mouse:

1. **Overlapping windows**: movable, resizable, zoomable, closable framed
   windows with z-order, window cycling (next/previous), select-by-number,
   a window list, and tile/cascade arrangement on a desktop.
2. **Menu system**: a menu bar with drop-down menus, nested submenus,
   highlighted mnemonic letters (Alt+letter activation), right-aligned
   accelerator hints, separators, checkable and disabled items, and
   context (pop-up) menus. Menus open with shadows and are fully keyboard-
   and mouse-navigable.
3. **Focus discipline**: one unambiguous focus at all times, visible focus
   styling, Tab/Shift-Tab traversal in a defined order, arrow-key movement
   within groups, correct initial focus in every dialog by construction,
   and focus restoration when windows and dialogs close.
4. **Hotkeys and accelerators**: application-wide and per-window
   accelerator tables, menu mnemonics, dialog-label mnemonics that focus
   their controls, a default-button Enter rule, and Esc-to-cancel. All
   bindings are rebindable at runtime.
5. **Modal dialogs**: nested modality with correct event scoping, plus
   standard message boxes and file dialogs.
6. **Status line**: context-sensitive hint line bound to the focused view,
   with clickable shortcut items.
7. **Full mouse support**: click-to-focus, drag to move and resize windows,
   wheel scrolling, double-click semantics, drag-selection in text
   controls — alongside, never instead of, complete keyboard operability.
8. **Chrome**: window shadows and menu shadows, active/inactive frame
   styling, and switchable color schemes — including a faithful classic
   look and modern dark and light themes.
9. **First-class inline graphics**: Sixel raster content anywhere in the
   UI — images, charts, previews — z-ordered, occluded, scrolled, and
   shadow-dimmed like any other content; a declared textual fallback at
   every image site so applications stay fully functional on text-only
   terminals; and mouse events on images that carry pixel coordinates
   alongside cell coordinates wherever the terminal reports them.
10. **First-class text and source editing**: a reusable UTF-8 document and
    editor widget with deterministic editing, undo/redo, search/replace,
    line/column indication, file-editor composition, and incremental syntax
    highlighting. Standard JSON, YAML, and Bash profiles ship with the
    library; applications extend profiles through explicit instance-owned
    registration, never a global grammar registry.
11. **Contained embedded terminals**: a resizable window can host an explicit
    child terminal session — shells, terminal applications, or another
    ckVision application — with normal input, resize, lifecycle, and Sixel
    graphics behavior. Child output is privately emulated and composited, so
    it cannot escape, glitch, or spill beyond its parent window.

## Beyond the baseline

Where ckVision must be **better** than the classic frameworks:

- **True color and themes.** 24-bit color with deterministic degradation to
  256 and 16 colors; flat, inspectable theme tables instead of palette
  index chains; runtime theme switching.
- **Unicode correctness.** UTF-8 throughout; grapheme-cluster aware width
  handling (East Asian wide, combining marks, emoji) in every widget, not
  as an afterthought — with the boundary stated honestly: rendering is
  logical-order (no bidi reordering, D-026), and agreement with each
  terminal's actual width behavior is managed as a probed capability
  (D-019), never assumed.
- **Performance as a feature.** Damage-tracked partial repaints, per-window
  backing stores (dragging a window never re-paints its content), cell-diff
  terminal output, flicker-free synchronized frame presentation, and
  allocation-free steady-state event dispatch — with measured budgets and
  regression benchmarks, not intentions. See ARCHITECTURE.md §Performance.
- **A modern widget set.** Everything the classics had, plus the controls
  they lacked: tab controls, combo boxes, tables with columns, trees,
  progress indicators, splitters.
- **Modern input fidelity.** Enhanced keyboard protocols where the terminal
  offers them (disambiguated modifiers, key release), bracketed paste,
  focus in/out events, system clipboard integration — each with graceful
  degradation.
- **Headless-first testability.** Every frame the library produces is a
  deterministic function of inputs; applications and the library itself are
  tested against byte-exact styled screen dumps without a terminal.
- **Instance-based architecture.** No global mutable state; multiple
  application instances per process; embeddable event loop.
- **Pixel-aware interaction.** One mouse model, two native coordinate
  spaces: cell position always, pixel position on graphics where the
  terminal reports it — first-class in the event type, no second event
  system, no bolted-on hit-testing.

## Design principles

1. **Native to the terminal, native to C++.** The API models terminal
   reality — cells, character width, styles — with modern C++ (strong
   types, RAII, value semantics, concepts at extension seams). It imports
   no paradigm from the web: no style sheets, no selector cascades, no
   reconciliation trees. Layout is explicit containers with weights and
   size policies; styling is flat semantic theme tables. Clarity beats
   familiarity-with-something-else.
2. **Easy things easy, hard things possible.** A complete windowed
   application with a menu bar, status line, and a dialog must fit on one
   screen of readable code with sane defaults. Custom widgets, custom
   themes, and custom backends are first-class, documented extension
   points, not internal surgery.
3. **Retained scene, event-driven.** A persistent view tree owns state;
   events route by focus, position, or broadcast; commands are first-class
   with central tables, enablement, and menus that render live from the
   command registry. This is the model
   that decades of windowed-application practice validated.
4. **Deterministic by construction.** Same inputs, same capability profile,
   same frames — byte-exact, on every platform. Time enters only through
   an injectable clock. Determinism is what makes golden testing, replay
   debugging, and cross-platform trust possible.
5. **Zero mandatory dependencies.** The library links nothing but the C++
   standard library and platform APIs. No curses, no terminfo files, no
   vendored kitchen sinks. Terminal capabilities come from curated
   profiles plus runtime probes.
6. **Every capability degrades gracefully.** Truecolor → 256 → 16 colors;
   enhanced → legacy keyboard; graphics → none; synchronized → unsynchronized
   output. Feature loss is proportionate and never catastrophic.
7. **Professional polish end to end.** Panic-safe terminal restoration,
   IME-aware input, resize-storm coalescing, suspend/resume, warnings-clean
   builds, sanitizer-clean tests, fuzzed parsers, documented behavior. The
   bar is: no caveats.

## Platforms

- **macOS**: Terminal.app, iTerm2, kitty, WezTerm, Ghostty.
- **Linux**: xterm-compatible emulators (the above plus GNOME/KDE
  terminals, foot, alacritty); the Linux console as a degraded profile.
- **Windows**: Windows Terminal and other ConPTY/VT-enabled hosts on
  Windows 10 1903+. Legacy non-VT consoles are detected and refused with a
  clear message — ckVision targets the modern Windows terminal stack.
- Terminal multiplexers (tmux, screen) as explicit, tested profiles.

One rendering model serves all platforms: VT/ANSI escape sequences over a
platform transport. There is no per-platform drawing code, only per-platform
I/O and process plumbing.

## Non-goals

- Web technologies in any form: HTML/CSS concepts, browser rendering,
  JavaScript-ecosystem idioms.
- Immediate-mode UI. It cannot carry this interaction model.
- API or source compatibility with Turbo Vision or any other framework.
  ckVision owes the classics inspiration, not bug-for-bug fidelity.
- Wrapping or depending on curses/ncurses/terminfo.
- Built-in scripting languages, markup dialects, or configuration DSLs.
- Being a general-purpose application framework: no application-specific file
  formats, project model, or network stack. The scoped in-memory text document,
  file-editor controller, and contained local terminal session are UI
  capabilities, not a general document/process/remote-terminal framework.

## Independence

ckVision is a standalone project. It depends on no other application or
library and embeds no other project's models; any application integrates
by mapping its own command and configuration systems onto ckVision's
public APIs through the documented extension points. Acceptance is
defined inside this repository — the non-negotiable capability list, the
gallery, and the golden specifications — and the standing policy for
real-world use is: when an adopting application finds a capability
missing or awkward, the library, not the application, is what gets
fixed.

## Provenance

ckVision is a clean design: all code is written for this project from its
own specifications. Behavioral inspiration comes from documented, published
interaction conventions (CUA and the classic DOS desktop environments);
technical references are open standards and published terminal
documentation (ECMA-48, xterm control sequences, the kitty protocol
specifications, Microsoft's console VT documentation). No source code from
Turbo Vision or any of its ports is consulted, copied, or transcribed —
see AGENTS.md for the binding rule.
