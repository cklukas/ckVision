<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# SpinBox presentation verification — 2026-10-02

D-119 defines unbracketed Compact, Separate and Stacked fields and steppers.
Shared rectangles govern rendering, caret and activation. Field/gap clicks do
not step. Keyboard/wheel adjustment, validation and callbacks remain shared.
Focus underlines the value; accessory roles provide hover and unavailable-limit
feedback. Editable fields and refusing controls report appropriate pointers.
Range changes repaint availability even when the value remains unchanged.

Long entries borrow complete-grapheme suffixes. Numeric values use a local
locale-free to_chars buffer and explicit ellipsis. All three styles passed zero
steady allocations during long Unicode entry painting, focused caret calculation
and pointer queries. Geometry, disabled input, editability, limits, tiny bounds
and composed-frame range-change regression tests passed.

Build details and exact hashes/times/output are in adjacent JSON. The final
availability fix took 8.448 seconds, 68 compile/link operations. Logs confirm
common-components and regression-test compilation plus affected executable links.
Compiled/source/reference hashes stayed unchanged during validation, and the test
executable timestamp lies within that successful build interval.

120 new four-scheme cases were pinned. 28 default SpinBox and 32 embedded
DateDialog references changed intentionally. Two Forms raw dumps and seven SVG
references were reviewed and refreshed. Initial full CTest found stale unit,
documentation and fixture references; these were repaired, followed by a full
218/218 pass. The subsequent repaint fix was regression-tested and the final
macOS CTest passed 218/218 in 21.11 seconds. Compiled-scene variant, DateDialog and
Forms images were reviewed. No running app was launched or visually validated.

NotificationCenter, PropertyInspector, Wizard and final clean platform,
sanitizer, fuzz and benchmark gates remain. No commit was made.
