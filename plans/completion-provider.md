<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->
<!-- SPDX-License-Identifier: MIT -->

# Input completion and file-dialog integration

Status: implementation in progress, 2026-10-11. D-139 adopts the owner's
requested behavior. This document does not claim acceptance. The authority
chain and verification gates remain binding.

## Intended behavior

Completion is a general InputLine capability supplied by an instance-owned
provider. FileDialog is the first concrete integration. Its ordinary path field
uses the same editing machinery as other fields; filename discovery and path
policies belong to the file-specific provider and its injected services.

- Show a theme-defined subdued suffix after the committed text, only when the
  candidate is an exact prefix extension at the caret. Gray is an appearance
  choice in a theme, not a hardcoded color.
- Plain Right accepts that suffix only at the end of the field with no
  selection. Otherwise Right moves the caret normally. Modified arrows retain
  their editing meanings.
- Tab and Shift+Tab retain focus navigation in provider-driven fields and
  responsive dialogs. The released synchronous filesystem-only convenience
  overload remains distinct under D-137.
- Preview is not part of text(), validation, clipboard contents, history, or
  search callbacks. Acceptance is one undoable edit and one edit notification,
  through the existing admission and validation rules.
- Enter does not accept an uncommitted preview. FileDialog interprets committed
  directory paths as navigation, as specified below; other fields retain their
  enclosing field/dialog's accept action. Escape
  first suppresses a visible suggestion without changing text; a subsequent
  Escape reaches the enclosing control. Suppression lasts for the same query
  revision so a late result cannot immediately restore it.
- Focus loss, selection, and incompatible caret movement remove the preview.
  Password and masked fields do not offer completion. Completion is optional.
- Providers may return arbitrary grapheme replacement ranges, but those
  candidates require explicit presentation if they cannot be shown honestly
  as an appended suffix. First delivery focuses on inline prefix completion.
  Preview boundaries are checked with the preceding committed grapheme, so
  inserting a combining mark at an empty replacement range cannot silently
  change that grapheme. File-provider ranges are measured in the full editing
  state; a basename boundary inside a grapheme includes the unchanged parent
  fragment in its replacement rather than duplicating a mark.

## General provider boundary

The public provider boundary consists of CompletionProvider, CompletionRequest,
CompletionCandidate, and CompletionSnapshot, documented in docs/completion.md.

A request identifies the committed text, grapheme caret/selection, and query
revision. A candidate identifies a valid grapheme replacement range, insertion
text and stable candidate identity. A snapshot has bounded
candidates and explicit pending/partial/complete/error state. InputLine owns
preview and edit state; the provider supplies candidates and ordering.

Provider reads on UI dispatch must be bounded and nonblocking. They must never
perform filesystem access, full-source matching/sorting, or worker waits.
Externally produced immutable results arrive through explicit UI-thread
publication with the request revision. Editing, changing context, or retiring
an owner makes old results ineligible. No global registry or hidden worker pool.
Source execution and lifetime use explicit injected services. Completion lists
remain explicit composite controls, consistent with D-042.

## One file model for listing and completion

FileDialog owns a directory model and a FileCompletionProvider over that model.
Both the visible file rows and completion candidates use one eligibility rule:
the effective directory, active FileDialogFilter, hidden-file setting, and typed
basename prefix. Directory entries pass extension filters so navigation remains
possible. Reuse existing extension semantics, including case-insensitive suffix
matching and the empty-filter meaning. Filename prefix matching and path
spelling are explicit policies; do not silently apply the suffix rule to every
filesystem's name semantics.

Proposed filename-entry behavior:

1. Typing a basename narrows the entry rows to matching eligible files and
   directories. Clearing the prefix restores the eligible directory listing.
   Navigation rows such as parent/recent locations are separate from filename
   candidates.
2. Highlight and reveal the suggested candidate in the existing file list
   without moving keyboard focus out of the path field. Use a suggestion
   indication distinct from a user-committed selection and do not activate it.
3. Selecting another matching row chooses the candidate previewed by the field.
   Keep current directory/file activation meanings; mere suggestion updates
   must never navigate, accept the dialog, or replace committed input.
4. Right accepts precisely the highlighted candidate. For a directory, include
   a separator and continue discovery for the next path segment; it does not
   submit the dialog.
5. Changing extension filters or hidden-file visibility invalidates the old
   suggestion immediately, then updates both presentations from the same model
   revision. Retain committed text and valid caret/selection; filtering must
   never reset the field to the directory or suggested save name.
6. A typed parent path changes the effective listing/completion context together.
   Show that context to the user; pending or invalid parent discovery must not
   display stale rows as matches for the new path. Relative paths resolve from
   an explicit navigation base, not a preview-induced moving base.

Use stable entry identities for suggestion and selection across incremental
updates and reordering. Only entry candidates can produce filename suffixes.
Publish listing and candidate changes coherently; an excluded or retired row
must not remain the field's accepted suggestion. Incomplete discovery does not
establish that a match is unique. A user's explicit candidate choice must not
be replaced merely because a later batch sorts before it. Save mode continues
to allow a new name: suggestion eligibility is not an existence validator.

## Enter on a directory path in the open dialog

In Open mode, Enter in the path field resolves the committed text against the
explicit current navigation directory. An absolute path uses its specified
location. Relative paths, including nested names, '.', '..', and combinations
such as '../images', use filesystem-defined resolution from that base. An
existing directory navigates the dialog instead of producing an accepted file
result. A trailing separator is optional for an existing directory.

With the navigation base '/home/user', the intended examples are:

| Entered text | Enter outcome |
|---|---|
| 'docs' or 'docs/' | Navigate to '/home/user/docs' if it is a directory |
| '/home/user/docs' | Navigate to that absolute directory |
| '..' | Navigate to '/home' |
| '../images' | Navigate to '/home/images' if it is a directory |
| '.' | Stay in the current directory; do not accept or close |
| 'readme.txt' | Retain the existing file-accept workflow |

Directory navigation preserves the dialog, path-field focus, active extension
filter, and hidden-file policy. Update the committed navigation base, path
field's directory prefix, listing and completion context coherently, then allow
typing the next segment. Clear old suggestions and old basename filtering.
Directories are navigable regardless of file extension. At a filesystem root,
'..' stays at that root according to the filesystem service's semantics.

Path resolution and directory classification must use the injected service
outside UI dispatch, including slow/network paths. The existing
FileSystem::normalize_path explicitly preserves '.' and '..'; normalization
alone does not implement this contract. Specify filesystem-aware resolution,
including symlink semantics, in the service design rather than inventing a
lexical replacement in InputLine.

Navigation classification also verifies that the resolved directory can be
opened before committing the navigation base. This probe is worker-owned and
never enumerates the directory; failure keeps the previous base and typed text.
A directory picker Select request requires existing-directory identity without
requiring readable contents. Both replies retain their original revision.

An Enter request captures both the text revision and navigation base. While
classification is pending, editing, focus navigation and cancellation remain
available. Later edits, another navigation, or dialog close invalidate the
request; a stale reply must neither navigate nor accept a file. Repeated Enter
for the same pending revision coalesces rather than starting duplicate work.
Do not report a file result before determining whether the path is a directory.
Right remains the explicit completion-accept key: Enter evaluates only committed
text, not the preview suffix or an automatically highlighted suggestion row.

Empty input does nothing. A missing/unreadable directory or failed navigation
keeps the previous navigation base, preserves typed text, and shows a useful
status. A directory-form path ending in a separator must not fall through to
file acceptance after failure. Non-directory filenames retain the caller's
existing file validation policy. Save mode also navigates existing directories on Enter submission, and accepts
non-directory names under its existing caller-owned validation policy. In a
directory picker, Enter in the path field navigates, while Select resolves and
accepts only an existing directory. A directory row activation navigates;
the Select button is the deliberate folder-accept action.

## Very large or slow directories

The current FileSystem::list_directory returns a whole vector. FileDialog's
initial refresh, navigation, and completion enumerate and sort synchronously.
This must be replaced for the new workflow, rather than moving only the key
handler off the UI thread.

Introduce an injected incremental directory-discovery contract with explicit
service ownership and concurrency rules. Enumeration, filtering, matching,
indexing, sorting, and large-result retirement happen outside UI dispatch.
The listing and completion share discovery; typing must not restart a directory
scan. Filtering a million discovered names must also stay outside dispatch.

Bound queued requests, result batches, publication work, and resident data.
A virtual row provider must not retain one UI label/control per directory
entry. Specify a bounded or paged index strategy for huge listings, including
any storage through injected services; no unlimited in-memory vector is an
acceptable scale contract. Do not silently truncate the accessible file list
to the completion candidate limit. Cache invalidation and refresh are explicit.

New queries supersede pending matching work. Navigation/filter changes carry
new revisions; old results cannot enter either presentation. Cooperative
cancellation occurs between discovery batches. A blocked platform operation
may not finish promptly: typing, dialog close, and provider retirement must
not join or wait for it. The service owns outstanding operation storage until
safe retirement, with bounded concurrency and no use of destroyed views.
Filesystem calls must not be assumed safe for simultaneous use without an
explicit service contract.

While discovery is pending, preserve immediate editing and focus navigation.
Show a modest loading state, keep candidates from the current revision only,
and accept ordinary input independently of discovery. No field draw, resize,
key handler, list-provider read, or UI publication may hide unbounded work.
Apply ARCHITECTURE.md's deterministic allocation and event-loop budgets; do
not invent a timing baseline or claim that asynchronous APIs prove latency.

## Adopted source execution and paging contract

Path classification follows the native adapter. POSIX absolute paths begin
with `/`; a component containing a colon (for example `C:/notes`) remains
relative to the dialog's base, rather than being mistaken for a Windows drive.
Windows retains its drive-rooted and UNC classification. Resolution still
runs through the injected service, never through field handlers.

The injected DirectoryService owns a fixed, explicitly configured number of
worker slots. A DirectorySession owns one slot until cancellation has been
acknowledged. Its request mailbox and publication mailbox each contain only
one immutable, size-bounded value; newer requests replace older ones. Closing
or destroying a session signals cancellation and never joins a worker. The
host must keep the service and its filesystem alive and destroy the service
outside UI dispatch, after dialogs have retired. A blocked slot stays occupied;
exhaustion reports unavailable instead of spawning unlimited replacement work.

NativeDirectoryService takes an explicit, exclusive index directory from the
host. Workers stream discovered entries into one disk index per slot, retaining
at most 64 ordered rows and bounded string bytes in memory. Query and filter
changes scan this index, not the filesystem again; discovery resumes from its
existing cursor. Canonical aliases share the same discovery generation, and
an unavailable typed parent does not discard the preceding usable index.
Matching and index reads are cooperatively superseded every
256 entries. Refresh explicitly replaces the source generation. Index errors
report failure; there is no unlimited memory fallback. Index files are removed
by their worker on retirement.

The file list shows an ordered page of up to 64 eligible rows, with explicit
previous/next page controls and a partial-discovery status. Every eligible row
remains reachable; the completion limit never silently truncates the directory.
Pages use stable entry keys as boundaries instead of an unbounded page-history
stack. File-name prefix matching is exact and case sensitive, while extension
matching retains the existing ASCII-insensitive suffix semantics. A selected
candidate can be retained alongside the page during partial updates, within the
same row and byte limits. Retention is evaluated after byte trimming, so a
selected row removed from the ordinary page is still retained as its pinned
row. If only that row fits, paging clears the pin and keeps the existing page
boundary rather than jumping to the directory's beginning or end.
For an unchanged canonical directory and generation, the bounded visible
subset is re-filtered immediately into a fresh partial query result while
the worker matches the complete index. These matches use the current prefix,
extension and hidden policy; they never establish uniqueness. Old worker
replies retain their original revisions and are still rejected.
All presentation reads and result retirement on the
UI thread are bounded. Native construction, shutdown, spool access and path
classification are application-boundary operations, never field operations.

D-137 preserves the released filesystem-only dialog overloads. New service
overloads are the responsive workflow and the recommended native integration;
the old overloads remain documented as synchronous, small scripted-tree
conveniences. They must not be used in the large-directory demo or as evidence
for asynchronous acceptance. The responsive overloads use the ordinary InputLine completion contract,
preserving Tab for focus navigation. The released filesystem-only overloads
retain their older interaction contract until a major-version replacement.

## Additional consumers

- SearchBox: query history, known names, and structured query terms through its
  existing InputLine field.
- Editable ComboBox: prefix suggestions from choices while allowing free text.
- Descriptor text fields: caller-provided paths, identifiers, and vocabularies.
- PropertyInspector text editors: per-property resource or configuration names.
- Application command-entry fields: command names and arguments from explicit
  application providers. CommandPalette's existing selection remains useful.

The first delivery is InputLine plus FileDialog. Additional consumers reuse the
same provider/edit contracts; they do not require a filesystem-aware input
base class or separate completion implementation per control.

## Acceptance evidence

Specify deterministic tests and goldens before marking implementation complete:

- Preview versus committed text, Right acceptance, ordinary arrows, Tab focus
  traversal, Escape suppression, focus/selection, Unicode widths and grapheme
  replacement boundaries, validation, password/mask behavior, and one-step undo.
- The same eligible entry identities in list and completion, active extension
  filters, directories retained, hidden-file toggles, prefix clearing, relative
  parent paths, save-new-name behavior, stable explicit candidate choice, and
  coherent updates without rewriting typed text.
- Open-dialog Enter on absolute/relative directories, nested paths, '.', '..',
  parent combinations, root boundaries, with/without trailing separators, and
  filesystem-specific resolution. Verify retained focus/filter/hidden settings,
  unchanged dialog lifetime, failed navigation, and ordinary filename acceptance.
  Delay directory classification, edit or close meanwhile, and prove stale
  replies cannot navigate or accept; test repeated Enter and unaccepted previews.
- Million-entry synthetic discovery, delayed and deliberately blocked services,
  rapid query/filter/navigation changes, close during pending work, stale-result
  rejection, bounded queues/memory/UI publication, and cancellation retirement.
- Interactive typing, caret movement, focus navigation, redraw and close remain
  available during slow discovery. Test initial listing and navigation as well
  as completion, including large-result destruction and background sorting.
- Measure dispatch latency against existing named performance budgets and
  record input/output identities, configuration, and evidence. Run the full
  cross-platform, sanitizer, fuzz, golden, provenance and benchmark gates from
  AGENTS.md; documentation-only checks do not establish feature acceptance.

## Implementation progress

2026-10-12: implementation now includes the general provider and InputLine
preview/edit machinery, theme role, native streaming cursors/path resolution,
DirectoryService and bounded native workers/disk index, responsive file/folder
overloads, paged eligibility/selection, asynchronous Enter navigation, refresh,
and Completion Lab using InputLine/SearchBox/ComboBox. The released synchronous
filesystem-only overloads remain distinct under D-137.

The latest focused macOS Release run passed 16 completion cases, eight directory
service cases, fourteen responsive-dialog cases, and five app/golden cases.
Coverage includes a million-entry index, blocked actual-dialog typing/focus/
redraw/close, canonical alias and failed-parent index reuse, byte-limited chosen
row retention, bidirectional paging from a pin-only publication, immediate
re-filtering of the known subset during a pending lookup, and Unicode ranges
that share a separator's grapheme. The list and preview use current eligibility;
late replies cannot navigate, accept, or replace a newer query.

Final full Release suites passed all 238 checks on macOS, 238 on Linux, and 232
on Windows, including the relevant provenance, layers, API comments, hygiene,
documentation, goldens, scripts and deterministic benchmark budgets. Windows
required a clean rebuild after MSBuild left one capture executable stale;
transferred changed inputs are timestamped after preceding builds before
incremental validation. All outputs have recorded configurations, input hashes,
times, logs and executable identities. ASan/UBSan and TSan full suites plus
focused reruns are green; the latest changed-surface checks and all eleven
fuzz corpora also passed. Long-Unicode-name list/ghost painting and borrowed
caption reads allocate no ordinary heap memory in the dedicated regression.

The native Completion Lab process passed a 16-stage PTY walkthrough including
relative/absolute/dot/parent navigation, a POSIX colon-bearing relative folder,
filtered completion, Right acceptance, cancellation and folder selection.
Its exact executable/PID, emitted bytes and decoded states were recorded;
it exited successfully and retired its index. A native Windows startup probe
verified wide arguments preserve a Unicode index path, before its deliberately
redirected terminal fails the console requirement. Neither probe claims a
GUI-window inspection. PendingDialogs keeps result handlers until detachment.

Remaining acceptance: idle-reference-host timing qualification and an actual
verified native-window walkthrough. Busy-host PTY/theme measurements are
only diagnostics and cannot establish the required idle baseline. Native
Terminal/Codex window inspection was denied by tool access restrictions.
Evidence is reconciled in the task's durable audit/build/test records. Disposable
sanitizer builds, the Mac source snapshot, scratch and generated test-export
projects have been removed; the verified Release build and native evidence
remain available. No feature
commit or acceptance claim has been made; keep the goal active while these
requirements remain unproven.
