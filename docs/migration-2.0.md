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

*(Sections for WP-86 through WP-96 will be populated as each work package lands.)*
