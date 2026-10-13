<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Live log monitoring work packages

Status: implementation scheduled by the owner, 2026-10-12. D-140 adopts
[live-log architecture](live-log-architecture.md). The
[corrective assurance queue](corrective-assurance.md) owns package states.
Load-deferred verification carries to the next package, through WP-84 at the
latest, under the owner's instruction; it is never recorded as passing.

ckVision should let a client monitor a growing log with a small composition of
shipped services, model and widget: open a source, follow updates, filter and
search, inspect older entries, and resume the live tail. A runnable log-monitor
demo must offer the same workflow on macOS, Linux and Windows, including native
system-log sources where available. Sustained input faster than terminal output
must remain bounded, responsive and explicit about lag or lost records.

## Existing support and the missing composition

The present public surfaces supply useful parts:

- `TextView` supplies styled read-only text, wrapping, links and scrolling,
  but `set_text()` and `set_spans()` replace content; it has no dedicated
  append/retention/filter/source contract.
- `ScrollViewport::scroll_to_bottom()` supplies a scrolling operation, not a
  follow/pause state machine or unread accounting.
- Provider-backed `ListView` and `Table` preserve identities through refresh
  and filtering. Their providers are application-owned, queried on the UI
  thread, and do not themselves ingest or follow logs.
- `Application::post()`, `wake()`, timers and the injected clock supply
  scheduling primitives. Posting one callback per log line does not establish
  bounded live-log delivery.
- `FileSystem::read_file()` reads a whole file; its fingerprint identifies a
  version, not an open-file identity/offset or file-change subscription.
- The directory discovery service supports bounded discovery publications,
  not continuous content monitoring. Captured process helpers return a bounded
  result for a completed invocation, not a continuous streaming subscription.

Reuse these mechanisms and their ownership rules. Do not require each adopting
application to build its own tailer, rotation handling, filter index, overload
policy or scroll-follow logic. See [data views](../docs/data-views.md),
[provider architecture](data-flow-architecture.md),
[process helpers](../docs/process-helpers.md), ARCHITECTURE §§1, 5, 8–9 and D-039.

## Documented competitive baseline

These published interfaces establish useful product expectations; ckVision's
contracts and executable evidence define acceptance.

| Published capability | Consequence for this plan |
|---|---|
| Textual [Log](https://textual.textualize.io/widgets/log/) documents realtime append, line retention limits, selection and automatic scrolling; [RichLog](https://textual.textualize.io/widgets/rich_log/) documents styled content and highlighting | Ship a dedicated reusable log surface with append, retention, follow and inspect behavior |
| [dialog tailbox and tailboxbg](https://invisible-island.net/dialog/manpage/dialog.html) document growing-file display and background updates, including polling while other widgets operate | Ship usable file-follow composition and ensure other controls remain interactive |
| Ratatui's [Paragraph guide](https://ratatui.rs/recipes/widgets/paragraph/) documents styled text, wrapping and scrolling | Reuse ckVision's text/scroll machinery, but make source following and log policy explicit library capabilities |

This is a capability baseline, not a claim that every competing library ships
native file watchers, rotation recovery, structured filtering or identical
performance. Technical behavior is derived from ckVision specifications and
published platform/Unicode/terminal documentation. Never consult prohibited
implementation source. No competing library becomes a dependency.

## Target composition and scope

Proposed names below describe responsibilities; WP-73 settles the public API.

- **Core contracts:** source identity/generation, byte ranges, bounded batches,
  source status, cancellation and explicit delivery/overflow policies.
- **Term adapters:** native file observation and range I/O; continuous private
  pipe sessions where needed. They depend only downward and own native resources.
- **Widgets models:** deterministic bounded `LogModel`, decoder, filter index
  and controller over injected services. No filesystem/environment/clock access.
- **Widgets presentation:** `LogView` and a compact `LogPanel` composition with
  filter/search, status and follow controls. No terminal sequences from widgets.
- **Demo application adapters:** platform-specific log discovery, metadata
  mapping and native system-log subscriptions. OS log schemas and source-choice
  policy stay out of generic widgets; reusable low-level services stay in `term`.

The basic path accepts plain UTF-8 lines, explicit styled entries and injected
structured records. Preserve source bytes/metadata separately from safe display
text. Optional parsers have bounded record sizes and instance-owned registration.
Plain-text filtering and live file monitoring need no external dependency.
Network collection, remote fleet management and durable log storage are outside
this delivery. Optional disk-backed history needs a separate explicit storage
contract; bounded live retention is required here.

## Delivery order

| Package | Deliverable | Prerequisites |
|---|---|---|
| WP-73 | Authority, public contracts and acceptance budgets | Existing layer, event-loop and provider contracts |
| WP-74 | Bounded incremental log model and text decoding | WP-73 |
| WP-75 | File identity, range reads and change notifications on three platforms | WP-73 |
| WP-76 | Reliable live file follower | WP-74, WP-75 |
| WP-77 | LogView interaction and LogPanel composition | WP-74 |
| WP-78 | Incremental filtering, search and structured-field predicates | WP-74, WP-77 |
| WP-79 | Overload handling, bounded scheduling and performance | WP-76, WP-77, WP-78 |
| WP-80 | Continuous process-output source service | WP-73, WP-74, WP-79 |
| WP-81 | Cross-platform log-monitor demo | WP-76…WP-79 |
| WP-82 | Native macOS, Linux and Windows demo sources | WP-80, WP-81 |
| WP-83 | Client reference, recipes and demo documentation | WP-81, WP-82 |
| WP-84 | Three-platform reliability, performance and release evidence | WP-73…WP-83 |

WP-73 sets estimates after bounded API prototypes and measurements; this plan
does not invent effort or throughput baselines. Contract work precedes adapters;
the portable file demo can land before native system-source integration. Package
status follows the existing queue rules when execution is scheduled.

## WP-73 Contracts and authority

Record the design in DECISIONS and update affected architecture, roadmap and
widget catalog before implementation. Specify borrowed/owned lifetimes, UI-thread
publication, source generations, record identities, cancellation, recovery,
retention and the exact meaning of follow versus paused presentation.

Specify defaults and hard limits for bytes/records per batch, record length,
decoder state, retained text, pending publications, filter work and redraw rate.
Reject configurations that cannot fit their mandatory bookkeeping. Settle
case-folding and regular-expression semantics against deterministic Unicode and
bounded-work requirements; no locale-sensitive matching or unbounded regex on
the UI thread. A new dependency requires a decision first.

Acceptance: a public-header-only composition sketch covers file following,
in-memory append, worker delivery, filter changes and teardown. A traceability
table ties each behavior and budget to a test/golden/benchmark. All decisions
below become explicit contracts, not application-specific exceptions.

## WP-74 Incremental model and decoding

Implement append batches, stable source/generation/record identities, bounded
retention, eviction notification and metadata. Incrementally decode UTF-8 and
LF/CRLF across arbitrary byte chunks. Define lone-CR progress updates, malformed
UTF-8, BOM/explicit encoding handling, unfinished final lines and oversized lines.
Partial-line updates revise one identity; truncation/rotation starts a new
generation. Provide explicit multiline-record grouping without guessing a
format from ordinary text. ANSI styling, if offered, is a restricted parser;
OSC/clipboard/cursor/private terminal controls never pass through as instructions.

Acceptance: chunk-boundary permutations yield identical records/frames. Tests
cover wide/combining/emoji text, embedded controls, empty files, long records and
eviction with surviving identities. Limits include partial records and indexes;
retention cannot grow indefinitely through metadata or deferred decode state.

## WP-75 Native file observation and bounded reads

Supply injected contracts for opening a regular file, native identity/size,
bounded offset reads and change hints. Include a fake adapter with scripted
notifications and identity changes. Implement macOS vnode/kqueue observation,
Linux inotify and Windows directory notifications with cancellation and explicit
sharing rules. Watch the containing directory when path replacement requires it.
Notifications trigger reconciliation; they are never assumed to be one event
per append. A configurable, injected-clock polling mode covers unsupported or
unreliable notification routes and periodic reconciliation covers missed hints.

Acceptance: real-host append, rename, replace, delete/recreate and permission
failures pass on all three systems. Overflow/lost-watch/fallback states are
observable and recoverable. Opening/reading/stat operations never block UI
dispatch. Document filesystem limits, including network mounts and same-size
rewrites; unsupported fidelity is explicit.

Platform authorities: Apple's [kernel queue guide](https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/FSEvents_ProgGuide/KernelQueues/KernelQueues.html),
Linux [inotify manual](https://www.man7.org/linux/man-pages/man7/inotify.7.html),
and Microsoft [ReadDirectoryChangesW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw).

## WP-76 File following and recovery

Compose notification, range-read and model contracts into a reusable follower.
Default to following the pathname through rotation; also offer explicit
follow-open-file behavior. Start with a bounded recent tail or from an explicit
offset; discovering the last lines of a multi-gigabyte file must not read the
whole file. Define backward-scan limits and initial partial-record treatment.

Handle append, copy-truncate, rename-and-create, atomic replacement, missing
paths, retry, permission changes and source switching. Register observation and
reconcile identity/size around initial reads to close startup races. Set a
bounded old-generation drain policy when rotation overlaps active writes.
Detect observable rewrites/truncation; a truncate-and-regrow between observations
cannot be promised lossless without producer cooperation. Show uncertainty/gaps
when continuity cannot be proven. Retire stale reads and callbacks by generation.

Acceptance: scripted interleavings prove no duplicate/skipped bytes during
ordinary append and supported rotation sequences. Reconcile after missed/coalesced
notifications and sustained growth. Destruction/source replacement during reads
cannot publish into a retired model or wait indefinitely on the UI thread.

## WP-77 Follow and inspect interaction

Implement a virtualized `LogView` with keyboard/mouse scrolling, horizontal
inspection or optional wrapping, selection/copy, stable anchors and styled
records. Default to follow; scrolling upward pauses automatic scrolling while
ingestion continues. A visible Resume/End command returns to the tail. Show
new-entry counts while paused. Resizing, filtering and arrivals preserve the
reader's identity-based anchor and selection whenever retained and eligible.
Eviction of an inspected entry must be visible and clamp deterministically.

Compose a `LogPanel` with SearchBox, status and standard commands; controls work
with dark/light/classic/mono themes and narrow windows. Distinguish paused
presentation from stopping the source, clearing the view and clearing retention.

Acceptance: goldens cover following, pause/arrival/resume, wrapped/unwrapped
scrolling, selection under append/filter/eviction, empty matches, source error,
resize and theme changes. Visible-row work cannot traverse all retained records.

## WP-78 Filtering and search

Ship include/exclude literal filters, explicit case sensitivity, severity/source
predicates and a caller-supplied structured-field matcher. Filter visibility and
search navigation are separate operations. Highlight matches, provide next/previous
search, raw-record detail and before/after context without discarding filtered
records from retention. Missing metadata remains unknown, not an inferred level.

Evaluate new records incrementally. Changing a query creates a revision and a
cancellable bounded rebuild; continued arrivals join the new query without a
lost interval. Keep the previous coherent result until replacement, marked with
pending state, or show an explicitly incomplete result. Never label old matches
as final results of new text. Optional regex requires bounded execution and
specified syntax/errors before shipment.

Acceptance: fake scheduling interleaves append, eviction and query replacement;
only current-revision results publish. Stable selections survive where eligible.
Matching Unicode and multiline text is deterministic. Search does not scan an
entire history in input dispatch or paint.

## WP-79 Overload handling and optimal performance

Treat producer rate, processing rate and terminal delivery rate independently.
Ingest/order records within explicit quotas; render the latest coherent state
at a configurable frame deadline. Coalescing frames does not drop retained log
records. Maintain at most one pending UI drain notification per source/session;
repeated arrivals extend a bounded queue rather than posting per-line callbacks.
A bounded drain yields to keyboard/mouse/timers and reschedules only when needed.
Terminal backpressure never turns a worker publication into an unbounded queue
or causes an immediate repaint for every append.

Default live monitoring retains a bounded recent window and reports eviction.
For file backlog, defer reads and retain offsets while the underlying bytes
remain available; report lag and rotation-related gaps. For a non-replayable
stream, choose explicit backpressure or drop-oldest/drop-newest behavior, count
loss and display gap markers. Do not block an arbitrary producer by default.
Account for queues, retained bytes, indexes, wrapped-row caches and snapshots
together. No bounded-memory, non-replayable consumer promises lossless indefinite
operation when its producer is permanently faster.

Optimize incremental append/filter/eviction and only visible-row layout/paint.
Use cached widths/spans and bounded reusable batch storage; do not concatenate,
copy, rewrap or refilter the whole history on every update. Keep the existing
damage/cell-diff pipeline, host pacing and ARCHITECTURE §8 allocation/frame
budgets. Audit size/counter arithmetic and fair scheduling across sources.

Acceptance: operation-count gates prove append cost depends on the new batch
and bounded eviction, painting on the visible slice, and filter rebuilds on an
explicit work quantum. A slow-output terminal double proves bounded outstanding
frames/bytes, coalescing to the latest state, progress for input and eventual
tail convergence after the producer stops. Fake-clock tests prove redraw limits,
lag/drop counters and declared queue/memory ceilings under prolonged overload.

Benchmarks use small/large histories, long lines, Unicode, low-match filters,
rapid query changes, multiple sources and 1×/10×/100× the measured sustainable
delivery rate. Record records/s and bytes/s for ingestion and matching, peak
memory, CPU/idle wakeups, frame/output-byte counts, source-to-visible p50/p95/p99
lag and input latency under load. Pin host/configuration and separate parsing,
publication, layout, composition and terminal write time. WP-73 sets numerical
limits from project baselines and product requirements; WP-84 ratchets them.
The existing pinned-host input p99 < 2 ms contract remains a gate, with load
conditions stated. Terminal-limited visibility is reported separately.

## WP-80 Continuous process-output sources

Add an instance-owned cancellable streaming pipe service if required by native
source adapters. Reuse the process launch model, native argv/environment handling
and containment; finite `ProcessRunner::run()` is not a live-feed API. Expose
bounded stdout/stderr chunks, exit/error status and explicit overflow policy.
Never emulate arbitrary process output as a TerminalView merely to capture logs.
UI teardown must release/terminate owned helpers according to the specified
contract without blocking. Test a producer whose stderr fills or descendant
holds a pipe open. Preserve stream identity; merged arrival order is not a
claim of original cross-stream causal order.

Acceptance: sustained output, EOF, nonzero exit, cancellation and blocked sinks
pass on POSIX and Windows; argv data remains data. No helper's bytes reach the
host terminal directly, and no orphan helper survives demo exit.

## WP-81 Cross-platform log-monitor demo

Ship `ckvision_logmonitor`, a useful example requiring no application-side
reimplementation of the follower or filter engine. Launch into a source picker
with Open File, platform sources and a deterministic Sample Stream. Support
multiple named sources/tabs, shared filter controls, severity/source filters,
follow/pause/resume, unread counters, details, copy and export of selected or
filtered retained records through the injected filesystem. Export is explicit
and reports its retention/filter scope. Keep source ordering/arrival metadata
honest instead of inventing comparable timestamps.

Show source state, notification/polling mode, backlog/lag, retained/matching
counts, evictions/drops, reconnect and errors. Sample Stream supports steady,
burst and overload modes plus rotation/truncation scenarios, driven by injected
time. Any generated files use an explicit caller-selected scratch directory.
Paths/tool arguments are not shell command strings. Start under ordinary user
permissions; selecting an unavailable source presents recovery choices.

Acceptance: a single app workflow operates on real growing files on macOS,
Linux and Windows. Headless scripts cover the source picker, tabs, filtering,
pause/resume, overload indication, export, failure/retry and source teardown.
The sample route works without system log permissions or installed helper tools.

## WP-82 Native system-log sources in the demo

| Host | Proposed adapter | Capability and fallback |
|---|---|---|
| macOS | Explicit `/usr/bin/log stream` process with a verified structured output mode; optional bounded recent history | Map timestamp, process, subsystem/category and message type; preserve redaction and unknown fields; ordinary file and Sample Stream remain available |
| Linux | Discovered explicit `journalctl --follow` helper with bounded initial history and structured output | Map unit, process, priority and cursor where available; hosts without systemd/journalctl use user-selected syslog/application files or Sample Stream |
| Windows | Native Windows Event Log subscription for selected Application/System or available operational channels | Map channel, provider, event ID, level, timestamp and bookmark; missing rendered message retains raw fields and a diagnostic; access-denied channels leave file/sample monitoring usable |

Discover capabilities in the application composition layer; do not assume a
universal `/var/log` pathname, journal service, helper version, accessible
channel or timestamp format. Show available sources and concrete permission/tool
errors. No automatic elevation or machine logging-configuration changes. Treat
source-side narrowing as an explicit subscription limit, separate from the
local view filter; reopening a local filter cannot recover excluded input.
Bound startup history and specify handoff to live events, bookmark/cursor
recovery, duplicates and unverifiable gaps per adapter. Privacy-redacted content
stays redacted. Source timestamps enter as data; no widget reads wall time.

Acceptance: a uniquely tagged controlled event appears through each native
adapter on its real OS, can be filtered/inspected, and continues after pause/
resume. Denial/unavailability, malformed records, helper exit or stale bookmarks,
rapid source replacement and close are tested. Replay fixtures provide identical
portable UI goldens; replay alone does not prove native subscription correctness.

Authorities: installed Apple `log(1)` documentation (structured modes are
version-checked), systemd's [journalctl manual](https://www.freedesktop.org/software/systemd/man/255/journalctl.html),
and Microsoft [event subscriptions](https://learn.microsoft.com/en-us/windows/desktop/WES/subscribing-to-events).
Use published documentation only; OS-specific schema mapping belongs to the demo.

## WP-83 Documentation and client integration

Publish `docs/live-logs.md` and `docs/logmonitor-example.md`, API reference,
widget catalog entries and source-backed screenshots. Cover the smallest
working file monitor, in-memory append, structured records, async publication,
polling fallback, rotation limits, follow/inspect behavior, query revisions,
overload/retention policy, performance tuning and extension with a custom source.
Explain platform source choices, permissions and metadata differences in the
demo guide. Supply public-header-only installed-client examples; convenience
construction must retain explicit lifetime/resource ownership.

Acceptance: documentation examples build against the installed library on all
three systems, and the minimal monitor composes shipped pieces without a custom
tailer, queue pump or filter implementation. Generated screenshots correspond
to tested goldens, including overload and unavailable-source states.

## WP-84 Final acceptance and evidence

Apply the engineering standard in order: warnings-clean clean builds on macOS,
Linux and Windows; unit/golden/script tests; built fuzz targets and CI corpus;
ASan/UBSan and TSan where threads exist; budget benchmarks; public documentation.
Include `provenance_check` and `provenance_checker_self_test`. Fuzz decoding,
optional style parsing, filters, source records and identity/rotation transitions.
Test slow terminals, notification floods, missed hints, producer outrunning
consumer, repeated query cancellation and long-lived resource stability.

For every native live check, record exact source/build/asset fingerprints,
start/end/elapsed/configuration and output path. Establish comparable build
duration from existing evidence; investigate anomalies before launch. Close
prior project instances, launch the verified absolute executable, resolve its
PID, and inspect its actual user-facing window. Capture each real-host demo's
native source, filtering, inspect/resume and overload status. Retain concise
durable evidence and remove superseded build/scratch data under the mandatory
temporary-workspace policy. Never delete another task's active workspace.

Completion requires all packages' literal criteria and current three-platform
evidence. A file-only demo is an intermediate delivery; the requested system-log
demo completes with all three native-source adapters and their failure paths.

## Planning validation

2026-10-12: the twelve package headings are unique and sequential; all local
links resolve; the new document has the required copyright/SPDX header;
`git diff --check` passes. `provenance_check` and
`provenance_checker_self_test` pass, 2/2, through a temporary script-only CTest
runner using the repository's current checker and test arguments. No library
build, native adapter or runtime performance acceptance is claimed by these
document checks. The temporary runner was removed after verification.

## Implementation checkpoints

- WP-73: D-140, architecture/queue and bounded core source contracts landed in
  `151674fd`. Three unit cases and seven lightweight CTest gates pass. Full
  builds, client compositions and the platform matrix remain pending.
- WP-74: bounded model/decoder and client guide implemented. Seventeen
  source/model cases pass under strict-warning ASan/UBSan, and a separate
  allocation case proves zero append/eviction/lookup allocations; final
  view goldens and cross-platform acceptance remain WP-77/WP-84 obligations.

- WP-75: native macOS/Linux/Windows file adapter code, fixed publication,
  explicit wake bridge, scripted source and client reference implemented.
  Current macOS and Debian ARM64 VM source/model/file suites pass 30 cases
  under ASan/UBSan, plus two allocation cases; the Mac passes 10/10 focused
  CTest gates and the VM 4/4. Windows native execution, full builds and the
  global matrix remain pending. [Evidence](evidence/live-log-wp75-2026-10-13.md).

- WP-76: LogFollower, bounded tail selection, overlap verification, short-read
  probes, generation splitting, old-object drain quotas and supplied-time
  recovery implemented. Current Mac and Debian ARM64 VM pass 51 ASan/UBSan
  source/model/native/follower cases and three allocation cases. The 20 follower
  scenarios include real path/open-file rotation and startup append interleaving.
  [Evidence](evidence/live-log-wp76-2026-10-13.md). Windows and global gates remain
  pending; WP-77 owns the view and its user-facing frame acceptance.

- WP-77: provider-backed LogView/LogPanel, shared allocation-free wrap iteration,
  follow/inspect and lifetime-guarded commands implemented. Current Mac passes
  112 ASan/UBSan cases plus four allocation cases, 16 frame goldens and 14/14
  focused CTest gates. [Evidence](evidence/live-log-wp77-2026-10-13.md). The view
  matrix, installed clients, timing/fuzz/thread checks and real demo windows
  remain carried acceptance; WP-78 is ready for incremental query delivery.

- WP-78: bounded literal/field filtering, dual-ring revision publication, partial
  snapshots, text revisions, independent search/navigation/highlighting, raw/
  context lookup and Filter/Find panel integration implemented. Current Mac:
  139 ASan/UBSan cases, five allocation cases, 23 frames and 16/16 CTest gates;
  Linux VM: 40 query/source/model ASan/UBSan cases.
  [Evidence](evidence/live-log-wp78-2026-10-13.md). Full host/fuzz/thread/installed
  and timing gates remain pending; WP-79 is ready for overload scheduling.

- WP-79: fixed publication mailboxes, explicit loss/epoch ordering, bounded
  receiver/model/follower admission, fair yielded lanes and coalesced refresh,
  plus D-141 all-frame delivery implemented. Current Mac: **286 ASan/UBSan
  cases**, six allocation cases, 26 pinned frames and **20/20 CTest gates**,
  including deterministic log work/memory/burst checks.
  [Evidence](evidence/live-log-wp79-2026-10-13.md). Linux/Windows, TSan/fuzz,
  installed clients and idle-host real-rate/performance acceptance remain pending.
  WP-80/81 are ready; no native demo-window acceptance is claimed.

- WP-80: injected continuous private process sessions, prestarted workers,
  independent stdout/stderr mailboxes, per-pipe EOF, native root readiness,
  cancellation and owned helper cleanup implemented over the shared finite/
  continuous pipe pump. Mac and Linux pass **32 ASan/UBSan cases** each,
  including 11 stream cases and 12 finite-runner regressions; Linux also passes
  all 32 under **TSan**. Mac **9/9 focused CTest gates** pass.
  [Evidence](evidence/live-log-wp80-2026-10-13.md). Windows and the global
  release/performance/client/demo gates remain pending. WP-81 is ready.
