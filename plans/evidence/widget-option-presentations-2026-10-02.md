<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Option-group presentation verification — 2026-10-02

D-119 defines Classic, BoxedRows and Buttons for CheckGroup and RadioGroup.
Selection indicators and bold captions remain distinct from underlined keyboard
focus. Boxed choices occupy three rows; button choices occupy one padded row.
Shared layout serves rendering, clicks and pointer shapes, with alternate gaps
and captions inert. Disabled input and key releases cannot change state.

Parsed mnemonic captions and column scratch storage are prepared at construction.
All-style painting/navigation/pointer checks passed zero steady allocations.
Geometry, disabled/release and monochrome contrast tests passed. Visual review
caught vanishing monochrome caption and mnemonic accents; both were corrected,
covered by tests, rebuilt and rerendered before acceptance.

Implementation build: 23.876 seconds, 114 operations. Subsequent coverage,
contrast/docgen, mnemonic and final test builds took 2.676/5, 3.316/68, 2.150/67 and
1.932/2 seconds/operations respectively, consistent with their compile/link scope.
Exact hashes/times/output are in adjacent JSON; compiled/source hashes stayed
unchanged through validation. The test executable timestamp is within the final
build interval. Full macOS CTest passed 218/218 in 22.55 seconds.

112 new four-scheme appearance cases were pinned. Two focused monochrome classic
references changed intentionally for contrast; other existing dumps stayed
byte-identical. Compiled-scene contact sheets were reviewed. No running application
was launched or visually validated.

NotificationCenter, PropertyInspector, SpinBox, Wizard and final clean platform,
sanitizer, fuzz and benchmark gates remain. No commit was made.
