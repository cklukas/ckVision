# Editor and syntax-highlighting architecture

## Purpose and scope

ckVision 0.1 includes a reusable, terminal-native source and document editor.
It is a first-class library capability, not an application-specific `Memo`
variant. The editor provides document mutation, selections, undo/redo,
search/replace, line/column indication, syntax styling, and the primitives
needed by a file-editor application. `Memo` remains a compact form-field
control; `TextView` remains read-only rich display.

This design is the authority for WP-39 through WP-44. It deliberately makes
the editor useful to ordinary clients without requiring an IDE, a filesystem,
threads, a locale, a clock, or a third-party parsing dependency.

### Included

- UTF-8 text documents, normalized internally to LF, with grapheme-safe caret,
  selection, movement, deletion, and visual-column behavior.
- Large-file-capable incremental editing with transactions and bounded undo/
  redo history.
- A `TextEditor` widget with scrolling, selection, clipboard integration,
  keyboard and mouse editing, optional gutter, and an editor indicator.
- Literal search and replace, including replace-all as one all-or-nothing undo
  transaction. A bounded, injected regular-expression engine may extend it;
  the standard library must not use an unbounded backtracking engine.
- Incremental syntax highlighting selected by deterministic language profiles.
  JSON, YAML, and Bash are the shipped profiles; plain text is always the safe
  fallback.
- An injected file-editor workflow: load, save, save-as, dirty state,
  close-veto, encoding/newline metadata, and externally changed-file conflict
  handling.

### Not included in this delivery

- A terminal emulator, binary/hex editor, language server, compiler/debugger,
  source-control client, code completion, formatter, or project model.
- Rich-text layout, proportional fonts, bidirectional text editing, or
  arbitrary inline graphics inside a document.
- A mandatory grammar runtime or a global language-profile registry.

## Architectural shape

The implementation is split along stable responsibilities. WP-39 records the
precise repository layer placement in `ARCHITECTURE.md` before code lands; the
placement must preserve its dependency rules.

| Component | Responsibility | Must not do |
|---|---|---|
| `EditorDocument` | Text storage, revisioned edits, line index, transactions, undo/redo, encoding/newline metadata | Paint, input, filesystem access, language detection |
| `EditorSearch` | Literal matching and replace plans over a document revision | Mutate outside an explicit document transaction or use unbounded matching |
| `TextEditor` | Carets/selections, viewport, input, clipboard bridge, paint, accessibility/focus state | Open files, choose process-global profiles, own hidden global state |
| `SyntaxProfileRegistry` | Instance-owned profile registration and deterministic detection | Read the filesystem or infer state from locale/environment |
| `ISyntaxHighlighter` | Line scanner from lexical state to semantic token spans | Mutate the document or emit unvalidated positions |
| `SyntaxCache` | Incremental lexical-state and styled-span cache | Change document contents or make paint depend on timing |
| `FileEditorController` | Explicit injected load/save, dirty/conflict state and close protocol | Bypass the platform filesystem abstraction or silently overwrite a conflict |
| `EditorWindow` convenience | Optional composition of editor, title, indicator, standard commands and dialogs | Hide the underlying document/controller APIs |

All ownership is explicit. A client can share an `EditorDocument` between a
`TextEditor`, search UI, status model, and controller through an ordinary
shared lifetime handle. The registry is owned by the relevant application or
editor service object and passed to editors; no static registration, singleton,
or hidden process registry is permitted.

## Document model

`EditorDocument` stores an immutable original buffer plus append-only inserted
storage. A balanced piece index (or equivalent rope) augments each subtree with
byte count, line-break count, and enough boundary information to locate
grapheme-safe positions without copying the entire document. The public model
uses a revisioned `DocumentPosition` / `DocumentRange`, never raw pointers into
storage.

Every mutation is a `TextEdit` applied inside a transaction. A successful
transaction increments the revision once and records one inverse undo item;
failed validation changes nothing. Undo and redo replay transactions, preserve
selection restoration metadata, and have explicit memory/entry limits selected
by document options. The document emits a deterministic change record:
old/new revision, replaced range, inserted text extent, and affected logical
line range. Observers are direct, scoped subscriptions with safe removal.

Internal line endings are LF. The load path records the detected input newline
convention, encoding, and BOM policy so the save path can preserve or
deliberately change them. Invalid UTF-8 is an explicit load error or an
explicit replacement policy chosen in `EditorOpenOptions`; it is never silently
reinterpreted by widget code.

## Editing, search, and display

`TextEditor` is a dedicated multi-line widget. It supports a primary selection
and extensible multi-selection state, insert/overwrite mode, word/line/page/
document navigation, indentation/tab policy, drag selection, auto-scroll while
dragging, clipboard cut/copy/paste, and visible caret handling. Rendering maps
document positions to terminal cells using the existing Unicode width and
clipping rules. A selection or caret may never split a grapheme cluster.

The gutter and `EditorStatusModel` expose line, column, selection extent,
modified state, insert/overwrite mode, profile, encoding, and newline mode.
They are ordinary model data that a client may render in a status line or a
window frame overlay; they are not a hard-coded window decoration.

Search begins with deterministic Unicode-aware literal matching and explicit
case/whole-word/direction options. It produces revision-bound matches, so a
replace command cannot apply stale offsets. Replace-all validates its complete
plan against the current revision and commits exactly one transaction. An
optional injected matcher can add regular expressions, but it must expose a
work/cancellation budget and bounded failure mode; no user search may turn into
an uninterruptible backtracking computation.

## Syntax-highlighting design

Syntax highlighting is semantic styling over unchanged document text. It has
three layers:

1. A `LanguageProfile` has a stable ID, display name, deterministic detector,
   theme-role mapping, and highlighter factory. Detection receives explicit
   open metadata (requested profile, filename, content prefix, and optional
   shebang) and returns a score plus reason. Explicit selection wins; otherwise
   the highest score wins and equal scores use a documented stable ID order.
   No environment or filesystem probe participates.
2. An `ISyntaxHighlighter` accepts one logical line and an immutable
   `LexerState`, then returns token spans and the next state. Spans are checked
   UTF-8 byte subranges at grapheme boundaries. Lexers are total: malformed or
   incomplete input yields error/plain spans and a valid next state, never a
   crash or an unbounded scan.
3. `SyntaxCache` stores each line's incoming state, spans, and outgoing state.
   On an edit it invalidates the changed line range, relexes forward, and stops
   as soon as an unchanged line again has equal incoming state, outgoing state,
   and spans. Thus a local edit normally reparses only a local suffix, while
   multiline strings, block scalars, and heredocs correctly carry state.

Paint combines base text styling, syntax spans, search matches, selection, and
caret in a documented precedence order. Selection and focused-caret state are
never overwritten by syntax color. Highlighter work is incremental and
budgeted: visible dirty lines are completed for the next frame, and off-screen
continuation is an explicit bounded application task. `SyntaxCache::update()`
offers the complete deterministic pass; `SyntaxCache::update_bounded()` accepts
a maximum number of lines and reports whether work remains. Repeating the
bounded call with the same source resumes at the recorded invalidation point
and produces the same cache as one complete pass. There is no hidden worker
thread or timing-dependent paint result.

The standard profile building block is a compiled rule set: keyword trie,
delimiters, comments, string/number/operator rules, and a compact value-type
lexer state. It covers common languages without a runtime regex grammar.
Profiles needing stateful behavior implement `ISyntaxHighlighter` directly.
Clients extend the registry with a C++ profile factory or the declarative rule
set API, using stable IDs and testable detectors.

The built-in profile contract is:

| Profile | Detection | Minimum semantic categories |
|---|---|---|
| Plain text | fallback or explicit | plain text, invalid-input marker where applicable |
| JSON | `.json`, `.jsonc`, explicit selection | property key, string, number, boolean/null, punctuation, error |
| YAML | `.yaml`, `.yml`, explicit selection | mapping key, scalar, comment, directive/tag/anchor, punctuation, error |
| Bash | `.sh`, `.bash`, Bash shebang, explicit selection | keyword, command/operator, comment, string, variable/expansion, heredoc, error |

Theme roles include editor text/background, gutter, line number, indicator,
selection, search match, syntax keyword, type, property, string, number,
comment, operator, escape, and error. Each role has an explicit fallback to the
plain editor scheme, and all built-in themes define the roles before release.

## File-editor workflow

File operations cross only an injected platform service. WP-44 extends that
service with explicit read, metadata/fingerprint, and atomic replacement
operations where the host supports them. `FileEditorController` records the
loaded fingerprint and clean document revision. Saving writes a complete,
validated output through the service, updates clean state only after success,
and refuses a changed-on-disk file unless the client resolves the conflict.

`EditorWindow` is a convenience composition, not the only API. It supplies
standard File/Edit/Search commands, title and dirty marker, Save/Discard/Cancel
close confirmation, and a status presentation. Applications may compose the
same document and editor controls into another shell.

## Required evidence

Every package adds its own focused tests, public reference material, and a
client-facing example. Final acceptance includes deterministic golden frames
for plain and highlighted documents, all built-in themes, selection/search/
error overlays, narrow-terminal behavior, malformed input, profile detection,
load/save conflict paths, and keyboard/mouse editing scripts. Fuzzing covers
document edits, Unicode boundaries, rule sets, profile detection, and malformed
JSON/YAML/Bash input. Benchmarks establish non-regressing edit, viewport-paint,
and incremental-relex budgets on the pinned reference host.
