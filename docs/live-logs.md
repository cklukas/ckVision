<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Live log models and sources

`widgets::LogModel` stores a bounded read-only log and incrementally decodes a
byte stream. It is UI-thread-owned, with no filesystem, environment or clock
access. Native file services implement the dependency-neutral contracts in
`<cvision/core/log_source.hpp>`. The view, follower and native implementations
are being delivered in subsequent live-log packages.

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
or bounded Read offset/length ranges. `take()` transfers one immutable result;
read results are consumed once while notification hints may coalesce.
Inspect selects the pathname and Read addresses the session's opened file.
The result reports native identity, size, opaque version, source/watch status,
offset, native error and whether reconciliation is necessary.

Request/take/cancel/session retirement must not perform native I/O or join a
worker in UI dispatch. Destroy the application-owned service outside dispatch.
Reject stale revisions before applying a result. Pure controllers use an
injected clock or caller-provided time for retries and reconciliation.
