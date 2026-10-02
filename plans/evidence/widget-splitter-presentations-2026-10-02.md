<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Splitter presentation verification — 2026-10-02

D-119 and plans/widget-presentations.md define Line, CentralGrip and Gutter.
Drawing, pointer shape and press handling share clipped divider geometry. Gutter
movement preserves grab offset. Disabled input and key releases are rejected;
reconfiguration cancels dragging. Hidden panes contribute no size hint or divider.
Tiny gutters keep pane bounds within the widget.

Release incremental build completed in 27.384 seconds (79 operations), consistent
with the affected splitter header callers. Exact output, timestamps and input
hashes are in the adjacent JSON. Splitter, standard-role and steady-allocation
suites passed (3/3). Full macOS CTest passed 217/218 in 26.66 seconds; its only
failure was the missing public enum comment. That comment was added and the
failed documentation gate passed on rerun. The comment postdates this build;
final clean builds remain required.

64 new appearance dumps cover both alternatives and four schemes. Existing
appearance dumps remain byte-identical. The generated scene contact sheet was
reviewed. No running application was launched or visually validated.

This is partial evidence. Eight widget families and the final clean platform,
sanitizer, fuzz and performance gates remain. No commit was made.
