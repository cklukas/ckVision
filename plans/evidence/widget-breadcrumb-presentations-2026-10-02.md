<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Breadcrumb presentation verification — 2026-10-02

D-119 defines Plain, Padded and Connected. The last segment is bold independently
of focus; focus adds underline. Padding belongs to the segment hit region;
separators are inert. Width/content/style changes discard prepared layout and
cancel a pending ellipsis press. Shared layout serves paint, navigation and
pointer regions, with disabled and out-of-bounds input rejected.

The first build failed misplaced private declarations. These were corrected
before validation. Release rebuild completed in 63.759 seconds, 150 operations,
with affected common-header callers compiled. Exact hashes/times/output are in
adjacent JSON; compiled/source hashes remained unchanged through validation.

Focused behavior, allocation and documentation gates passed 3/3. Full macOS
CTest passed 216/218 in 22.80 seconds; only two stale reference screenshot gates
failed. Their expected bold/underline diffs and compiled-scene render were
reviewed, references refreshed, and both failed gates passed on rerun.

56 new four-scheme alternate cases and 44 intentional existing breadcrumb
emphasis changes were pinned. Other appearance dumps remained unchanged. All
three styles passed zero steady allocations during paint, navigation and pointer
queries. No running application was launched or visually validated.

Five families and final clean platform, sanitizer, fuzz and benchmark gates
remain. This is partial incremental evidence. No commit was made.
