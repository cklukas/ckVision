<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Widget presentations: first implementation batch, 2026-10-02

Scope: D-119 InputLine/ComboBox/SearchBox, Progress and StatusLine. This is
partial implementation evidence; the full thirteen-row goal remains active.

- Fields support Flat, Padded and Underlined surfaces without ornamental brackets.
  Geometry controls caret, selection, Unicode scrolling, pointer regions and size
  hints. The editable ComboBox has a real child editor and one focus stop.
  SearchBox uses a padded bare `x`, with independent accessory/hover roles.
- Progress supports Solid, Block and Segmented meters with optional determinate
  percentage and independent track/fill/label/disabled roles.
- StatusLine keeps Plain as default and offers explicit Grouped separators.
  Leading chords retain their accent without brackets. Geometry changes, registry
  revisions and focus-origin changes cancel held commands. Captions, layout and
  provider hints are prepared and reused; `refresh_hint()` announces host mapping
  changes. Painting, pointer queries and repeated activation allocate nothing
  after preparation. Tiny bounds clip both text and chord accents.

The current macOS Release incremental build passed all 218 CTest gates, including
provenance and checker self-tests, the complete appearance matrix, raw generated
bytes, documentation, package/install, architecture and benchmark budget checks.
The final run took 23.19 seconds. Changed geometry has behavioral coverage, and
fields/meters/status have zero-steady-allocation coverage. New appearance states
add 272 dumps across Classic, Dark, Light and Monochrome. Existing composite
screens and goldens were regenerated where their embedded fields changed.

Compiled scene SVGs were reviewed, including the input/progress alternatives and
`docs/generated/screenshots/widget-statusline-grouped.svg`. No app was launched;
this is evidence of scene rendering, not a claim about a running user window.

The exact build path, executable identity/timestamp, implementation manifest
fingerprint, build timings and new golden counts are in the adjacent JSON.
No comparable incremental baseline was available for the first input rebuild:
its log records 282 compile/link operations after shared public headers changed.
Comparable StatusLine rebuilds took 23.70 and 22.73 seconds (131 operations).
The final 4.76-second rebuild has 68 operations, predominantly relinks, with two
changed translation units compiled; its short duration is accounted for by the
actual inputs and cached objects. No failed build or stale output was launched.

Outstanding: the ten other widget-review rows and the final feature-frozen clean
macOS/Linux/Windows, fuzz corpus and sanitizer gates. Prior toolbar platform runs
do not cover these newer changes. No commit has been made for this batch.
