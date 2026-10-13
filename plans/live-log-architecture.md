<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Live log architecture and delivery contracts

D-140 adopts these contracts for WP-73…WP-84. Generic log monitoring is a
read-only UI capability in VISION's scope. File and pipe services belong to
term; their dependency-neutral contracts belong to core. Pure widgets own
retention, decoding, filtering and presentation, receiving explicit service
instances and caller-supplied monotonic times. OS log schemas and discovery
belong to the demo application's adapters. No new dependency is introduced.

## Ownership and identities

One session owns one byte stream and decoder. One model represents that stream;
the demo composes multiple source models in tabs. Every record has a nonzero
source, generation and monotonic sequence. Filter/display position and source
timestamps are never identity. Partial-line revisions keep their ID; generations
change on source replacement or observable truncation. Sequences never wrap or
restart after clear. Exhaustion is a reported admission failure.

Models and views are UI-thread-owned. Native services are explicitly
application-owned and outlive borrowed sessions. Request/take/cancel and session
retirement perform bounded work without native I/O or worker joins. Service
shutdown joins outside dispatch. Native publications carry a request revision;
retired revisions never mutate the active model. There is at most one undrained
read result per file session. Change notifications are hints and may coalesce;
completed reads never coalesce away. The controller issues its next bounded
read only after consuming the previous result.

## Storage and parsing

LogLimits specifies product defaults: 16384 retained records, a 16 MiB payload
arena, a 64 KiB raw record limit, a 64 KiB ingest quantum, four pending stream
batches, 256 records per filter step, a 33.333334 ms frame interval, and a
1 s reconciliation interval. These are not measured throughput baselines.
User-selected limits must fit one worst-case record plus metadata. Total
allocated memory includes fixed record slots, raw/display staging, pending
publications, indexes, and layout caches; report each component independently.

The model allocates a byte arena and record-slot ring at construction. Entries
borrow arena slices until the next model mutation. Finalized records pack raw
bytes, safe display text and at most eight unique metadata fields (4 KiB total)
into the arena. Partial raw/display records have separately bounded reusable
staging buffers. FIFO eviction frees oldest records/arena allocations together;
IDs survive ring relocation and never identify a different record.

Incremental decoding uses ckVision's UTF-8 and display-safety policies. Hold at
most four bytes of a candidate scalar until it can be decoded or EOF resolves
it. LF and CRLF commit lines; a lone CR is a line terminator by default. An
explicit progress mode replaces the current partial line after lone CR without
reusing a different record's identity. A UTF-8 BOM is consumed only at generation
start. Malformed UTF-8/C0/C1 become U+FFFD in display text; raw bytes retain their
original spelling. Oversized lines retain a bounded prefix, count discarded
bytes, and drain to the next terminator. EOF commits a nonempty partial record
with an incomplete flag. Plain append never guesses multiline semantics;
explicit structured-record append accepts multiline raw text and bounded fields.
No ANSI parser or regular-expression engine is added in this delivery. Literal
filtering supports exact UTF-8 or explicit ASCII-insensitive matching; non-ASCII
case equivalence is not silently guessed. A later Unicode-folding/regex feature
requires its own specified bounded engine and conformance evidence.

## File following and recovery

Core LogFileService/Session exposes async Inspect and Read requests. term owns
opening/stat/range I/O, native vnode/inotify/directory notifications and fallback
polling. Inspect observes pathname identity; Read addresses the opened object.
Native handles never escape core contracts. Path following reconciles containing
directory changes; open-file following deliberately keeps the old object.
Read results identify the opened object and source offset. File services are
constructed outside dispatch with explicit capacity and worker ownership.

The follower uses bounded recent-tail reads by default, never a whole-file read.
Register observations before initial reads and reconcile after them. Rotation
has a bounded old-generation drain, followed by an explicit generation marker.
Missing/denied/failed sources retain the view and present recovery state.
Continuity cannot be guaranteed after unobserved truncate/regrow or unreplayable
rotation; report a gap/uncertain state rather than inventing records.

## Filtering and presentation

Queries have monotonic revisions. Incremental matching indexes reference record
IDs. Query rebuilds yield every configured record quantum; new arrivals and
evictions are incorporated before publication. Keep old coherent results marked
pending until the new revision is coherent. Search and filter are independent;
context rows never become matches just because they are displayed.

LogView paints only visible retained records and uses central grapheme/width
logic. Follow is the default; user scroll pauses presentation, not ingestion.
Resume/End returns to the tail. Use IDs and intra-record offsets for anchors and
selection. Show unread entries, evicted selection, source state, backlog and
loss. LogPanel composes normal inputs/status/commands around that view.

## Overload and performance acceptance

Separate source read/decode work, filter work and frame delivery. The frame
deadline coalesces presents to the latest coherent state, without discarding
retained entries. Do not post one Application callback per line; at most one
pending drain notification per session, with bounded drain/reschedule work.
Use the existing presenter pacing/damage pipeline rather than a parallel writer.

For file backlog, retain offsets and defer reads while bytes remain available.
For non-replayable streams, select Backpressure, DropOldest or DropNewest
explicitly. Default the monitor's live pipe to DropOldest, with exact drop
counters and a visible gap. No promise of indefinite lossless consumption when
the producer outruns every bounded consumer. Retention eviction, source loss,
pending backlog and frame skipping are distinct counters.

Hard operation budgets: no native I/O in pure layers/dispatch; no mutation-time
allocation in model append after construction; no whole-history walk in paint;
one filter work quantum per scheduled drain; bounded bytes/records/publications
and output frames at every handoff. Preserve ARCHITECTURE §8's existing budgets.
The 2 ms pinned-host input p99 target remains; source-to-visible lag is measured
separately. Wall-clock throughput thresholds require a measured named-host
baseline before ratcheting. A loaded host is not used to invent that baseline.

## Public composition sketches

The source contract is available in `<cvision/core/log_source.hpp>`; model and
view implementation follow in WP-74/WP-77. The following API responsibilities
are binding; later packages add their headers and compile these recipes.

1. File: construct NativeLogFileService outside dispatch; borrow a session in
   LogFollower; attach LogModel to LogView; drive follower/filter/frame deadlines
   with the Application's injected Clock. Retire the controller before service
   shutdown. Never call FileSystem::read_file in the log paint/input path.
2. In memory: construct LogModel with LogLimits; begin a source generation;
   append a bounded byte batch or explicit record; notify the attached view of
   model changes. Scroll pause never inhibits this append path.
3. Worker: construct a bounded stream mailbox outside dispatch; producer offers
   batches under the declared overflow policy; an application-owned wake bridge
   schedules one drain; the owning thread drains one quantum into LogModel.
4. Filtering: set a new query revision; step its bounded rebuild; publish only
   current coherent IDs; preserve a surviving view anchor across publication.
5. Teardown: cancel source, retire drain/view handles, discard stale revisions,
   then destroy/join the application-owned service outside callback dispatch.

## Acceptance traceability

| Contract | Executable evidence required | Owner |
|---|---|---|
| Bounds, metadata, identity, request validation | test_log_source.cpp | WP-73 |
| Chunk invariance, raw/safe text, partial IDs, eviction, no append allocation | test_log_model.cpp and allocation gate | WP-74 |
| Async file reads, hints, overflow/cancel, native identity | native log file suites on three hosts | WP-75 |
| Startup/rotation/truncation interleavings and recent tail | test_log_follower.cpp and real files | WP-76 |
| Follow/inspect/select/copy/wrap/themes | test_log_view.cpp and frame goldens | WP-77 |
| Revision cancellation, incremental queries/context/search | test_log_filter.cpp | WP-78 |
| Bounded mailboxes, slow sinks, fair drain and latest-frame convergence | test_log_delivery.cpp and named-host benchmark | WP-79 |
| Private continuous pipes, stderr, cancel, owned helper cleanup | native streaming suites | WP-80 |
| Portable source picker/sample/overload/export | logmonitor app scripts and goldens | WP-81 |
| Real tagged OS events and denial/restart paths | macOS/Linux/Windows live evidence | WP-82 |
| Public installed-client composition, reference, screenshots | client compile/doc coverage gates | WP-83 |
| Full builds, fuzz, sanitizers, benchmarks and actual windows | WP-84 evidence matrix | WP-84 |

## Native file delivery refinement

WP-75 replaces the unreleased WP-73 heap-owned result prototype with a
caller-owned fixed-capacity LogFileResult and take(result) transfer. Request,
read publication and consumption use preallocated mailboxes; no allocation or
worker join occurs in steady UI dispatch or native bounded reads. The result
records operation and before-read observation for reconciliation. Native
identity preserves the high half of platforms with 128-bit file IDs. Session
change hints are independent of read results and clear only through inspection.
Native waiting blocks on the request wake and filesystem event handles together;
forced polling waits for caller-scheduled inspection, without a hidden poll loop.

NativeLogFileService optionally receives an explicit LogWake target. Publication,
first pending change and failure wake that target; repeated hints coalesce.
TerminalLogWake borrows a Terminal that outlives service shutdown, keeping
Application/view lifetimes out of worker callbacks. Sessions expose has_wake
so controllers can wait for notification rather than run result-poll timers.

## File follower execution contract

WP-76's LogFollower owns a borrowed-service session and is the model's exclusive
stream writer. Monotonic time enters step/start/retry explicitly. Each step
consumes at most one publication and delivers one configured byte quantum.
Startup recent-tail storage/scanning is capped at 64 KiB, with explicit newest-
line selection and incomplete leading-line discard. Ongoing reads verify a
bounded overlap before the current offset; those probe bytes are never appended
again. A changed boundary starts a new generation with a continuity warning.
A matching boundary preserves the cursor through metadata-only changes; it does
not establish that every earlier byte is unchanged. Unobservable rewrite limits
remain explicit. Before path reconciliation, drain the opened object only up to
its configured old-generation quota, then yield to inspection. New identity,
truncation and reconnect split generations; partial old text is committed as
incomplete. File EOF leaves partial text open. Source switching advances request
revisions and discards stale results. All storage is preallocated.

LogFileSession::request borrows its immutable request for the call; native
adapters immediately copy into fixed request storage. read_limit exposes the
adapter's admission bound. LogModel::begin_source explicitly declares whether
BOM consumption applies, so tailing from a nonzero offset cannot eat a literal
BOM in later log content. These settle unreleased prototypes before delivery.

## WP-77 view contracts

A borrowed LogRecordProvider publishes one coherent UI-thread snapshot. Lookup
by identity is logarithmic or better; records are bounded by max_text_bytes.
Its revision covers content/visibility changes, its monotonic latest_sequence
covers admission even when a filter excludes the new record, and eviction
counts distinguish lost retention from filtering. LogModel implements it;
WP-78 supplies a filtered provider. No borrowed record string survives mutation.

LogView allocates row capacity for an explicit maximum viewport height and a
single-record wrap index at construction. Resizing beyond that capacity is
rejected explicitly. Layout visits only viewport records and at most one whole
bounded record for intra-record navigation. Paint uses cached byte ranges; it
never walks retained history. A record content revision permits reuse across
unrelated appends. Central text layout supplies the allocation-free wrap step.
The vertical track navigates records, not an invented total wrapped-row count.
The horizontal track measures encountered content, preserving the inspected
column; both tracks reserve stable space when there is room.

Scrolling pauses presentation; End resumes it. Arrivals and partial revisions
keep paused ID/byte anchors. Removed anchors clamp to the first available record
and expose a notice. Invalidated selections clear explicitly; surviving
endpoints clamp to grapheme boundaries. Copy includes only eligible retained
records between its endpoints and is capped at an explicit byte limit; truncated
copies are reported and never split a grapheme. Record attributes augment theme
roles; severity remains metadata. Pause, source stop, hide-current-view and
clear-retention are distinct commands. The panel borrows a provider, composes
SearchBox/Resume controls and exposes query/source-command callbacks; it never
owns platform I/O.

Inactive selection uses the theme's selection role; when its foreground and
background equal ordinary text, reverse rendition keeps selected bytes visible
while query controls own focus. Unavailable-entry/selection and source failure
notices precede routine counts/watch labels in a narrow status line.

## WP-78 literal queries and coherent publication

Filter queries own bounded UTF-8 include/exclude literals, severity/source gates,
up to eight named-field predicates and an optional borrowed deterministic field
matcher. Literals and values are at most 1024 bytes each; aggregate query bytes
are at most 8192; field names are at most 128. Matching is exact UTF-8 or explicit
ASCII case folding, with no locale or regex. A shared compiled prefix matcher
bounds comparison work, yielding between comparisons without losing its cursor.
Custom predicates must be bounded, nonblocking, allocation-free and deterministic
over at most eight/4096-byte metadata fields; they never mutate the model.

Two preallocated admission-ordered ID rings serve the published and rebuilding
queries. Eviction prunes a ring prefix by binary search. New records and the one
mutable streamed record are evaluated incrementally; no full-history refilter
occurs after an ordinary append. A query replacement cancels the builder and
restarts under a new revision. The previous result remains labeled as the old
query while rebuilding. Each ring owns a bounded snapshot of its last processed
partial record, so a later partial update cannot change the displayed old result
before matching completes. Strings are borrowed until mutation; no model lookup,
filter step or publication allocates after construction. Call step after source
mutation and before refreshing the view. A step evaluates at most the configured
record quantum and text/name comparison budget; publication is an O(1) ring swap.

Search owns an independent literal revision and cancellable navigation cursor.
It searches only the published eligible prefix, snapshots its upper admission
bound, and yields by record/comparison budgets. A filter publication restarts
navigation under the new visibility revision; ordinary later arrivals do not
turn a bounded request into an endless scan. Results identify query/content
revisions and are checked against current retained eligible data before delivery.
Highlighting marks only a viewport byte window plus bounded pattern overlap;
visible graphemes are emphasized if any of their bytes match. Navigation reveals
a whole-grapheme selection. Search never changes filter visibility or retention.

Raw detail lookup and an ID-anchored bounded context provider use the original
model. Context rows do not enter the filter's match index or counts. The panel's
filter field and optional Find field remain separate, with F3/Shift+F3 commands
and explicit pending/error/no-match status. The app schedules filter/search steps;
paint and keyboard dispatch never perform a history scan.

The unrestricted empty filter is an O(1) model projection. Matchers/layout key
on a separate per-record text revision: oversized-line discard counters and
incomplete decoder bytes do not restart unchanged-text work. Finalization
refreshes snapshot flags without refiltering unchanged text. Search records
eviction during a request as incomplete and labels no-match accordingly.
SearchBox owns an injected display-safe prompt; LogPanel labels its separate
Filter/Find fields and uses filter_box() for visibility edits.
