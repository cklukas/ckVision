<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Advanced progress displays

Status: implemented, 2026-10-03. D-120 records the contracts. The
[progress guide](../docs/progress.md) documents the public API, and
`ckvision_progress` is the interactive Progress Lab. This document preserves
the research and design rationale; the guide is the delivered reference.

## Findings and visual direction

The useful qualities are legible partial completion, visible activity when the
total is unknown, and a clear relationship between overall work and current
work. Animation should communicate activity without making the interface busy.
No decorative brackets or enclosing frames are required by default.

Published references inspected as documentation, not implementation source:

| Library | Published appearance and behavior | Useful ckVision direction |
|---|---|---|
| [Rich Progress](https://rich.readthedocs.io/en/latest/progress.html) | Configurable description/bar/percentage/time columns; multiple task rows; spinner columns; pulsing unknown-total bars; nested displays | Aligned task rows, optional metadata, clear unknown-total state |
| [tqdm reference](https://tqdm.github.io/docs/tqdm/) | Smooth Unicode block fill; counters, rate, elapsed and ETA; row positioning and bounded nested displays; throttled updates and ASCII alternatives | Fractional-cell meters, stable row identity, economical refresh |
| [alive-progress documentation and exhibits](https://github.com/rsalmei/alive-progress#styles) | Spinner and bar style exhibits; activity independent of measured completion; unknown-total animation; optional detail on a second line | A few restrained spinner/pulse choices and a detail line |

Rich is the strongest reference for a multi-task presentation. alive-progress
is useful for animation choices; its documented dual-line mode is not evidence
for a hierarchical task model. tqdm's nested row positioning likewise does not
define automatic parent aggregation. Hierarchy and aggregation below are our
own specified semantics.

These are illustrative ckVision layouts, not screenshots of those libraries:

```text
Copying          ███████████▍░░░░░░░░  57%

⠹ Discovering files…

Import           ████████████░░░░░░░░  60%   3/5 stages
  Pictures       ███████▍░░░░░░░░░░░░  37%   74/200 files
  Indexing       ⠹ Waiting for pictures

Download         ██████████▍░░░░░░░░░  52%   8.2 MiB/s   12 s left
                 release-assets.tar
```

Actual figures must use ckVision's renderer, themes and measured cell widths.
The default task display shows a description, smooth meter and percentage;
counts, elapsed, rate, ETA and detail are independently selectable. Parent rows
use modest emphasis and children use a two-cell indentation. No tree branches,
emoji, rainbow effects or frames are needed in the default presentation.

## Existing ckVision surface and gaps

`include/cvision/widgets/progress.hpp` already provides the single-row `Progress`
widget: Solid, Block and Segmented presentations, an optional percentage,
overlaid label, fraction, indeterminate sweep and caller-controlled pulse.
`src/widgets/progress.cpp` uses whole-cell rounding. It does not schedule its own
animation. Forms, Workbench and SysInfo already use it.

Missing capabilities are fractional-cell Unicode fill, a compact spinner,
easy animation ownership, counts and timing metadata, stable multi-task rows,
hierarchy and explicitly defined aggregation. Extend the current meter rather
than introducing a competing progress-bar class hierarchy.

## Proposed components and ownership

All names below are proposed API names, to be settled before implementation.

1. **Progress remains the meter.** Add `ProgressPresentation::Smooth` and
   `ProgressActivityStyle::{Sweep,Bounce,Pulse}`. Keep meter geometry separate
   from glyph preference and activity behavior. Existing Solid/Block/Segmented
   remain usable; Smooth is the task display's preferred meter.
2. **ActivityIndicator is a small view.** Offer Dots, Braille and ASCII spinner
   frames, plus static working/completed/paused/failed representations. It can
   be used independently in a status row, dialog or task display.
3. **ProgressModel owns tasks, not views.** Stable task IDs, owned strings,
   explicit parent IDs, completion/total, lifecycle state and timing samples.
   No global registry, automatic terminal output, environment detection or
   worker threads. A view borrows a model; it must detach before the model dies.
   Observations use scoped subscriptions consistent with existing repository
   practice. Multiple views may observe one model.
4. **ProgressView presents the model.** Compact rows or rows with a detail line;
   selectable columns; aligned shared column geometry; bounded visible tasks.
   It reuses the meter/indicator paint helpers rather than allocating a View
   tree for every row. An optional dialog factory composes this view and normal
   Cancel/Close buttons, without duplicating progress logic.
5. **ProgressController supplies time and scheduling.** Construct explicitly
   with an Application and model. Read only the injected Clock. Own one timer
   for all animated rows, coalesce display refreshes and cancel on destruction.
   Standalone meters retain manual phase updates; an explicit binding helper
   makes automatic animation convenient without changing draw-time purity.

Model and pure measurement types live in `widgets` initially, matching the
existing widget-owned document/model pattern. They depend only downward on
`core` contracts. The controller/view may use `ui`; no widget includes platform
backends. Do not introduce an abstract framework for all future dashboards.

## State and main/subprogress semantics

- Lifecycle: Queued, Running, Paused, Completed, Failed, Cancelled. Disabled is
  a View interaction/style property, not a task lifecycle state.
- A missing total means unknown work; it never displays a fabricated percentage
  or ETA. A queued unknown task is static; a running unknown task animates.
- A zero total is an empty known workload, with no division; only explicit
  completion marks it finished. Reaching a numeric total does not silently
  turn a running task into Completed. Display counts may exceed total; meter
  fraction clamps to one. Increasing totals may legitimately reduce fraction.
- `reset` starts a new timing epoch; pause excludes paused time. Failed and
  cancelled tasks retain actual partial completion and an explicit state label.
- Parent progress defaults to **Manual**. A main task can count five stages
  while its active child counts bytes; unrelated units must not be summed.
- Optional **WeightedChildren** aggregation normalizes explicit positive child
  weights. Each known child contributes weight times fraction. Unknown children
  make the exact aggregate unknown; optionally show a separately labelled known
  lower bound, never an exact-looking percentage. Completed children contribute
  one. Failed/cancelled children retain their measured fraction and propagate a
  visible exceptional-state summary, rather than being counted as completed.
- Parent state is explicit; child summaries do not silently finish a parent.
  Aggregate ETA is unavailable unless a separate caller-supplied estimate exists;
  summing child ETAs is invalid for concurrent work.
- Adding/removing a weighted child changes the denominator and may move the
  aggregate backwards. Document this, and provide a finalized child-set option
  for operations needing a fixed plan. Reject cycles, stale IDs and invalid
  weights without partially mutating the model.
- Stable insertion order, explicit hide/remove operations and configurable
  retention of finished children prevent rows jumping on each update. Completed
  children can collapse to a summary; failed/cancelled children remain visible.

## Rendering, Unicode and restrained motion

Smooth meters use Unicode Block Elements: U+2588 FULL BLOCK and U+258F through
U+2589 for increasing eighths. Derive the fill from fraction times cell width,
with specified rounding and exact empty/full endpoints. Do not round the entire
bar to full cells. Use the existing central width and grapheme rules.
Authoritative glyph reference: [Unicode Block Elements chart](https://www.unicode.org/charts/PDF/U2580.pdf).

Prepare the small spinner frame tables and formatted strings outside drawing.
Validate custom frames by measured grapheme/cell width and pad to a stable frame
width. Never slice UTF-8 bytes or a wide glyph at the edge. Braille is optional:
font quality and terminal width policy vary. Provide explicit Unicode/ASCII
configuration and a static reduced-motion mode; do not infer glyph support from
terminal color capability or probe the environment in widgets.

Animation is driven by an explicit elapsed duration, not by the number of
progress updates. The controller uses a 100 ms default refresh interval (a
proposal, not a measured performance claim). Compute current phase after a late
callback and schedule one future callback; never catch up by replaying frames.
Only active animated displays need a timer. Detachment, invisibility and static
mode suspend repaint work; use existing effective-visibility APIs, or add a
documented visibility lifecycle notification before implementing this rule.

Determinate bars need no perpetual movement. A small optional spinner may show
that an operation is running even when its completed amount has not changed.
Sweep, bounce and pulse have deterministic phase/width behavior at tiny sizes.
Completion stops movement immediately; paused tasks remain static. No keyboard
focus adornment is painted on passive meters.

New semantic roles, only where needed, distinguish activity, muted track,
parent emphasis and exceptional states. Reuse current progress roles where
their meaning already fits. State text/shape remains readable in Mono, without
relying on color. Measure all text and choose column widths across the visible
rows. On shrink, drop optional rate/ETA/elapsed/count columns in a documented
order, then shorten description; retain state and useful progress. On a single
tiny cell use a state/activity marker. At zero width paint nothing.

## Counts, rate, ETA and update costs

Use owned unit labels, explicit decimal/binary scaling, locale-independent
formatting and bounded precision. Percent/count remains exact model data;
rate and ETA are explicitly estimates. Samples have explicit monotonic elapsed
times. Specify a bounded estimator with a documented warm-up, pause/reset
behavior and stale-rate handling before writing it. Nonpositive sample
intervals do not divide; regressing work resets estimation. A stalled task must
eventually stop showing an optimistic ETA; display unavailable rather than
inventing a finish time. Cap the history and avoid unbounded sample growth.

All model mutation occurs on the owning UI thread. Workers submit owned updates
through `Application::post`; they never touch views or borrowed model strings.
For high-frequency work, provide a bounded latest-value mailbox with explicit
owner lifetime and at most one pending wakeup. Batch updates atomically and
invalidate affected rows once. Avoid allocating per animation frame and avoid
rendering offscreen task rows. Rate/ETA text updates only when displayed values
change. No locks or hidden background work are added to the basic Progress.

## Ease-of-use target

The high-level API supports this shape without the caller implementing timers,
rate estimation, indentation or column measurement:

```cpp
widgets::ProgressModel tasks;
widgets::ProgressController progress(app, tasks);
auto* display = content.make<widgets::ProgressView>(tasks);
display->set_presentation(widgets::ProgressViewPresentation::Compact);
progress.bind(*display);

auto main = tasks.add_task({.title = "Import", .total = 5, .unit = "stages"});
auto pictures = tasks.add_task({.title = "Pictures", .parent = main,
                                .total = 200, .unit = "files"});
tasks.start(main);
tasks.start(pictures);
tasks.advance(pictures, 74);
tasks.set_message(pictures, "holiday-2026.jpg");
// Later, after the stage actually completes:
tasks.complete(pictures);
tasks.advance(main, 1);
```

Also document standalone Smooth Progress, an unknown-total ActivityIndicator,
switching unknown to known total, weighted child progress, concurrent downloads
and UI-thread delivery from a worker. Cancellation is a request callback owned
by the operation; clicking Cancel must not falsely report that work has stopped.

## Implementation sequence and acceptance

1. Record the new behavior in DECISIONS and reference docs before public API
   implementation. Pin the glyph, state, aggregation, estimator and clipping
   contracts. Review names and ergonomic examples; update all existing callers
   if a design is superseded (pre-1.0 has no compatibility debt).
2. Implement Smooth meter and ActivityIndicator with pure phase rendering,
   Unicode/ASCII/static modes and shared paint geometry. Keep standalone Progress
   small. Add exact fractional-fill and animation-phase goldens.
3. Implement model state, scoped observation, parent relationships and weighted
   aggregation; then explicit-time estimation. Test exceptional and zero/unknown
   cases before composing the multi-row view.
4. Implement responsive ProgressView, controller lifetime/timer ownership,
   batched updates and the bounded worker bridge. Add real example integration
   in Workbench and a main/subprogress example in SysInfo's benchmark workflow.
   A dedicated small Progress example should expose every proposed presentation
   and mode interactively without depending on real network operations.
5. Publish compiled examples and generated figures in each new widget's section
   of the widget gallery. Show all meter/activity/layout variants in one scheme;
   show known/unknown, hierarchy and pause/failure separately as state examples.
   Add a generated animation sequence or browser-playable exhibit if supported
   by ckdocs; static phase strips remain the portable reference.
6. Run clean warning-free macOS/Linux/Windows builds, all unit/script/golden/docs
   and provenance gates, fuzz corpus, sanitizers, and existing performance gates.
   Benchmark many active/hidden rows, high-frequency coalesced updates, resize,
   timer lateness and idle behavior against a recorded comparable baseline.

Required coverage includes every eighth-cell boundary, width 0/1/tiny/full,
negative/nonfinite inputs, complete Unicode graphemes, injected width policy,
all four schemes, animation wrap/late callbacks, no timer after detach/complete,
pause/resume, changing totals, nested mixed units, unknown weighted children,
cycles/stale IDs, model/controller destruction order, failed cancellation,
no stale cells after collapse, and zero steady-frame allocations in prepared
draw paths. Fuzz hierarchy mutations, validated frame sequences and clipping.

Acceptance: clients can show determinate, unknown-total and main/subprogress
with a few explicit objects and updates; output stays deterministic and quiet;
existing Progress remains lightweight; examples, variants and lifecycle rules
are documented and executable evidence verifies the ckVision specification.
No outside implementation's code or faithfulness is validation evidence.
