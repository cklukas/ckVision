# ckVision Standard Commands, v1

`ckv::ui::std_commands` (`include/cvision/ui/command.hpp`,
`src/ui/command.cpp`) is the library's own reserved command set —
`CommandId`s below `kFirstApplicationCommandId` (4096), materializing
D-013's "ids below 4096 are reserved for the library's standard
commands" and registered once, automatically, by every
`ui::Application`'s constructor (D-029). An application never
registers into this range itself — `CommandRegistry::register_command`/
`add` `CKV_ASSERT` if asked to — it attaches its own handler to
whichever of these ids its widgets actually need, via
`CommandRegistry::set_handler` or the `Application::set_command_handler`
convenience forward.

## Why a reserved range at all

Before this landed, every example independently defined its own
"quit," "activate the menu," "tile windows" commands — same concept,
three different ids, three different (and sometimes inconsistent)
chord/title choices. A shared, framework-owned id for each of these
lets:

- an application skip re-declaring metadata for a concept the
  framework already models (`examples/gallery` needs zero application-
  space command ids of its own — every command it uses is standard);
- library-internal machinery (the default keymap, WP-13; the modality
  stack's close-request sweep, WP-15) bind default behavior to a
  stable id without needing an application to have defined it first;
- a menu or status-line item reference the same id everywhere it
  appears and render consistent title/chord/enablement (M9/WP-11) no
  matter which application built it.

An application that needs different wording for the same concept —
matching an external reference example's own vocabulary, the way
`examples/hello` keeps "Exit" (not the standard "Quit") to match Turbo
Vision's own `hello.cpp` tutorial — keeps its own command in the
application range instead of referencing the standard one. That is a
legitimate, expected choice, not a workaround: the reserved range's
titles are fixed (only `register_standard_commands()` can populate
that range), precisely so that referencing a standard id always means
the same thing everywhere.

## The set

Generated from the actual registration
(`tools/docgen/generate_standard_commands_table.cpp`, run by hand —
regeneration is a manual, reviewed act, exactly like a golden-fixture
update, never automatic):

| Id | Title | Category | Default chord |
|---|---|---|---|
| 1 | `&Quit` | System | Alt+X |
| 2 | `&Close` | Window | Alt+F3 |
| 3 | `&Zoom` | Window | F5 |
| 4 | `&Next` | Window | F6 |
| 5 | `&Previous` | Window | Shift+F6 |
| 6 | `&Tile` | Window | — |
| 7 | `C&ascade` | Window | — |
| 8 | `&Window List` | Window | — |
| 9 | `Menu` | Window | F10 |
| 10 | `Help` | System | F1 |

The `CommandId` constants themselves
(`ckv::ui::std_commands::kQuit`, `kClose`, `kZoom`, `kNextWindow`,
`kPreviousWindow`, `kTile`, `kCascade`, `kWindowList`, `kMenu`,
`kHelp`, in that id order) are what application code actually
references — this table exists to make the wording, grouping, and
chord choices reviewable in one place, not as a numeric contract an
application should hard-code.

## Design notes

- **`&`-mnemonics are deliberately included** on every title except
  `kMenu` and `kHelp`. Those two are invoked by their chord directly
  (F10, F1) and are not typically menu ITEMS themselves — an
  application that does place one in a menu of its own can still
  `parse_mnemonic()` over whatever text it wants there (see the
  "keep your own command" escape hatch above). `kCascade` uses `C&ascade`
  (mnemonic on 'a') rather than 'c', since `kClose` and `kCascade` can
  plausibly coexist in the same Window menu.
- **`kTile`/`kCascade`/`kWindowList` have no default chord.** There is
  no comparably strong, widely-recognized single-key convention for
  them the way there is for Quit/Close/Zoom/Help/Menu/window-cycling —
  an application binds one itself (`CommandRegistry::bind_key`) if it
  wants one.
- **The chord scheme is this project's own choice**, authored for
  M9/WP-12 with no prior source consulted, per this repository's own
  provenance rule (`AGENTS.md`) — informed by publicly documented,
  widely-known DOS-era TUI conventions (F1 help, F10 menu, F6 window
  cycling), not derived from any specific prior implementation's
  source code.
- **`kHelp`'s default handler** resolves the focused view's nearest
  help-context key (`View::resolve_help_context_key`, D-027) and hands
  it to whatever `Application::set_help_provider` installed — a no-op
  if no provider is set or no key resolves anywhere in the focus
  chain. F1 therefore always reports as "handled" once unconsumed by
  the focus chain (the command ran), regardless of whether the
  provider itself found anything to show; that visible-effect nuance
  is observable through the provider callback, not through
  `Application::dispatch`'s return value — the same way every other
  standard chord already works.
