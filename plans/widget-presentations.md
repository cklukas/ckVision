<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Widget presentation improvements (D-119)

The complete scope is the owner's approved thirteen-row widget review. The
implementation may improve an existing function or add an explicit presentation;
completion requires every row, not only the first implementation batch.

| Family | Required result | State |
| --- | --- | --- |
| InputLine, ComboBox, SearchBox | Flat, Padded and Underlined fields; visibly separated dropdown/clear controls; shared content/caret/hit geometry | Implemented; platform gates passed; pinned timing pending |
| Progress | Solid, Block and Segmented meters; optional adjacent percentage; dedicated roles | Implemented; platform gates passed; pinned timing pending |
| NotificationCenter | Lines, Banners and Framed cards; severity identity, padding and dismissal affordance | Implemented; platform gates passed; pinned timing pending |
| PropertyInspector | Plain, Divided and Sectioned layouts; optional owned group headings and banding; editor geometry follows layout | Implemented; platform gates passed; pinned timing pending |
| Slider | Line, Block track and Prominent thumb; optional value readout; tick semantics retained | Implemented; platform gates passed; pinned timing pending |
| CheckGroup, RadioGroup | Classic markers, Boxed rows and Button groups; selection distinct from focus | Implemented; platform gates passed; pinned timing pending |
| SpinBox | Compact, Separate steppers and Stacked controls; unbracketed edit and step regions explicit | Implemented; platform gates passed; pinned timing pending |
| Button | Existing classic/flat behavior plus Padded and Outlined presentations; default emphasis distinct from focus | Implemented; platform gates passed; pinned timing pending |
| StatusLine | Plain and Grouped segments; unbracketed hotkeys precede labels with color/attribute accent; command regions and status message separated | Implemented; platform gates passed; pinned timing pending |
| BreadcrumbBar | Plain, Padded and Connected segments; location distinct from focus; overflow retained | Implemented; platform gates passed; pinned timing pending |
| Splitter | Line, Central grip and Wider handle gutter; hover/focus feedback; drag geometry shared | Implemented; platform gates passed; pinned timing pending |
| ListView, Table | Plain and Banded rows; optional Divided table columns; existing model/style/selection contracts retained | Implemented; platform gates passed; pinned timing pending |
| Wizard | Compact, Header/footer bands and Step rail; navigation uses button presentation; validation and focus contracts retained | Implemented; platform gates passed; pinned timing pending |

Each family keeps one behavior implementation and explicit instance-owned
presentation configuration. Geometry belongs to the widget; colors and
attributes belong to semantic theme roles. Colored flat fields remain the default. Decorative square brackets around
fields, values and shortcut hints are explicitly excluded by owner preference.
Alternate chrome must improve spacing, focus, grouping or containment; meaningful
checkbox markers are retained. Classic defaults remain available.
Presentations never change silently because of small bounds: they clip safely,
with nonnegative content rectangles, complete-grapheme text and deterministic
layout. Size changes relay size hints and invalidate prepared geometry. An
armed action is canceled when its geometry changes. All variants must retain
keyboard access, accurate pointer regions and enabled/invalid/read-only state.
Focus remains visible without color, and is independent of checked/selected or
default-action state. No new external dependency or mutable global facility.

The visual standard is sophisticated and elegant without crowding or complexity.
Defaults use restrained color, consistent alignment and deliberate whitespace.
Avoid redundant signals: a colored input surface needs no decorative enclosure,
and an accented hotkey needs no keycap brackets. Borders and separators serve
actual grouping or containment; they are not applied to every element. Alternate
presentations remain opt-in and must justify their extra cells through clearer
hierarchy, focus or interaction. Keep inactive chrome quiet and reserve emphasis
for the current location, selected state and actionable focus.

Each row requires public API/reference documentation, golden states across the
four appearance schemes, behavioral tests for changed geometry/input and steady
allocation coverage where hot paths change. The final gate set is the project
standard: warning-free clean macOS/Linux/Windows builds, all suites, provenance
and self-test, fuzz corpora, ASan/UBSan/TSan and benchmark budgets. Compiled scene
renders prove scene appearance only; a claim about a running application requires
inspection of its verified user-facing window.

Final evidence: [widget-presentation verification](evidence/widget-presentations-final-2026-10-02.md).
The absolute latency limits passed in a diagnostic run under recorded host load.
That run does not establish the separate idle-host timing baseline.
