<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Notification presentation verification — 2026-10-02

D-119 defines compact Lines, padded Banners and optional Framed cards.
Severity markers and accents, focused newest content, muted disabled state,
unbracketed dismissal controls and transparent inert gaps were reviewed in
compiled scene renders. No running application was launched.

Shared rectangles govern paint and dismissal. Behavioral tests verify control
hits, inert body/gaps, disabled input and persistent entries. Long Unicode paint
and pointer queries allocate zero steady-state memory in all presentations.

The recorded Release build completed successfully in 32.723 seconds (150
compile/link operations), comparable to the previous 37.540-second notification
build with the same operation count. Adjacent JSON records source fingerprints,
exact times and output. Focused CTest passed 4/4; full macOS CTest passed 218/218
in 22.30 seconds, including provenance and self-test. 72 new appearance cases
were pinned across four schemes; existing dumps remained byte-identical.

PropertyInspector, Wizard and the final clean platform, sanitizer, fuzz and
benchmark gates remain. No commit was made.
