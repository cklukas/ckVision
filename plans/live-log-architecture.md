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
