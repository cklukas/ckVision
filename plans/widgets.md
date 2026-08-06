# ckVision — Widget and Chrome Catalog

The binding inventory for ROADMAP M5/M6. "Baseline" is the classic
desktop-framework capability floor — everything a first-class 1990s
windowed TUI offered. "Beyond" is where ckVision must exceed it. Every
widget: keyboard-complete, mouse-complete, theme-role documented, golden
appearance tests, scripted interaction tests, reference docs.

## Desktop and window chrome (M5)

| Element | Baseline | Beyond baseline |
|---|---|---|
| Desktop | Background fill, owns z-ordered windows, tile/cascade, window cycling, select-by-number, window-list dialog | Background drawing hook; typed snapshot/restore of layout (geometry, z-order, zoom) |
| Window | Frame (active/inactive styles), title, close control, zoom/restore, move by title-bar drag or keyboard mode, resize by grip/edges or keyboard mode, min/max size limits, **shadow**, vetoable close request on every close path | Backing store (drag never repaints content); double/single/rounded frame sets; optional maximize-to-desktop |
| Dialog | Modal and modeless, default button (Enter), Esc-cancel, accept-time validation with veto (a failing control blocks accept and takes focus), correct initial focus by construction, focus restore on close | Declarative descriptor construction (labels auto-bind mnemonics to controls); resizable dialogs with layout |
| Message boxes | Info/warning/error/confirm with standard button sets | Localizable via string table; theme-role styled severity accents |

## Menu system (M5)

| Element | Baseline | Beyond baseline |
|---|---|---|
| Menu bar | F10/Alt activation, mnemonic letters, arrow navigation, drop-down on Enter/Down/hover-while-open, disabled items | Overflow behavior on narrow terminals |
| Drop-down menu | **Shadow**, separators, nested submenus, right-aligned accelerator hints, checkable items, disabled items, mnemonic jump, Esc closes one level | Commands-driven: items render title/chord/enablement live from the command registry |
| Context menu | Positional pop-up with same feature set | Keyboard invocation (Menu key / Shift-F10) at focus location |
| Status line | Context-sensitive hints bound to focus, clickable items, command chords displayed | Priority-based item layout on narrow widths |

Menu behavior contract (excerpt, pinned by event scripts): bar activation
never steals unsaved state; navigation wraps; disabled items are skipped
by arrows but visible; accelerators fire without opening menus; every
menu action is reachable by keyboard alone and by mouse alone.

## Controls (M4 minimal set; M6 completion)

The M4 minimal set is Label, Static text, Button, and Input line (core
editing); everything else lands in M6. Input line and Combo box histories
live in the application-scoped history registry — shared by key across
widgets and dialogs, never per-widget state.

| Widget | Baseline | Beyond baseline |
|---|---|---|
| Label | Mnemonic that focuses its buddy control | — |
| Static text | Styled, wrapped | Alignment options |
| Button | Default/normal, mnemonic, pressed feedback | Repeat-on-hold option |
| Input line | Editing keys, selection, clipboard, overwrite/insert, scrolling content, validators, input masks, password echo, history dropdown | Grapheme-correct editing (never splits clusters); undo within field |
| Memo | Multi-line editing, scrolling, selection, clipboard, bounded undo | Word wrap toggle; grapheme-correct navigation |
| Check group | Multi-select, arrow navigation, mnemonics | Tri-state option |
| Radio group | Exclusive select, arrow navigation, mnemonics | — |
| List view | Scrolling, single/multi select, keyboard search, double-click activate | Virtualized model interface (millions of rows), incremental search, column-less and columned modes |
| Combo box | — (the classics lacked a true one) | Editable and pick-only variants, history integration, dropdown with shadow |
| Scrollbar | Proportional thumb, arrows, paging, drag | Auto-hide policy option |
| Scroll viewport | Scrolls any content view, keyboard and wheel | Smooth cell-line scrolling via scroll-region diffs |
| Tab control | — | Top tabs with mnemonics, keyboard cycling, per-tab pages, overflow |
| Table | — | Columns with headers, resize, sort indicators, virtualized rows, cell styling hook |
| Tree / outline | Expand/collapse, keyboard navigation | Virtualized model, connector-line styles |
| Progress | — | Determinate/indeterminate, label slot |
| Splitter | — | Keyboard-adjustable split panes |
| Text view | Read-only styled text, scrolling | Links (activatable; rendered as OSC 8 hyperlinks where supported), simple inline styles; the building block for help browsers and about boxes |
| Help viewer | Context topics (F1 from any view via help-context keys), activatable cross-links, back navigation | Index/keyword lookup; content via an injected provider — the library defines no help file format |
| Image view | — | Shows an `Image` scaled to its cell box; mandatory fallback content; dual-space (cell + pixel) mouse events where the terminal reports pixels |
| Canvas | — | Application draws RGBA at the widget's current pixel size (cell box × cell metrics, redraw on resize); mandatory fallback painter; dual-space mouse events |

## Standard dialogs (M6)

| Dialog | Baseline | Beyond baseline |
|---|---|---|
| File open/save | Path input with completion, file list, directory navigation, filters | Recent locations via the history registry, hidden-file toggle, cross-platform path semantics; enumerates through the injected filesystem (headless-testable) |
| Directory picker | Tree-based selection | — |
| Window list | Select/activate/close from list | Type-ahead filter |
| Theme editor | — | Enumerates theme roles from the flat tables, edits styles with live preview, produces a theme value the application persists (M8) |

## Explicitly not widgets

- A code/document **editor**: applications own their editors; the library
  provides Memo (forms) and Text view (display) only.
- A terminal emulator widget: out of scope for 0.1; revisit on demand.

## Cross-cutting acceptance (all widgets)

- Operable entirely by keyboard and entirely by mouse.
- Correct under every built-in scheme including Mono/16-color.
- Correct with wide/combining/emoji graphemes in content and labels.
- Correct under terminal shrink to and below the minimum supported size
  (deterministic clamping, never corrupted output).
- Golden-tested in active, inactive, focused, disabled states.
- Raster-bearing widgets additionally: correct occlusion and scrolling,
  fallback rendering verified under a no-graphics capability profile,
  dual-space mouse verified under a pixel-mouse profile.
- Documented behavior contract: keys, mouse, notifications, theme roles.
