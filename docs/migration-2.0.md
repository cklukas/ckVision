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

*(Sections for WP-87 through WP-96 will be populated as each work package lands.)*
