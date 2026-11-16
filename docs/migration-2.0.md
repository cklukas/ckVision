<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Migrating from ckVision 1.x to ckVision 2.0

ckVision 2.0 removes accumulated 1.x compatibility shims, mega-headers, and
redundant factory parameters in favor of one clean, fast, ergonomic API. Every
breaking change below has a direct mechanical replacement.

---

## Summary of Breaking Changes by Work Package

### WP-85 — Build & Package Changes
- Shared library builds (`-DBUILD_SHARED_LIBS=ON`) now export `CVISION_API`
  symbols with `SOVERSION 2`.
- When embedding ckVision via `add_subdirectory()` or `FetchContent`,
  `install()` and CPack targets are no longer emitted by default unless
  `-DCKVISION_INSTALL=ON` is set.

### WP-86 — 32-Byte Inline `Cell`, Checked Geometry & Unicode Integrity
- `ckv::LinkId` is now `std::uint16_t` (up to 65,535 distinct live hyperlinks
  per `Surface`/`FrameView`; `LinkTable::acquire()` returns `kNoLink` if full)
  so `ckv::Cell` packs to **32 bytes** with 15-byte inline UTF-8 storage.
- `ckv::Rect` adds 64-bit `right64()` / `bottom64()` and clamps `right()` /
  `bottom()` / `intersected()` without signed 32-bit overflow UB.
- `ckv::Image` validates `stride` and pixel buffer bounds in 64-bit arithmetic
  (clamping overflowing sizes to an empty `0x0` image) and exposes
  `content_id()` / `mark_modified()` for `O(1)` raster caching.
- `ckv::text::sanitize_display_text()` now neutralizes Unicode `Cf` zero-width /
  BiDi format controls and `U+00AD` (while preserving `U+200D` ZWJ for emoji).

### WP-87 — Clean `<windows.h>`-Free `term` Headers & `InputDecoder`/`KeyChord` Fixes
- Public `include/cvision/term/windows_*.hpp` headers no longer include
  `<windows.h>` or leak `min`/`max`/`near`/`far` macros into consumer code:
  - `WindowsTerminal(clock, output, input)` takes `void*` (`HANDLE`) instead of
    Win32 `HANDLE`, with a 1-arg `WindowsTerminal(clock)` overload for standard
    console handles.
  - `sample_windows_job(void* job)` takes `void*` (`HANDLE`).
  - `WindowsTerminalSubsession` moves ConPTY native Win32 state behind a Pimpl.
- `KeyChord::parse()` now round-trips `"+"` and `"Ctrl++"` and rejects raw
  control bytes.

### WP-88 — Lifetime Safety, Focus Visibility, `TextEditor` & `Desktop` Invariants
- `View::focusable()` now checks `visible_in_tree()` instead of only the
  view's own `visible_` flag, preventing hidden ancestors (such as inactive
  `TabControl` pages) from leaving focusable descendants reachable.
- `TabControl::set_active_index()` evacuates focus when hiding the old page
  and restores per-page focus when returning to a tab.
- `Desktop::tile*()` filters by `tiling_participant()` (excluding fixed-size
  dialogs), clears `Window::zoomed_` on tile/cascade/manual resize, and
  `Desktop::cascade()` keeps `popups_` frontmost and activates the top window.

### WP-89 — Theme Overhaul, WCAG 2.1 Gates & Surface-Aware Chrome
- `StandardRoles` adds `input_selection` (`"ckv.input.selection"`); `InputLine`
  and `Memo` paint selections with `ckv.input.selection` instead of `Attr::Reverse`
  and publish `cursor_state()` for native/IME caret tracking.
- Explicit color roles in `Classic`, `Dark`, `Light`, `Mono`, and `High Contrast`
  no longer carry `Attr::Reverse`, `High Contrast` no longer carries `Attr::Dim`,
  and `set_editor_roles` removes hardcoded pink RGB leaks so all 5 themes pass
  WCAG 2.1 contrast (`>= 4.5:1`) and palette-purity gates.
- `scene::box_drawing` adds `mixed_junction_glyph` and classifies `╞`, `╡`, `╤`,
  `╧`, `╪`; `Table` renders `" ▲"` / `" ▼"` sort chevrons.

### WP-90 — Adaptive Bayer Sixel, `FunctionRef`, `ImageSampling` & Kitty Graphics
- `Painter::draw_image()` now accepts `ckv::FunctionRef<void(Painter&)>`
  instead of `const std::function<void(Painter&)>&`, and `painter.hpp` no
  longer includes `<functional>`. Existing lambdas and callables bind
  implicitly; callers that previously passed a `std::function` directly may
  pass it unchanged as an lvalue or convert callables to lambdas.
- `ckv::Image::scaled_to(PixelSize, ImageSampling)` adds `ImageSampling::{Auto, Nearest, Smooth}`
  (default `Auto`: deterministic box-averaging downscale, bilinear upscale);
  `ImageView` adds `set_sampling()` / `sampling()`.
- `SixelEncoder` deduplicates palette entries in `0..100`% space, quantizes
  adaptively up to `min(max_colors, 256)` with stateless `(anchor_px_x, anchor_px_y)`
  4x4 Bayer ordered dithering, and supports RGBA alpha compositing + `P2=1`
  transparent holes via `SixelEncodeOptions`.
- `PosixTerminal` and `WindowsTerminal` probe Kitty Graphics before `DA1`;
  `Presenter` and `VirtualDisplay` support chunked 32-bit RGBA Kitty Graphics
  placements (`\x1B_Ga=T,f=32,...`) and cleanup (`\x1B_Ga=d,d=A`).

### WP-91 — High-Fidelity SVG/PNG DEFLATE Snapshot Engine
- `scene::SvgSnapshotOptions` adds `std::optional<CursorState> cursor` and
  escapes `font_family` for `<style>` safety.
- `scene::render_svg_snapshot()` renders `Attr::Dim`, `Attr::Italic`,
  `Attr::Strike`, OSC 8 `<a href="...">` anchors, optional cursor overlays,
  coalesces contiguous `<text>` runs, and compresses embedded raster PNGs with
  PNG `Sub` filtering + fixed-Huffman DEFLATE.

### WP-92 — Desktop-in-a-Terminal UX Mastery
- **`StatusLineItem::command` removed**: construct command-backed items with
  `StatusLineItem{CommandPresentation{cmd, label, chord}, priority}` instead of
  `StatusLineItem{label, cmd, priority}`; construct plain-text items with
  `StatusLineItem{label, priority}`.
- `CommandRegistry` adds `CommandDescriptor::secondary_chord` (and
  `CommandInfo::secondary_default_chord`); `standard.window_list` binds `Alt+0`,
  and `standard.command_palette` binds secondary `F12` alongside `Ctrl+Shift+P`.
- `Desktop` installs live `set_enabled_predicate`s on standard window commands
  and exposes active desktop verbs in `CommandPalette`.
- `ApplicationShell` auto-wires `menu_bar_->on_highlight_changed` to
  `status_line_->set_transient_hint`; `widgets::make_standard_window_menu()`
  builds a standard `&Window` menu.
- `Window` gates chrome/drag clicks to `MouseButton::Left`, toggles `Zoom` on
  left double-click on resizable title bars, and opens a Window Control Menu on
  `Alt+-` or title-bar right-click.

### WP-93 — `O(line)` Incremental `TextEditor` & Syntax Lexers
- `EditorDocument` adds `line_byte_bounds(std::size_t line)` and
  `line_text(std::size_t line)`.
- `TextEditor` eliminates full-document `document_->text()` copies on edits,
  arrow navigation, `Backspace`, `on_resized()`, and double-click word
  selection; updates lines and search matches incrementally; and uses
  `std::partition_point` for `O(log M)` search-match lookup.
- `syntax_profile` adds `.jsonc` (`//` and `/* ... */`) comments in `json_line`,
  string-aware `#`/`:` + strict content detection in `yaml_line`, and
  `${...#...}`/`$#`/`\"`/quoted-keyword/heredoc fixes in `bash_line`.

### WP-94 — `O(damage)` `Presenter`, Delta-SGR, `DECSTBM` Scroll-Intent & Sixel LRU
- `FrameView` adds `cells()` and `ScrollIntent` (`region`, `dy`); `LinkTable` adds
  `generation()`.
- `Presenter::present()` accepts an optional `std::optional<ScrollIntent> scroll =
  std::nullopt` parameter, emits `DECSTBM` (`CSI top;bottom r`) + `SU`/`SD` (`CSI S`/`T`)
  hardware scroll sequences when safe, emits minimal delta-SGR sequences via
  `<charconv>`, degrades `underline_color` through `ColorDepth`, caches encoded Sixel
  rasters in an 8 MiB LRU, skips unchanged `LinkTable` copies using `generation()`,
  and narrows `is_width_unsafe()` to multi-codepoint, format, emoji, and ambiguous
  symbol graphemes (single-codepoint precomposed Latin and box-drawing graphemes
  coalesce into single runs).
- `VirtualDisplay` supports `DECSTBM` (`CSI ... r`) and bounds `SU`/`SD` (`CSI S`/`T`)
  and line-feed scrolling to `[scroll_top_, scroll_bottom_]`.
- `ui::View::notify_scroll()` and `Application` wire vertical scroll intents from
  `TextEditor`, `LogView`, and `ScrollViewport` to `Presenter::present()`.

*(Sections for WP-95 through WP-96 will be populated as each work package lands.)*
