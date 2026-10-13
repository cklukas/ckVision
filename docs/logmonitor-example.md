<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Log Monitor example

Build with examples enabled and run `ckvision_logmonitor`. The same portable
graph builds on macOS, Linux and Windows. It starts at Sources, with Open File,
Sample Stream and an application-supplied system-source catalog. Ordinary user
permissions suffice for File/Sample. Native system adapters and their host
verification are WP-82 delivery; unavailable entries explain recovery options.

Enter an absolute file path and choose Open. The shipped native file service
and LogFollower read a bounded recent tail, then follow the pathname through
supported append/rotation/truncation. Each named tab owns retained history and
its follow/inspect position. Source failures retain the view; Retry reopens a
bounded recent tail in a new generation. Earlier retained generations remain
identified separately. Stop stops intake; Close retires the source and tab.

The panel's Filter field applies a shared include literal across source tabs.
Exclude, severity and source controls add explicit predicates. Unknown severity
stays unknown for ordinary files. Find is separate: Ctrl+F, F3 and Shift+F3
search the published eligible entries. Query rebuild/backlog status stays
visible. Sources (Ctrl+O), Pause/Live (F5), Details (F4), Stop (F6), Retry (F7),
Copy selection (Ctrl+C), Export filtered retained (Ctrl+E) and Close source
(Ctrl+W) are ordinary commands under Logs. Narrow layouts retain the menu paths.
Wrap lines is a menu choice. Scrolling up pauses presentation while intake
continues; Resume/End returns to the tail with unread accounting.

Samples use supplied monotonic time and no disk files or helper permissions.
Steady emits one record per 100 ms, Burst 32, and Overload 128 into a four-slot
DropOldest queue. Loss is a visible gap, distinct from retention eviction.
Rotation/Truncation use an injected deterministic range/identity adapter and
the real LogFollower. Their notifications, offsets and generation transitions
follow the same library contracts as a native file. No generated source chooses
a hidden temporary directory.

Details inspect the selected retained identity, or the current live/paused
entry. They show source, generation/sequence, observed level, source timestamp
or unknown, fields, safe display text, escaped raw bytes and three neighbors
on each side. Context does not alter filtering. Copy uses grapheme-safe selected
display text and the injected clipboard.

Export offers selected **whole records touched by the selection** or the
current **published filter's retained window**. It does not claim durable/full
source history. The snapshot records source/query revision and include/exclude/
severity/source scope. While copying up to 128 records/64 KiB per turn, only
that source's ingress is held; other sources and input continue. The 2 MiB
snapshot limit rejects excess rather than silently exporting a partial result.
Intake resumes before worker file I/O. Atomic create-only writing preserves
existing filenames; choose another name after conflict. Snapshot construction
and write completion are shown separately. Changing the filter afterwards does
not change an already captured export.

The native composition constructs file and export workers outside dispatch.
Open/reopen requests are admitted in the outer loop, then allocate bounded
models/followers outside event dispatch. The application has eight source slots,
2048 records and a 1 MiB arena per model, with an 8192-byte raw-record limit.
Source publication, matching and panel refresh use LogController's yielding
deadlines. Bounded Application frame delivery coalesces slow host output while
input continues. Idle lanes need no controller poll timer; native file
reconciliation remains explicit. Export completion wakes through the activity
latch, with a 50 ms polling fallback for injected writers without a wake.

The example library injects FileSystem, LogFileService, ExportWriter, LogActivity
and NativeSourceDescriptor/StreamFeed. OS discovery/schema mapping belongs to
the catalog/feed, not widgets. The view tree dies before borrowed source models,
and native workers join only after the controller/receivers retire. See
[Live logs](live-logs.md) and [private process streams](process-helpers.md#continuous-process-output)
for bounds, ownership, source continuity and physical-visibility distinctions.

The headless graph has ten pinned states and workflow tests, plus real native
file/atomic-export checks. Full three-platform, installed-client, performance
and actual native-window acceptance remains the WP-84 matrix. Headless frames
alone are not evidence of what a user's running terminal displays.
