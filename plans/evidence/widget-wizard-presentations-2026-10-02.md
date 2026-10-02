<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Wizard presentation verification — 2026-10-02

D-119 defines Compact, Bands and StepRail. Shared clipped rectangles place page
content and owned Flat/Padded Buttons. Step markers are informational; they do
not bypass validation. Wizard focus underlines its title, independently of
Button focus and default emphasis. Numbered rail captions and step formatter
results are prepared on content/label changes. Three independent surface roles
bring the standard role count to 112.

Behavioral tests cover shared content geometry, field focus transitions,
Down/Up drag cancellation, validation revocation during a press, release-key
activation and Next/Finish focus transfer. Layout changes and page replacement
cancel armed input. Clipped-out controls leave traversal; shrinking a focused
button away restores wizard focus. Disabled ancestors prevent activation and
pointer cues. All layouts pass zero steady allocations for painting (including
children), size hints and next/back transitions with long Unicode captions.

156 new appearance cases and 40 updated Wizard cases were pinned across four
schemes. Two Forms raw goldens and five SVG references were refreshed. Compiled
scene renders and Forms headless captures were reviewed, including Classic
heading contrast, focused/blocked/disabled states, narrow bounds and content.
No running app was launched or visually validated.

The first build stopped on test assertions using an unsupported Attr operator;
the second stopped on a specimen calling a private hover setter. Both were
corrected to public APIs. The final 150-action Release build took 32.363 seconds,
comparable to the 35.062-second property and 32.723-second notification builds.
Logs identify affected inputs and links. Adjacent JSON records exact times,
source fingerprints and executable identity; compiled inputs stayed unchanged
through validation, and the executable timestamp is within its successful build.

Forms traversal tests now include Cancel and retain button focus after completion.
The example relays Name edits to refresh_navigation and uses InputLine::empty()
without materializing text. The initial full suite found a stale palette table
and source snippet; both were regenerated. Final macOS CTest passed 218/218 in
19.65 seconds, including provenance/self-test and deterministic budget gates.

All thirteen table families now have incremental macOS evidence. The final
style coherence audit and clean cross-platform, sanitizer, fuzz and benchmark
verification remain. No commit was made.
