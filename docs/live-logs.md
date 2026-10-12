<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Live log models and sources

`widgets::LogModel` stores a bounded read-only log and incrementally decodes a
byte stream. It is UI-thread-owned, with no filesystem, environment or clock
access. Native file services implement the dependency-neutral contracts in
`<cvision/core/log_source.hpp>`. Native file observation and range reads are supplied by
`term::NativeLogFileService`; the view and follower are subsequent packages.

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
