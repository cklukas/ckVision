# ckVision Standard Commands, v1

The library's own command set (`include/cvision/ui/command.hpp`,
`src/ui/command.cpp`) is declared by every `CommandRegistry`'s
constructor and reached through `CommandRegistry::standard()` —
`registry.standard().quit`, `.help`, `.menu`, and the rest. An
application never re-declares this set; it attaches its own handler to
whichever of these its widgets actually need, via
`CommandRegistry::set_handler` or the `Application::set_command_handler`
convenience forward.

## Identity is a key; the id is an assigned handle

A command declares itself under a namespaced string key and the registry
assigns it a `CommandId`:

```cpp
const ui::CommandId save = app.commands().declare({
    .key = "editor.save", .title = "&Save", .category = "File",
    .chord = "Ctrl+S", .handler = [this] { save_document(); }});
```

No source file anywhere — the library's, a widget's, an extension
library's, or the application's — writes a command id. That is what
makes the space collision-free: the prefix of the key belongs to the
declaring party (`ckv.` here), two parties cannot pick the same number
because neither picks one at all, and within one party a repeated key is
the same command by definition. `CommandId` is an `enum class`, so
`base + index` — the arithmetic that made ranges necessary, and then
made them collide — does not compile.

Consequences worth stating plainly:

- **Ids are per-registry and per-run.** They are assigned in declaration
  order from a monotonic counter. Never persist one, never write one in
  a configuration file, never send one to another process. Persist the
  key and resolve it with `CommandRegistry::id_for(key)`;
  `key_for(id)` is the reverse.
- **`declare()` is idempotent per key.** Re-declaring returns the same id
  and replaces the metadata, so a surface that rebuilds its commands
  keeps every id its menu and status entries already hold. An empty
  `.handler` leaves an installed handler alone.
- **`withdraw()` retracts a declaration** — metadata, handler, enablement
  predicate and key bindings together — while the key keeps its id
  reserved, so a re-declaration cannot hand out an id that has come to
  mean something else. `MenuBar` uses exactly this for the
  Alt+&lt;mnemonic&gt; accelerators it rebuilds whenever its menus change.
- **Visibility is metadata, not arithmetic.** `CommandVisibility::Palette`
  or `Hidden` states whether `widgets::CommandPalette` lists the command.
  The standard set below is `Hidden`; so are `MenuBar`'s menu
  accelerators, which duplicate a menu the reader can already see. An
  application that wants one of them browsable calls
  `set_visibility(standard().quit, CommandVisibility::Palette)`.

## Why a standard set at all

Before this landed, every example independently defined its own
"quit," "activate the menu," "tile windows" commands — same concept,
three different ids, three different (and sometimes inconsistent)
chord/title choices. A shared, framework-owned command for each of these
lets:

- an application skip re-declaring metadata for a concept the
  framework already models (`examples/gallery` declares no commands of
  its own at all — every command it uses is standard);
- library-internal machinery (the default keymap, WP-13; the modality
  stack's close-request sweep, WP-15) bind default behavior to a
  command that exists from the moment a registry does, without needing
  an application to have declared it first;
- a menu or status-line item reference the same command everywhere it
  appears and render consistent title/chord/enablement (M9/WP-11) no
  matter which application built it.

Menu and status entries may also use `widgets::CommandPresentation` to give a
surface-specific label or mnemonic while still referencing the same
`CommandId`. That is how `examples/hello` presents one quit command as
`Exit` in the File menu and `Quit` in the status line without a second
handler, chord, enablement rule, or command identity. Declare a command
of your own only when the concept itself is different, not merely when
one surface needs different wording.

## The set

Generated from the actual declaration
(`tools/docgen/generate_standard_commands_table.cpp`, run by hand —
regeneration is a manual, reviewed act, exactly like a golden-fixture
update, never automatic). The rows are in declaration order, which is
also the order `CommandRegistry::all()` reports them in:

| Key | Title | Category | Default chord |
|---|---|---|---|
| `ckv.app.quit` | `&Quit` | System | Alt+X |
| `ckv.app.help` | `Help` | System | F1 |
| `ckv.app.menu` | `Menu` | Window | F10 |
| `ckv.window.close` | `&Close` | Window | Alt+F3 |
| `ckv.window.zoom` | `&Zoom` | Window | F5 |
| `ckv.window.next` | `&Next` | Window | F6 |
| `ckv.window.previous` | `&Previous` | Window | Shift+F6 |
| `ckv.window.tile` | `&Tile` | Window | — |
| `ckv.window.cascade` | `C&ascade` | Window | — |
| `ckv.window.list` | `&Window List` | Window | — |
| `ckv.focus.next` | `Next Field` | Window | Tab |
| `ckv.focus.previous` | `Previous Field` | Window | Shift+Tab |
| `ckv.app.terminal_report` | `&Terminal Report` | System | — |
| `ckv.window.tile_horizontal` | `Tile &Horizontally` | Window | — |
| `ckv.window.tile_vertical` | `Tile &Vertically` | Window | — |
| `ckv.window.tile_grid` | `Tile &Grid` | Window | — |
| `ckv.window.minimize` | `Mi&nimize` | Window | — |

Application code references these through
`CommandRegistry::standard()` — `.quit`, `.close`, `.zoom`,
`.next_window`, `.previous_window`, `.tile`, `.tile_horizontally`,
`.tile_vertically`, `.tile_grid`, `.cascade`,
`.window_list`, `.menu`, `.help`, `.terminal_report`, `.focus_next`,
`.focus_previous`, `.minimize` —
rather than by key; the keys are spelled out in
`ckv::ui::std_command_keys` for anything that has to name a command as a
string, and this table exists to make the wording, grouping, and chord
choices reviewable in one place.

## Design notes

- **`&`-mnemonics are deliberately included** on every title except
  `menu` and `help`. Those two are invoked by their chord directly
  (F10, F1) and are not typically menu ITEMS themselves — an
  application that does place one in a menu of its own can still
  `parse_mnemonic()` over whatever text it wants there (see the
  "declare your own command" escape hatch above). `cascade` uses
  `C&ascade` (mnemonic on 'a') rather than 'c', since `close` and
  `cascade` can plausibly coexist in the same Window menu. The three
  explicit tilings put their mnemonic on the distinguishing word
  (`Tile &Horizontally`, `Tile &Vertically`, `Tile &Grid`) for the same
  reason: a Window menu carrying all of them alongside `&Tile` would
  otherwise have four items competing for 'T', and the axis — or
  "Grid" — is the word the reader is actually choosing between.
- **The tilings are fixed by the arrangement, not by the word.** The
  two axis names are used inconsistently across desktops, so the set
  states which is which: **the axis names what the windows are laid out
  ALONG, not the direction of the dividers between them.**
  `tile_horizontally` lays full-HEIGHT bands side by side, in a row
  across the desktop; `tile_vertically` lays full-WIDTH bands stacked
  down it; `tile_grid` lays a near-square grid of `ceil(sqrt(n))`
  columns, filled row by row, its last row stretched across the full
  width so the grid is still an exact cover. `tile_horizontally` produces
  exactly the arrangement `tile` has always produced. The two are not
  merged because `tile` is a command applications already bind, and
  re-pointing it would change behavior under callers that never asked
  for a change.
- **`tile`/`tile_horizontally`/`tile_vertically`/`tile_grid`/`cascade`/
  `window_list`/`terminal_report` have no default
  chord.** There is
  no comparably strong, widely-recognized single-key convention for
  them the way there is for Quit/Close/Zoom/Help/Menu/window-cycling —
  an application binds one itself (`CommandRegistry::bind_key`) if it
  wants one.
- **The chord scheme is this project's own choice**, authored for
  M9/WP-12 with no prior source consulted, per this repository's own
  provenance rule (the engineering standard) — informed by publicly documented,
  widely-known DOS-era TUI conventions (F1 help, F10 menu, F6 window
  cycling), not derived from any specific prior implementation's
  source code.
- **`help`'s default handler** resolves the focused view's nearest
  help-context key (`View::resolve_help_context_key`, D-027) and hands
  it to whatever `Application::set_help_provider` installed — a no-op
  if no provider is set or no key resolves anywhere in the focus
  chain. F1 therefore always reports as "handled" once unconsumed by
  the focus chain (the command ran), regardless of whether the
  provider itself found anything to show; that visible-effect nuance
  is observable through the provider callback, not through
  `Application::dispatch`'s return value — the same way every other
  standard chord already works.
- **Modal scope preserves field navigation and context help.** After a
  modal control and its ancestors decline a key, `focus_next`,
  `focus_previous`, and `help` remain available: Tab and Shift-Tab traverse
  only the active modal subtree, while F1 resolves the focused modal control's
  help context. Every other standard command and every application-declared
  accelerator remains blocked until the modal scope ends unless that
  application command declares a named context that is active in the modal's
  focused ancestry. This allows modal-local commands while still preventing
  F10, window commands, Alt+X, and other background UI accelerators from
  leaking through.
- **Named command contexts** live in `CommandRegistry` metadata and can be
  activated by explicit push/pop scopes or by a focused view's ancestry
  (`View::set_command_context`). A context-bound command is unavailable until
  one of those sources names its context. `CommandRegistry::withdraw`
  removes the command's metadata, handler, enablement predicate, and key
  bindings together so stale menu/status references become inert.
- **`close`/`quit`'s default handlers** (M9/WP-15) are installed by
  `Desktop::on_attached()` — but only if nothing has claimed the
  command yet (`CommandRegistry::has_handler`), the same guarded pattern
  `menu`'s own default follows. `close` closes the desktop's active
  window (vetoable, `Window::close_request`); `quit` sweeps every
  window through that same vetoable protocol, front-to-back, and only
  calls `Application::request_quit()` if none of them veto
  (the architecture §5 "application quit sweeps all windows through
  the same protocol"). Modal scoping (`Application::push_modal`)
  suppresses both while a modal dialog is open, the same as any other
  standard chord. The sweep snapshots the instances present when it begins:
  a close callback may detach or replace windows, but a newly presented
  replacement is not unexpectedly closed as part of that original request.
  If such a callback executes `quit` again, the nested default request is a
  no-op; the original sweep remains the sole authority for later vetoes and
  the final shutdown decision.
