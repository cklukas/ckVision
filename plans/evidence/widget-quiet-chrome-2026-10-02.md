<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Restrained tab and toolbar chrome — 2026-10-02

D-119 refines D-117/D-118 to the owner's preference against decorative brackets.
Compact tabs distinguish selection with color and bold weight, and focus with
an independent underline. Compact/Padded toolbar faces and overflow use blank
insets, retaining their widths, hit rectangles, grouping and overflow geometry.
Framed containment and meaningful checkbox/toggle markers remain available.

Focused interaction, allocation and documentation gates passed 5/5. 96 toolbar
and 40 compact-tab appearance cases were refreshed; four SVG references changed.
Compiled scene renders were reviewed across Classic, Dark and Mono, including
focus and grouped command bars. No running application was launched.

The final Release build took 36.770 seconds for 151 actions, comparable to the
35.062-second property and 32.363-second wizard builds of approximately 150
actions. Adjacent JSON records exact output, times and input fingerprints.
Final macOS CTest passed 218/218 in 21.33 seconds, including provenance/self-test
and deterministic budget gates. All thirteen table families are implemented;
final clean platform, sanitizer, fuzz and wall-clock benchmark checks remain.
No commit was made.
