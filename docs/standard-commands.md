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
| `ckv.window.select.1` | `Window &1` | Window | Alt+1 |
| `ckv.window.select.2` | `Window &2` | Window | Alt+2 |
| `ckv.window.select.3` | `Window &3` | Window | Alt+3 |
| `ckv.window.select.4` | `Window &4` | Window | Alt+4 |
| `ckv.window.select.5` | `Window &5` | Window | Alt+5 |
| `ckv.window.select.6` | `Window &6` | Window | Alt+6 |
| `ckv.window.select.7` | `Window &7` | Window | Alt+7 |
| `ckv.window.select.8` | `Window &8` | Window | Alt+8 |
| `ckv.window.select.9` | `Window &9` | Window | Alt+9 |
| `ckv.window.size_move` | `&Size/Move` | Window | Ctrl+F5 |
| `ckv.app.command_palette` | `Command &Palette` | System | Ctrl+Shift+P |
| `ckv.app.tooltip` | `Tooltip` | System | Ctrl+F1 |

Application code references these through
`CommandRegistry::standard()` — `.quit`, `.close`, `.zoom`,
`.next_window`, `.previous_window`, `.tile`, `.tile_horizontally`,
`.tile_vertically`, `.tile_grid`, `.cascade`,
`.window_list`, `.menu`, `.help`, `.terminal_report`, `.focus_next`,
`.focus_previous`, `.minimize`, `.select_window[0]` … `.select_window[8]`
for windows 1 … 9, `.size_move`, `.command_palette` and `.tooltip` —
rather than by key; the keys are spelled out in
`ckv::ui::std_command_keys` for anything that has to name a command as a
string, and this table exists to make the wording, grouping, and chord
choices reviewable in one place.

## Chord text follows the keymap

Menus and status lines ask the registry for a command's chord every time they
draw, and `Application` repaints on the step after any change to it
(`CommandRegistry::revision()` counts the changes), so a runtime
`bind_key`/`unbind_key` reaches every surface already on screen. Text an
application writes itself — a help sentence, a placeholder, a hint — follows
the same way when it takes the chord from `CommandRegistry::chord_text(id)`,
which is the bound chord in the display spelling, or empty when nothing is
bound. The examples never spell a chord in a string literal outside a
command's `.chord =` declaration; the `example_hygiene` gate rejects one.

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
- **Alt+1 … Alt+9 select a window by its number.** `select_window[n - 1]`
  runs `Desktop::select_by_number(n)`: the n-th window in the desktop's
  insertion order, the same numbering its window list shows, restoring the
  window first if it is minimized. A number past the last window does
  nothing. Naming a window by its digit with Alt held is the convention
  observed on classic text-mode desktops. No other standard chord uses Alt
  with a digit, and `MenuBar`'s accelerators take Alt with a menu title's
  mnemonic letter, so the two never compete. Both encodings a terminal sends
  for these keys — the legacy ESC prefix and the kitty keyboard protocol —
  decode to exactly these chords. A focused `TerminalView` forwards Alt+digit
  to its child program, as a terminal must, so there the window menu or
  `window_list` is the way to switch.
- **Ctrl+F5 moves and sizes the active window from the keyboard.**
  `size_move` runs `Window::enter_move_size_mode()` on the desktop's active
  window: the arrow keys move it a cell at a time, Shift+arrow resizes it by
  its bottom-right corner (Right and Down grow it, Left and Up shrink it),
  Enter keeps the result and Esc restores the bounds the mode began with.
  The frame holds the keyboard while the mode lasts, so the arrows reach it
  past a list or an editor that would otherwise take them, and every other
  key is swallowed rather than editing the content underneath. Ctrl+F5 is
  the Size/Move key observed on classic text-mode desktops, the Ctrl sibling
  of F5, which zooms. Nothing else in the standard set binds Ctrl with a
  function key, and every decoder path already reports Ctrl+F5 as that chord
  (the xterm modifier parameter, the kitty keyboard protocol and the Windows
  console alike). A focused `TerminalView` forwards it to its child, as it
  does every key.
- **Ctrl+Shift+P opens the command palette.** `command_palette` runs
  `Desktop::show_command_palette()`, which puts a `widgets::CommandPalette`
  up as a popup over the desktop (`widgets::show_command_palette`): it lists
  the palette-visible commands that apply where the reader was, disabled ones
  greyed, and is dismissed by Escape, a press outside it, or running a
  command. Ctrl+Shift+P is the chord observed for a command palette across
  contemporary editors and terminal emulators, and it competes with nothing
  here: no standard command takes Ctrl with a letter, and the Shift keeps it
  apart from a Ctrl+P an application binds for printing. Only a terminal that
  speaks the kitty keyboard protocol reports Shift with a Ctrl+letter; the
  legacy encoding sends Ctrl+Shift+P and Ctrl+P as the same control byte,
  which decodes to Ctrl+P and so never opens the palette by accident. On such
  a terminal an application offers the command where its reader can see it —
  a menu item or a status-line entry — or binds a chord of its own. The
  command itself is `Hidden` like the rest of the set, so the palette never
  lists the command that opened it; a focused `TerminalView` forwards the
  chord to its child program, as it does every key.
- **Ctrl+F1 shows the focused view's tooltip.** F1 asks about the focused
  view — its help topic; with Ctrl it asks for that view's short explanation
  instead. No other standard binding, no example application and no decoder
  path uses the chord, and both encodings a terminal sends for it — the
  legacy `CSI 1;5P` and the kitty keyboard protocol — decode to exactly it.
  The library installs no handler: a `widgets::TooltipController` claims the
  command when it is constructed, if nothing else has, and gives it back when
  it is destroyed. The tip it shows is held the way an open menu is, so the
  next key or a press anywhere dismisses it. A desktop that reserves Ctrl+F1
  for itself (macOS's keyboard-access toggle is one) keeps the key from the
  terminal; the command can then be rebound like any other.
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
- **Modal scope preserves field navigation, context help, tooltips and
  moving the dialog.** After a modal control and its ancestors decline a key,
  `focus_next`, `focus_previous`, `help`, `tooltip` and `size_move` remain
  available: Tab and Shift-Tab traverse only the active modal subtree, F1
  resolves the focused modal control's help context, Ctrl+F1 shows its
  tooltip, and Ctrl+F5 puts the modal window itself into the keyboard
  move/size mode — the active window while a modal is up is the modal, and a
  dialog covering what the reader needs to see has to be movable without a
  pointer. Every other standard command and every application-declared
  accelerator remains blocked until the modal scope ends unless that
  application command declares a named context that is active in the modal's
  focused ancestry. This allows modal-local commands while still preventing
  F10, window commands, Alt+X, and other background UI accelerators from
  leaking through.
- **Named command contexts** live in `CommandRegistry` metadata and can be
  activated by explicit push/pop scopes or by a focused view's ancestry
  (`View::set_command_context`). A command's `CommandScope` names the
  contexts it belongs to, and it is available while any one of them is
  active — an editor's Save can belong to the document and to the outline
  pane beside it. A scope that names none is available everywhere.
  `CommandScope::outside_contexts` adds the one place a list of names cannot
  state: where no context is active at all — nothing pushed, and nothing on
  the focus path naming one, such as the bare desktop with no window focused.
  So an Open that belongs to the desktop and to document windows, but not to
  the field where the reader is typing a value, is
  `{.contexts = {"document"}, .outside_contexts = true}`. A modal scope admits
  a command only through a context it names; being usable outside contexts
  says nothing about the modal. `CommandRegistry::set_command_scope` restates
  a declared command's scope. `CommandRegistry::withdraw`
  removes the command's metadata, handler, enablement predicate, and key
  bindings together so stale menu/status references become inert.
  Menu-bar dropdowns and context menus preserve the invoking focus ancestry
  while they are open. Moving focus into temporary menu UI therefore does not
  grey or block commands that belong to the document, board, or editor from
  which the menu was opened; choosing a command still restores that focus
  before its handler runs. A command palette presented in a window of its own
  does the same when it is told the invoking contexts
  (`CommandPalette::set_invocation_contexts`, with
  `ui::command_context_path` of the view the reader was on), and the
  palette the `command_palette` command puts up is told them as it opens.
  `CommandRegistry::in_scope` answers the scope half of availability on
  its own, enablement aside: it is how the palette lists a command that
  applies where the reader is but is disabled right now, greyed, instead of
  hiding it.
- **`close`/`quit`'s default handlers** (M9/WP-15) are installed by
  `Desktop::on_attached()` — but only if nothing has claimed the
  command yet (`CommandRegistry::has_handler`), the same guarded pattern
  `menu`'s own default follows. `close` closes the desktop's active
  window (vetoable, `Window::close_request`); `quit` sweeps every
  window through that same vetoable protocol, front-to-back in z-order (the
  window in front first, whatever order the windows were opened in), and only
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
