# Editor delivery target and acceptance plan

## Product target

The ckVision editor delivery gives clients a credible foundation for a source editor or text-document application without making ckVision itself an IDE. A small client can create a document, place it in `TextEditor`, register the standard profiles, and obtain usable editing, syntax coloring, command routing, and status information. A file-oriented client can add `FileEditorController` or `EditorWindow` for a complete, explicit file lifecycle.

The architecture and behavioral constraints are in [editor-architecture.md](editor-architecture.md). This document turns them into release targets and evidence. It is binding for WP-39 through WP-44.

## Client experience target

The shipped `examples/editor` program is the public proof point. It opens a document through an injected filesystem, detects YAML/JSON/Bash or falls back to plain text, presents line/column and modified state, exposes File/Edit/Search menus plus status commands, and demonstrates search/replace, save/close veto, and profile selection. Its documentation includes complete source excerpts, object hierarchy, screenshots of each teaching state, a command table, and the exact build/run path.

The public API is expected to support this shape (names may be refined before implementation, responsibilities may not):

```cpp
auto document = std::make_shared<cvision::editor::EditorDocument>(text);
cvision::editor::SyntaxProfileRegistry profiles;
cvision::editor::register_standard_profiles(profiles);

auto editor = std::make_unique<cvision::widgets::TextEditor>(document, profiles);
editor->set_show_line_numbers(true);
editor->set_profile_selection(cvision::editor::ProfileSelection::automatic());
```

File opening is deliberately separate and receives an injected service and explicit options. A client can keep the lower-level pieces and omit the convenience window/controller.

## Delivery sequence

| Work package | Deliverable | Exit proof |
|---|---|---|
| WP-39 | Revisioned UTF-8 document, piece index, transactions, undo/redo, position/line APIs | Deterministic unit/fuzz corpus; Unicode and transaction goldens; edit benchmark |
| WP-40 | `TextEditor`, selections, input, scrolling, gutter, indicator/status model | Keyboard/mouse scripts and all-theme visual goldens via public API |
| WP-41 | Deterministic profile registry, rule-set compiler, incremental lexical cache, theme roles | Detection and convergence tests; malformed-input fuzzing; incremental-relex benchmark |
| WP-42 | Standard JSON, YAML, and Bash profiles plus extension guide | Checked-in profile fixtures/goldens; profile author sample; all built-in themes |
| WP-43 | Search/replace and editor command integration | Revision-staleness, replace-all atomicity, focus/modal, and command enablement scripts |
| WP-44 | Injected file-editor controller/window, `examples/editor`, reference/tutorial coverage | Memory-filesystem workflow tests, conflict/close/save goldens, documented runnable example |

The order is intentional: data integrity first, then interaction, then semantic styling and shipped profiles, then command workflow and file lifecycle. Each package may refine public names only by updating the architecture plan and all callers/tests together; it must not reduce the target or move it back onto `Memo`/`TextView`.

## Acceptance matrix

| Capability | Required acceptance |
|---|---|
| Text integrity | Every public edit preserves valid position rules, deterministic revisions, undo/redo, LF normalization, and grapheme-safe movement/deletion |
| Viewport | Large documents paint only the required viewport plus bounded cache work; scrolling and gutter alignment remain correct on resize |
| Input | Keyboard, mouse click/drag/double-click, clipboard, focus cycling, disabled/read-only state, and modal command scoping are covered |
| Search/replace | Forward/backward, case/whole-word, empty/no result, stale revision rejection, replace-one, and atomic replace-all have script and unit evidence |
| Detection | Explicit profile, suffix, shebang, content-prefix, fallback, and tie behavior are deterministic and documented |
| Highlighting | JSON/YAML/Bash valid and malformed fixtures cover multiline lexical state; a changed line converges to a cache fixed point without repaint-order dependence |
| Themes | All new editor and syntax roles are defined in Classic, Dark, Light, and Mono; selection/search/caret precedence remains legible |
| Files | Open/save/save-as/error/conflict/encoding/newline/dirty/close-veto behavior is tested through injected filesystem fakes; no direct filesystem use appears below the platform boundary |
| Public usability | `examples/editor`, API reference, profile author guide, screenshots, source snippets, hierarchy, and diagnostics are generated and checked like the other examples |
| Quality gates | Layer/provenance checks, warning-clean builds, full existing test matrix, new fuzz targets/corpus, sanitizers, and pinned-host performance budgets hold; WP-37 and WP-38 retain their global platform/release responsibility |

## Non-negotiable implementation constraints

- No mandatory external dependency, vendored grammar engine, process-global profile registry, direct filesystem read, wall-clock work scheduler, or locale-dependent behavior.
- No silent input rewriting, unsafe conversion of invalid UTF-8, implicit overwrite of an externally changed file, or search engine with unbounded worst-case behavior.
- Syntax profiles must be deterministic values with stable identifiers and isolated test fixtures. Extensions are registered explicitly by the client.
- Every new public editor surface has reference documentation and a test that exercises it through the public API. Every visual promise has a golden or a documented, executable non-visual oracle.

## Release handoff

WP-44 does not declare release success. It supplies the editor evidence to WP-37's cross-platform matrix and WP-38's independent release audit. Those packages must verify the editor's complete state/theme/reference matrix before M11 can close.
