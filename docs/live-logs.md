<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Live log models and sources

`widgets::LogModel` stores a bounded read-only log and incrementally decodes a
byte stream. It is UI-thread-owned, with no filesystem, environment or clock
access. Native file services implement the dependency-neutral contracts in
`<cvision/core/log_source.hpp>`. Native file observation and range reads are supplied by
`term::NativeLogFileService`; `widgets::LogFollower` composes recovery and
`widgets::LogView`/`LogPanel` supply follow/inspect presentation.


The complete [installed client recipes](../examples/log_clients/CMakeLists.txt)
build against `find_package(ckvision CONFIG REQUIRED)` and use public headers
only. [file_monitor.cpp](../examples/log_clients/file_monitor.cpp) is the smallest
interactive file composition; [recipes.cpp](../examples/log_clients/recipes.cpp)
executes append, metadata and bounded custom-source examples;
[file_contract.cpp](../examples/log_clients/file_contract.cpp) validates real
growing files and explicit native/polling modes without a custom tailer.

## Build the installed recipes

Select explicit disposable build, install and scratch paths. On macOS keep
project temporary data beneath the mounted S8T or PRO-BLADE temporary root.
Use the SDK build's compiler, configuration, generator/target architecture and
runtime-library selection. Sanitizer usage requirements travel with the exported
target. A Development installation supplies the library, headers, package files
and client sources without requiring the other demo executables.

```sh
cmake --install BUILD --config Release --component Development --prefix PREFIX
cmake -S PREFIX/share/ckvision/examples/log_clients -B CLIENT_BUILD \
  -DCMAKE_PREFIX_PATH=PREFIX -DLOG_CLIENT_SCRATCH=ABSOLUTE_SCRATCH
cmake --build CLIENT_BUILD --config Release
ctest --test-dir CLIENT_BUILD -C Release --output-on-failure
```

These uppercase paths are caller-selected values, not default temporary paths.
`share` is the default data directory; use the SDK's configured install data
directory when it differs. Set TMPDIR (and Windows TEMP/TMP) to the selected
scratch directory before compiler/test execution. The file contract creates
only its own unique create-only test file and removes it after worker retirement.

Run `log_file_monitor` with an absolute UTF-8/native log path. Ctrl+Q quits; the
panel supplies filter, Find and live inspection controls. The examples explicitly
intern standard roles and install a theme before attaching the panel; plain
Application construction does not create standard roles. Windows receives
Unicode arguments through wmain and converts them explicitly to UTF-8. The view
fills the application root on terminal resize. The controller drives the shipped
follower/filter/search/panel, while Application handles input and bounded frame
delivery. Services/wake targets enclose sessions, models enclose their borrowed
views, and lanes detach before destruction. No source launch or worker join
belongs in a UI handler.

## Compose and tune a source

For byte streams use append_bytes or a Bytes mailbox. For already grouped events
use append_record or Record messages with supplied metadata. A custom source
converts its own schema into bounded publications and handles Accepted, Busy,
WouldBlock, Dropped, Stale and Closed explicitly. The shipped receiver performs
model admission, loss resynchronization and end-of-stream decoding; the shipped
controller schedules model/filter/search/view work. A callback per record or a
replacement TextView buffer is unnecessary. Retain one declared staging packet
on Busy and retry at a yielding producer deadline or capacity wake.

Start with the product limits, then account for every capacity: retained raw and
safe text/metadata, slots, partial decoder, filter/search indexes, view caches,
mailbox slots, receiver staging, service workers/stacks and native buffers. Use
Backpressure for replayable producers, or an explicit drop policy for live sources
that cannot wait. Report queue loss, record truncation, retention eviction and
source continuity separately. Reducing retention does not by itself bound ingress
or terminal output. Bounded frame delivery coalesces output while preserving
input scheduling; a controller refresh is not physical visibility evidence.

Tune read/publication size and record quanta before increasing redraw rate.
Reconciliation remains necessary after missed file hints or polling fallback.
FollowPath rotation cannot prove lossless old-writer history; repeated query
changes publish coherent revisions and never restore records already evicted or
excluded at source subscription. The final live-log matrix measures throughput,
peak memory, idle wakeups and input/source-to-visible latency on pinned hosts;
loaded-host test duration is not an optimal-performance claim.

## In-memory streaming

```cpp
#include <cvision/widgets/log_model.hpp>

ckv::widgets::LogModel log;
log.begin_source(1, 1); // nonzero source and generation
log.append_bytes("first line\nsecond ");
log.append_bytes("line\n");
auto first = log.at(0); // optional, strings borrow until the next mutation
```

Each append accepts at most `limits().batch_bytes`, 64 KiB by default. Check
its boolean result: invalid/oversized requests change nothing. A partial line
is visible with a stable `LogRecordId` even while its final UTF-8 scalar is
incomplete. Appending its LF commits that identity. LF/CRLF and lone CR are
line boundaries. Construct with `LogCarriageReturn::Progress` to make lone CR
replace the current partial line instead; CRLF still commits one line.

Call `finish()` only when a stream actually ends, not whenever a followed file
temporarily reaches EOF. It resolves unfinished UTF-8 and commits nonempty
partial content with `incomplete=true`. A generation change also finishes that
partial entry. `clear()` discards retained entries without reusing identities,
stopping the source or resetting cumulative counters.

## Retention and overload

`LogLimits` defaults to 16384 records, a 16 MiB payload arena and a 64 KiB raw
record limit. The arena includes raw text, safe text and metadata; fixed record
slots and bounded partial staging are counted separately by `allocated_bytes()`,
a conservative storage bound including object and backing capacities.
Construction allocates those capacities once. Append, eviction and record
lookup allocate nothing. `size()` includes the current partial record and
never exceeds the record limit. Oldest records are evicted when slots or
payload space run out; `stats().evicted` reports how many were removed.

An oversized line keeps its bounded prefix and drains to its terminator.
`discarded_bytes` records that loss per entry and cumulatively. This is distinct
from retention eviction and future source/mailbox drop counters. `raw` retains
original bytes; `text` is well-formed UTF-8 with malformed input and terminal
controls replaced by U+FFFD. A generation-start UTF-8 BOM is consumed. All
parsing is independent of how the source chunks its bytes.

`LogLimits::valid()` checks capacity/arithmetic constraints. Invalid construction
throws `std::invalid_argument`; allocation failure follows ordinary C++ behavior.
No widget should silently substitute smaller limits.

## Structured records

```cpp
const std::array fields{ckv::LogField{"unit", "demo"}};
const ckv::LogMetadata metadata{ckv::LogSeverity::Warning, 1234, fields};
log.append_record("warning\n  detail", metadata);
```

Explicit records do not guess a source format or split at LF. LF separators
are preserved in their safe text; other controls are neutralized. Metadata
contains at most eight uniquely named fields with 4 KiB total name/value bytes.
Source timestamps are supplied data, in nanoseconds under the adapter's
documented time convention; absent timestamp/severity remain unknown. The
model copies field/content bytes during the call, so producer storage need not
outlive it. Returned field strings borrow model storage, and `metadata()` also
borrows the returned `LogRecord` value's field array.

## File service ownership

`LogFileService` owns execution resources and must outlive its sessions.
`open_session()` may refuse when capacity is exhausted. A session accepts
bounded `LogFileRequest` values with nonzero revisions, absolute Inspect paths
or bounded Read offset/length ranges. `take(result)` transfers one publication
into a caller-owned fixed-capacity `LogFileResult`;
read results are consumed once while notification hints may coalesce.
Inspect selects the pathname and Read addresses the session's opened file.
The result reports native identity, size, opaque version, source/watch status,
offset, native error and whether reconciliation is necessary.

Request/take/cancel/session retirement must not perform native I/O or join a
worker in UI dispatch. Destroy the application-owned service outside dispatch.
Reject stale revisions before applying a result. Pure controllers use an
injected clock or caller-provided time for retries and reconciliation.

## Native file construction

```cpp
#include <cvision/term/log_file_service.hpp>

ckv::term::NativeLogFileService files(2); // construct outside dispatch
{
    auto session = files.open_session();
    ckv::LogFileResult result; // preallocate outside dispatch
    ckv::LogFileRequest request;
    request.revision = 1;
    request.path = "/absolute/path/to/application.log";
    session->request(std::move(request));
    // In a caller-scheduled step, take(result) consumes an available result.
    // A false return leaves result unchanged. Never spin in UI dispatch.
    // Use changes_pending() and injected-time periodic reconciliation.
} // retirement never waits for the worker
// Service destruction joins workers here, outside dispatch.
```

Capacity is 1–8 sessions with a chosen read limit up to 64 KiB. A retired slot
becomes available after its worker closes native resources. Opening may return
no session while slots are occupied or retiring. Requests have strictly
increasing revisions; later revisions supersede earlier work. Read results
remain available until consumed or explicitly superseded, and transfer copies
only used payload. Inspect follows replacement by pathname by default;
`OpenFile` retains the opened object when the same pathname is inspected again.
A changed pathname always selects a new source object.

Native mode uses macOS vnode/kqueue, Linux inotify, or Windows overlapped
parent-directory notifications. Parent observation starts before file opening,
so a missing file can still produce a recreation hint. Hints coalesce and may
be incomplete; periodic reconciliation remains necessary, especially for
network mounts, writes through other links, and unobserved same-size rewrites.
`LogWatchPreference::Polling` disables native hints; inspections are scheduled
by the caller's injected clock, without a hidden worker polling timer. Native
watch registration failure likewise reports polling fallback.

Read observations include native identity/size/version before and after I/O.
Identity retains 128-bit Windows file IDs where supported, with the documented
native 64-bit fallback. Version is an opaque observed metadata token, not a
content hash or an integrity guarantee. `rescan_required` reports lost/invalidated
notification evidence. Missing and denied sources carry explicit state and
native error (`errno` on POSIX, Win32 code on Windows). Only regular disk files
are accepted. Reads/stat/open and source-resource cleanup occur on owned workers;
request, take and cancel do not allocate or join them. Service shutdown may wait
on a stalled remote filesystem, so its lifetime belongs at the host boundary.

An optional `LogWake` target announces readiness, first pending change, or
failure from the worker. `TerminalLogWake` wakes an explicitly supplied terminal;
that terminal must outlive the service. It retains no Application/view callback.
Repeated native hints coalesce while `changes_pending()` is true. Use
`session.has_wake()` to select notification-driven result waiting; without a
wake target, schedule bounded checks with the injected clock. Do not post work
per line or invent another terminal-output path.

## Following a file

```cpp
#include <cvision/widgets/log_follower.hpp>
#include <cvision/term/log_file_service.hpp>

ckv::term::NativeLogFileService files;
ckv::widgets::LogModel model;
ckv::widgets::LogFollower follower(model, files.open_session());
follower.start("/absolute/path/to/application.log", 1, 0);
// In the host's loop, pass its injected monotonic clock value to step().
follower.step(0);
// Combine next_deadline() with the host's other event-loop deadlines.
```

The model is the follower's exclusive stream destination. The follower owns one
session and retires it without joining a worker. The service outlives both.
`start()` can switch sources while keeping retained history; stale request
revisions never enter that history. `stop()` is final for the session. Create
another follower/session to restart after stop. Invalid starts and backwards
time do not change source/model state.

Defaults select up to 500 newest lines from a 64 KiB startup window, with a
16 KiB step quantum, a 256 KiB old-object drain quota and one-second periodic
reconciliation/retry. Constructor options choose beginning, explicit offset,
recent-tail bytes/lines, pathname versus open-file following, and time intervals.
The actual read quantum also respects model and adapter admission bounds. Tail
storage never scales with file size. An unverifiable leading partial line is
discarded, including its continuation if no separator appears in the initial
window. `startup_skipped_bytes` reports bytes deliberately skipped within that
window/continuation; it does not claim the size of all older unexamined history.
An explicit offset starts at exactly that byte. BOM admission is enabled only
when delivered content starts at source offset zero.

The follower rechecks a small raw-byte overlap before its cursor and never
appends that overlap again. Short reads can split a probe. A changed boundary
or observed truncation separates generations; metadata-only changes preserve
surviving record identities. Before pathname inspection, a bounded drain retains
readable old-object output. Reaching the quota yields to inspection; remaining
known old bytes are counted only if that object was actually replaced. Initial
reads are followed by reconciliation, including concurrent append/rotation.

File EOF keeps an unfinished line open for further append. A real stop, source
switch or generation change finishes it as incomplete. Missing/denied/failed
sources retain history and retry through explicitly supplied monotonic time;
`retry(now)` advances recovery manually. No UI pause state stops ingestion.

`status()` reports state/watch mode, source/generation, cursor, observed size,
backlog, bytes read/delivered, intentional startup skips, known unread skips,
continuity warnings, pending work and native errors. Uncertainty is not an exact
lost-record count: an unobserved earlier rewrite, identical truncate/regrow or
old writer continuing after rotation cannot be proved lossless with a bounded
probe. A changed same-size token with a matching boundary is reported as an
unverified change without replaying retained history. Lost notifications likewise
mark uncertainty. These warnings are distinct from model retention eviction.

Each `step(now)` consumes at most one publication and delivers one byte quantum,
then yields. `next_deadline()` is immediate while buffered work remains or the
session reports `changes_pending()`, a reconcile/retry deadline when idle, or
absent while a supplied wake target owns
result delivery. Without a wake target, a configurable result-poll deadline is
used. Deadline arithmetic saturates and does not wrap into a busy retry loop.
Construction reserves payload, tail-selection and path storage; start, append,
reconciliation and lookup do not allocate. Requests borrow their values only
for immediate adapter copying and must never be retained by a custom adapter.

## Follow and inspect

```cpp
#include <cvision/widgets/log_panel.hpp>

// model outlives panel; the application owns both and the native service.
ckv::widgets::LogModel model;
ckv::widgets::LogPanel panel(model);
panel.set_bounds({0, 0, 80, 24});
model.begin_source(1, 1);
model.append_record("disk warning", {ckv::LogSeverity::Warning}, ckv::Attr::Bold);
panel.refresh(); // after a bounded follower/filter publication
```

The view follows the tail by default. Up/PageUp/wheel or selecting text pauses
automatic scrolling while the model keeps accepting records. End or Resume
returns to the tail and resets the unread counter. Partial revisions do not
count as new admissions. Left/Right and the horizontal track inspect wide
lines. `set_wrap_mode()` chooses the central Word/Character/None rules. A
vertical track jumps between records; keys and wheel step display rows. Its
thumb deliberately represents records rather than a global wrapped-row count.
The horizontal range grows from encountered records, preserving the inspected
column without measuring the full retained history.

Shift+arrows/Home/End extends a grapheme-safe selection; dragging selects text.
Ctrl+A selects eligible retained records and Ctrl+C copies safe display text
through Application's clipboard. `selected_text(max_bytes)` supports a client
without Application; it caps output at 1 MiB, reports truncation, and never cuts
a grapheme. Copying a filtered provider includes only eligible retained records
between the endpoints. Raw bytes remain available from the model for details.

`LogRecordProvider` is a borrowed coherent UI-thread snapshot, ordered by
admission sequence with logarithmic-or-better ID lookup. Strings expire on its
next mutation. The provider reports a content revision for each record, a
publication revision, monotonic pre-filter admissions and eviction counters.
`LogFilter` supplies incremental query indexes. Construct `LogPanel` with the
filter to bind its `filter_box()` automatically. For a custom provider, wire
`on_query_changed`, publish its coherent revision and call `set_query_pending()`
as appropriate. Query-pending status marks incomplete rebuilds.

Resize, append and query changes preserve surviving IDs and selections. A
removed anchor clamps to the first eligible record with an unavailable notice;
removed selection endpoints clear the selection visibly. Rewrapping starts at
the row containing the previous anchor byte. Notices persist until acknowledged.
The view allocates its viewport rows and one bounded record's wrap index at
construction; refresh/layout do not allocate. `allocated_bytes()` reports that
storage, excluding the two child controls and allocator overhead. Defaults cover
4096 viewport rows and 64 KiB raw records (up to 192 KiB sanitized text). Providers
with larger record limits must use a separately designed capacity contract;
construction rejects them. A viewport beyond its chosen row capacity or 65535
columns displays a capacity error instead of truncating silently.

`LogPanel` displays follow/unread, source state, watch mode, eligible count,
backlog, evictions, drops, native errors and continuity/query notices. It composes
standard SearchBox/Button/Scrollbar controls and existing theme roles. Narrow
layouts hide the Resume button while End remains available. Source status is
injected with `set_source_status()`; no widget reads a native file or clock.

`commands(prefix, context)` returns ordinary registry descriptors for menus and
the command palette. Register them under application-owned keys; their callbacks
become inert after the panel is destroyed. `ClearView` hides current admissions
only in this view, `reset_view()` shows retained entries again; `ClearRetention`,
`Stop`, and `Retry` call explicit owner callbacks. Pause never invokes Stop.
Retired commands should be withdrawn by the application when its window closes.

## Incremental filtering and independent search

```cpp
#include <cvision/widgets/log_search.hpp>
#include <cvision/widgets/log_panel.hpp>

ckv::widgets::LogModel model;
model.begin_source(1, 1);
ckv::widgets::LogFilter filter(model);
ckv::widgets::LogSearch find(filter);
ckv::widgets::LogPanel panel(filter);
panel.set_search(&find);
panel.filter_box().set_query("error");

// Once per scheduled bounded drain, after any follower/model publication:
filter.step();
find.step();
panel.refresh();
```

The app schedules another step while either engine returns pending. Do not drain
an entire query synchronously in an input handler or paint. Default budgets are
256 record visits and 65536 byte comparisons; filter record visits are clamped
to the model's `filter_records_per_step`. Explicit budgets require nonzero values,
at most 256 visits and 65536 comparisons. Comparisons include prefix fallback
and field-name matching; a partial record can span many steps. Getters, query
replacement, ordinary append matching and publication do not allocate after
construction. `allocated_bytes()` reports indexes, compiled literals and the
bounded partial snapshots. An unrestricted empty query projects the model
immediately; it does not build or copy an all-record index.

`LogFilterQuery` supplies include and exclude literals, a severity mask (one bit
per `LogSeverity`, Unknown included), up to eight exact source IDs, up to eight
named-field predicates, and an optional borrowed field matcher. All gates are
ANDed. Field modes are Present, Equals and Contains; names are exact bytes,
values follow the query's case policy. Missing metadata stays unknown; text
spelled ERROR does not become Error severity. `case_sensitive=false` folds ASCII
A-Z only, while every non-ASCII UTF-8 byte must match exactly. LF can match inside
an explicit multiline record. No regex, locale inference or implicit format
parser is involved.

```cpp
ckv::widgets::LogFilterQuery query;
query.include = "timeout";
query.exclude = "expected";
query.case_sensitive = false;
query.severity_mask = static_cast<std::uint16_t>(
    1U << static_cast<unsigned>(ckv::LogSeverity::Error));
query.field_count = 1;
query.fields[0] = {"unit", "backend", ckv::widgets::LogFieldMatch::Equals};
filter.set_query(query); // check false and status().error for rejected input
```

Literals/values are at most 1024 UTF-8 bytes, field names 128, aggregate query
bytes 8192. Invalid text/predicates or oversized input leave the last accepted
query intact and expose an error. A custom field matcher receives at most eight
fields/4096 metadata bytes and borrowed context. It must be bounded, nonblocking,
allocation-free and deterministic, and must not mutate/reenter the model or
query engine. Its context outlives both published and rebuilding queries.

Changing a query increments `requested_revision`; old IDs remain under their
`published_revision` while the replacement ring builds. The panel labels which
query is rebuilding. A newer request cancels the earlier builder. Arrivals join
the pending query; eviction prunes ID prefixes by binary search. Completion swaps
the two rings atomically. With an unchanged query, only new admissions and the
previous streamed partial are evaluated. A fixed snapshot of the last processed
partial keeps old displayed text coherent until its new predicate finishes.
`live_pending` distinguishes catching up on source changes from changing the
query. Stable text revisions prevent discard counters/unfinished decoder bytes
from restarting matching or wrapping of an unchanged retained prefix.

Find is independent of the visibility filter. Ctrl+F opens its own field; F3 and
Shift+F3 request next/previous. It searches the published eligible prefix under
a captured upper admission bound, so later arrivals cannot extend a request
forever. Filter publication restarts navigation under the new visibility revision.
Pending/no-match/stale/error states are explicit; eviction during navigation
marks the request incomplete instead of implying an exhaustive retained-history
search. Before delivery, `take_result()` checks the ID and text/query revisions.
`LogView::reveal()` expands the literal byte range to whole touched graphemes and
selects/scrolls that bounded record. Alt+F3 remains the standard close command.

The view borrows `LogHighlighter` from `set_highlighter()` (the panel wires it
when a search is bound). It marks only visible byte windows plus bounded literal
overlap, merges overlapping occurrences in linear work, and emphasizes touched
graphemes with bold/underline. Selection keeps its own theme rendition. The
private byte-mark buffer is allocated with the view; paint never scans history.

`filter.detail(id)` returns current retained raw/safe bytes and metadata, even
for excluded records. `LogContextProvider` supplies a separate ID-anchored
original-model window of up to 128 neighbors on each side. Its rows never enter
the primary filter's index/count merely because they are displayed. If the
anchor is evicted, context becomes empty explicitly. Model outlives filter/context;
filter outlives search and panel; search outlives its borrowed highlighter.

## Bounded worker delivery and overload

`LogMailbox` owns a fixed publication ring. The default is four 64 KiB slots
and DropOldest. A `LogMessage` owns its byte/metadata buffers, so a producer
can reuse its staging object after an accepted offer. Bytes messages preserve
one stream's ordering; Record messages preserve explicit grouping and metadata.
Supply nonzero source/epoch and a monotonic arrival time. Copy/move rebases the
message's metadata views into its own storage. `set_record()` inputs must not
alias that same message's metadata storage. Empty byte messages are rejected;
empty explicit records are valid. Limits are 1–64 slots, 1–64 KiB per message,
eight fields and 4 KiB metadata. `allocated_bytes()` includes fixed slot storage.

```cpp
#include <cvision/widgets/log_controller.hpp>

ckv::LogMailbox queue(1, 1, {4, 64 * 1024,
    ckv::LogOverflowPolicy::DropOldest});
ckv::widgets::LogModel model;
ckv::widgets::LogFilter filter(model);
ckv::widgets::LogPanel panel(filter);
ckv::widgets::LogStreamReceiver receiver(model, queue);
ckv::widgets::LogController controller;
const auto lane = controller.attach(
    {&model, &filter, nullptr, &panel, nullptr, &receiver});

// Worker: explicit source/epoch/time, one caller-owned staging packet.
ckv::LogMessage message;
message.source = 1; message.epoch = 1; message.arrival_nanos = 0;
message.set_bytes("new entry\n");
const auto offered = queue.offer(message); // handle the result below

// UI: schedule one bounded turn at next_deadline(), using the app's clock.
controller.step(0);
const auto deadline = controller.next_deadline();
```

Every try-lock operation returns promptly. Busy means retry the caller-owned
packet at a yielding producer deadline; it never implies acceptance or loss.
Backpressure returns WouldBlock when full. Use that policy for a producer which
can safely pause or replay. DropNewest returns Dropped and records loss at its
stream position; DropOldest accepts the new packet and attaches evicted-prefix
loss to the next surviving packet. Do not block an arbitrary producer implicitly.
For bytes, dropped record count is unknown; structured-record loss is exact.
Queue counters report bytes/batches and that distinction. EOF/failure is stored
out of band by `close()` and arrives after retained queue data and trailing loss.
Cancellation retires queued data explicitly; `reset(source, epoch)` rejects
stale epochs and old producers must handle Stale. close/reset/cancel return false
on contention and require a bounded retry by the owner.

One work notification stays armed until `acknowledge()` after a bounded drain.
Its true result means schedule another quantum. Capacity notifications are
separate. For native sources, pass a shared `LogActivity` with a downstream
`TerminalLogWake` to the service/mailbox, and borrow that activity in the
controller. The activity coalesces wakes across lanes and never retains a view.
All shared wake targets outlive producers; attached model/filter/search/panel/
follower/receiver objects outlive attachment. Detach before destroying a lane.
Do not post an Application callback for every log line.

`LogStreamReceiver` holds at most one packet in addition to the mailbox. Its
default quantum is 16 KiB and 256 new records; a single explicit multiline
record can span turns without changing identity or metadata. Byte loss finalizes
the old partial as incomplete, advances generation, emits a warning gap marker
and resynchronizes through the next CR/LF delimiter. It cannot splice pre-loss
and post-loss fragments into an apparently complete record. Structured records
need no delimiter resynchronization. End flushes pending decoding. `stop(now)`
cancels and retires the source; retry false on contention. Receiver diagnostics
separate delivered bytes, resynchronization bytes, gaps and intentional retirement.

`append_bytes_bounded(input, new_records)` returns the actual consumed suffix
boundary, including when a dense batch reaches its record quota. A zero record
quota can continue the existing partial but cannot admit a new identity.
`begin_record(metadata, attrs)`, `append_record_bytes()` and `end_record()`
provide the corresponding incremental structured-record path. The convenience
`append_record()` still performs one complete bounded record append.

## Fair scheduling and terminal pacing

`LogController` owns 1–64 fixed borrowed lanes; default capacity eight. A turn
visits at most `lanes_per_turn` ready lanes in round-robin order, advancing one
ingress, filter and search quantum each. The default yield is 1 ms and refresh
interval 33.333334 ms. Pending matching defers file reads without consuming their
offsets; stream queues use their chosen overflow policy. Idle lanes require no
controller poll timer. File reconciliation deadlines remain explicit.
`kick(lane)` or `kick_all()` requests a probe after owner-driven changes.
Time is supplied to `step(now)`; backward/negative times are rejected.

Refreshes coalesce to the latest coherent state. The controller exposes turns,
lane quanta, deferrals, coalesced updates and observed-to-refresh lag. The panel
shows queue/backlog bytes, known/unknown record loss and oldest queued lag. These
times end at a panel refresh, not physical host visibility. Retention eviction,
queue loss and replayable source backlog remain distinct. Paint and append do
not rewrap, copy or refilter the entire retained history.

For a live monitor, select
`app.set_frame_delivery_policy(ckv::ui::FrameDeliveryPolicy::Bounded)`.
The existing Presenter adds a completion question and gates all subsequent
frames, including small ones, while input and timers continue and dirty state
coalesces. Once the host has acknowledged, slowness never writes off its frame
debt. A host which never responds uses the existing three-probe timeout fallback;
controller refresh remains paced. Adaptive is the Application default.
The native synchronous Terminal::write transport contract still applies; this
does not promise input progress while a native write itself physically stalls.

Memory is the sum of model arenas/slots/staging, filter/search indexes/snapshots,
view caches/marks, mailbox slots, receiver staging and controller lanes. Include
each producer's declared staging packet and backend buffers in an application
budget. Bounded non-replayable monitoring cannot guarantee lossless indefinite
operation above its consumption rate. Tune explicit quotas and retention from
measured workloads, rather than increasing queues to conceal sustained lag.
The log benchmark gates cover history size, Unicode/long lines, query churn,
fixed memory and multi-source bursts. Real terminal-rate 1×/10×/100× trials,
source-to-visible percentiles, idle/CPU and pinned-host input p99 < 2 ms are
WP-84 acceptance; loaded-host CPU timings are not release performance evidence.
