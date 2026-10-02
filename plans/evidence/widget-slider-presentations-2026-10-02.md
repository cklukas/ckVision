<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Slider presentation verification — 2026-10-02

D-119 defines Line, Block and ProminentThumb with optional unbracketed readout.
Rendering, ticks and pointer mapping use track_bounds(); numeric space is stable
across value changes. Readout and tick labels are inert. Disabled input is rejected.
Tick labels retain priority, spacing, complete graphemes and ellipsis, using
retained scratch ranges prepared at set_ticks rather than allocating in draw.

The initial build detected misplaced private declarations and failed. Those were
corrected before any tests were accepted. Release rebuild completed in 45.657
seconds (160 operations), with the common-components header rebuilding its
callers. Exact input hashes, timestamps and output path are in the adjacent JSON.
Compiled/source input hashes remained unchanged through validation; golden
pins and the catalog were updated afterward.

Focused common-components, allocation and documentation gates passed 3/3. Full
macOS CTest passed 218/218. All three styles with ticks and readout passed zero
steady-paint allocation checks. 84 new four-scheme appearance cases were pinned;
existing appearance dumps stayed byte-identical. Generated scene contact sheets
were reviewed. No application was launched or visually validated.

Seven families and final clean platform, sanitizer, fuzz and benchmark gates
remain. This is incremental macOS evidence, not final completion. No commit made.
