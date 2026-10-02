<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# PropertyInspector presentation verification — 2026-10-02

D-119 specifies Plain, Divided and Sectioned property layouts, optional owned
contiguous group headings and independently enabled row banding. Prepared row
positions govern rendering, input, validation and editor placement. Section
headings and gaps are inert; keyboard movement visits properties. Changing
presentation retains an open edit. Editor bounds clip to the shared value
rectangle after resize. Disabled direct input is rejected.

All styles borrow complete-grapheme text. Long Unicode painting, keyboard
navigation and pointer queries passed zero steady-state allocations. Behavioral
coverage verifies grouping, validation row insertion, editor preservation and
geometry, navigation, Bool toggles and disabled input.

120 new four-scheme appearance cases were pinned; existing dumps remained
byte-identical. Compiled scene renders of normal, focused, disabled, editing,
refused, choice and narrow states were reviewed. No running app was launched.

The first build stopped on a shadowed specimen capture under warnings-as-errors;
its capture was renamed. The layout refinements then passed a 35.062-second,
150-action build, comparable to the 32.723-second notification build. The final
explicit include and host-hint regression build took 8.559 seconds for 68 actions,
comparable to SpinBox's 8.448-second, 68-action incremental build. Logs show the
common-components and test compilations plus affected relinks. Adjacent JSON
records exact paths, times, source hashes and executable identity; inputs stayed
unchanged through validation and the executable timestamp is within its build.

Final macOS CTest passed 218/218 in 21.01 seconds, including provenance and
self-test. Notification stack-height hints were also verified in this build:
posting, dismissal and presentation changes notify the host; live bounds do not
feed into the hint, and a larger caller-preferred height is respected.

Wizard and the final clean cross-platform, sanitizer, fuzz and benchmark gates
remain. No commit was made.
