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

*(Sections for WP-89 through WP-96 will be populated as each work package lands.)*
