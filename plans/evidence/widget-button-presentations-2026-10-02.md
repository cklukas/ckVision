<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Button presentation verification — 2026-10-02

D-119 defines Classic, Flat, Padded and Outlined. The new styles distinguish bold
default-action emphasis from underlined focus. Caption, style and bounds changes
cancel armed presses. Mnemonic captions are retained at mutation.

Implementation build: 154.090 seconds, 175 operations; concurrent unrelated
compilation observed. Coverage rebuild: 3.102 seconds, five test/docgen operations.
Exact times, source hashes and Release output path are in the adjacent JSON.
Compiled/source hashes remained unchanged during validation. Golden pins and
catalog were updated afterward.

New metrics/cancellation tests and all-style zero steady-paint allocation checks
passed. Full macOS CTest: 218/218 in 21.96 seconds. 108 new four-scheme appearance
cases include default/focus combinations, press, hover, disabled, tiny bounds
and Unicode; previous dumps stayed byte-identical. Compiled scene contact sheets
were reviewed. No running application was launched or visually validated.

Six families and final clean platform, sanitizer, fuzz and benchmark gates remain.
This is incremental evidence. No commit was made.
