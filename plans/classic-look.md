<!-- Copyright (c) 2026 C. Klukas. All rights reserved. -->

# Classic look — specification of the visual baseline

This is the written specification of ckVision's **Classic** scheme as ckVision
draws it and its goldens pin it: frame glyphs, window controls, junctions,
shadows, the desktop, menus, the status line, dialog controls, scrollbars,
focus cues and the palette. It is the M5 deliverable named in
[ROADMAP.md](../ROADMAP.md) and the document
the M5 exit review signs off against. Every statement is measurable in a
golden, and the numbered [checklist](#checklist) at the end names, for
each item, the golden files and cells that show it.

## Authority and provenance

This specification describes ckVision's own design. Its sources are
ckVision's own documents and artifacts only: [ARCHITECTURE.md](../ARCHITECTURE.md)
§3 and §5, [DECISIONS.md](../DECISIONS.md) (D-036 junction scope, D-037 shadow
coverage, D-076 disabled state), the theme construction in
`src/ui/standard_roles.cpp`, the drawing code in `src/widgets/window.cpp`,
`menu.cpp`, `desktop.cpp`, `status_line.cpp`, `button.cpp`, `input_line.cpp`
and `scrollbar.cpp`, `include/cvision/scene/box_drawing.hpp`, and the pinned
goldens under `tests/golden/`. Where it names a convention of classic
text-mode desktops (a patterned desktop, a grey menu bar, double-line active
frames, drop shadows two columns right and one row down), it names a publicly
documented, generally observed convention, as ARCHITECTURE §3 already does.
No other product's source code was read, quoted or used for any value here,
per [AGENTS.md](../AGENTS.md); nothing in this document claims fidelity to any
other product.

The glyphs are Unicode characters (Box Drawing U+2500–257F, Block Elements
U+2580–259F, Geometric Shapes U+25A0–25FF, Arrows U+2190–21FF), named by code
point where it matters.

## How to read this specification

- A **golden** is a dump in the version-1 format of
  [docs/golden-format.md](../docs/golden-format.md). Paths are relative to
  `tests/golden/`. The pinned Gallery frames are `gallery_no_graphics.dump`
  and `gallery_sixel.dump`; the hello frames are `hello_*.dump`; the
  appearance matrix's Classic frames are `appearance/<Element>/<state>.classic.dump`.
- **Coordinates** are 0-based cells: `row 6 col 20` is the seventh row, the
  twenty-first column. Window-relative positions are written *local*, with
  local column 0 at the window's left frame column and `w` the window's width.
- A **style** is named from the [style legend](#style-legend); each legend row
  spells the style exactly as a dump's `styles` line does.
- The conformance gate `classic_look` (`tests/check_classic_look.py`, CTest
  labels `golden;script`) reads the `classic-check` blocks of the checklist out
  of this file and verifies every cited glyph and style in the named goldens,
  and it regenerates the [palette](#palette) with
  `tools/docgen/generate_classic_palette_table` and fails when the table below
  differs. `classic_look_self_test` proves the gate notices a mutated glyph, a
  mutated style, a drifted palette and a malformed checklist, and that every
  item of this checklist fails once the first cell it cites changes.
- A change to the Classic look is a change to this document first: update the
  prose and the checks, regenerate the goldens and the palette table, and have
  the owner sign again.

## Colours

Classic draws with twelve RGB colours, each stated as ckVision's own value.
Light gray (200 per channel) and gray (170) are distinct: gray is the colour of
disabled text drawn on light-gray surfaces.

<!-- classic-look:colours:begin -->
| Name | RGB | Where Classic uses it |
|---|---|---|
| black | `#000000` | Text on grey, green, cyan and white surfaces |
| blue | `#0000AA` | Document windows, input fields, the desktop pattern, scrollbars |
| green | `#00AA00` | Buttons, highlight bars, window controls |
| cyan | `#00AAAA` | Check and radio groups, marked calendar days, grid selection |
| red | `#AA0000` | Hotkey accents, invalid fields, error messages |
| gray | `#AAAAAA` | Disabled menu and status text, disabled fields, editor gutter |
| light gray | `#C8C8C8` | The desktop ground, dialogs, the menu bar, dropdowns, the status line |
| dark gray | `#555555` | Disabled labels, buttons, options, lists and memos |
| light cyan | `#55FFFF` | Default-button text, the focused field value, scrollbars |
| yellow | `#FFFF55` | Label and control mnemonics, help text, tooltips |
| white | `#FFFFFF` | Frames, titles, focused buttons, list and memo surfaces |
| orchid | `#DC82D2` | Editor syntax numbers only |
<!-- classic-look:colours:end -->

## Style legend

The styles the checklist cites, as they appear in the goldens. Most are a
role of the [palette](#palette) verbatim; the others are named composites:
a window control's foreground on its frame's background, a mnemonic accent's
foreground on the surface it sits on, and the dimmed cells under a shadow.

<!-- classic-look:styles:begin -->
| Style | Foreground | Background | Attributes | What it is |
|---|---|---|---|---|
| `desktop` | `#0000AA` | `#C8C8C8` | `-` | `ckv.desktop.background` |
| `doc-frame` | `#FFFFFF` | `#0000AA` | `bold` | `ckv.window.frame.active` and `ckv.window.title.active` |
| `doc-frame-inactive` | `#C8C8C8` | `#0000AA` | `-` | `ckv.window.frame.inactive` and `ckv.window.title.inactive` |
| `doc-control` | `#00AA00` | `#0000AA` | `bold` | `ckv.window.control` foreground on the active document frame |
| `control-pressed` | `#0000AA` | `#00AA00` | `bold` | `ckv.window.control.pressed` |
| `dlg-frame` | `#FFFFFF` | `#C8C8C8` | `-` | `ckv.dialog.frame`: an active dialog's frame, title and interior |
| `dlg-frame-inactive` | `#000000` | `#C8C8C8` | `-` | `ckv.dialog.background`: an inactive dialog's frame, title and interior |
| `dlg-control` | `#00AA00` | `#C8C8C8` | `bold` | `ckv.window.control` foreground on the active dialog frame |
| `shadow-on-desktop` | `#000055` | `#646464` | `-` | `desktop` under a shadow |
| `shadow-on-doc-frame` | `#7F7F7F` | `#000055` | `bold` | `doc-frame` under a shadow |
| `shadow-on-doc-frame-inactive` | `#646464` | `#000055` | `-` | `doc-frame-inactive` under a shadow |
| `shadow-on-doc-control` | `#005500` | `#000055` | `bold` | `doc-control` under a shadow |
| `menu` | `#000000` | `#C8C8C8` | `-` | `ckv.menu.bar.normal`, `ckv.menu.dropdown.normal` |
| `menu-hotkey` | `#AA0000` | `#C8C8C8` | `-` | `ckv.hotkey` on the menu surface |
| `menu-active` | `#000000` | `#00AA00` | `-` | `ckv.menu.bar.active`, `ckv.menu.dropdown.highlighted` |
| `menu-active-hotkey` | `#AA0000` | `#00AA00` | `-` | `ckv.hotkey` foreground on the highlight bar |
| `menu-disabled` | `#AAAAAA` | `#C8C8C8` | `dim` | `ckv.menu.dropdown.disabled` |
| `menu-disabled-hotkey` | `#AA0000` | `#C8C8C8` | `dim` | `ckv.hotkey` foreground on a disabled item |
| `menu-active-disabled` | `#AAAAAA` | `#00AA00` | `dim` | a disabled item under the highlight bar |
| `menu-active-disabled-hotkey` | `#AA0000` | `#00AA00` | `dim` | its mnemonic |
| `status` | `#000000` | `#C8C8C8` | `-` | `ckv.statusline.normal` |
| `status-hotkey` | `#AA0000` | `#C8C8C8` | `-` | `ckv.hotkey` on the status line |
| `status-pressed` | `#000000` | `#00AA00` | `-` | `ckv.statusline.selected` |
| `status-pressed-hotkey` | `#AA0000` | `#00AA00` | `-` | `ckv.statusline.selected.hotkey` |
| `status-disabled` | `#AAAAAA` | `#C8C8C8` | `dim` | `ckv.statusline.disabled` |
| `label` | `#000000` | `#C8C8C8` | `-` | `ckv.label.text` |
| `label-mnemonic` | `#FFFF55` | `#C8C8C8` | `-` | `ckv.label.mnemonic` |
| `caption-focused` | `#FFFFFF` | `#C8C8C8` | `-` | a focused option group's caption: `ckv.option.focused` foreground on the dialog |
| `button` | `#000000` | `#00AA00` | `-` | `ckv.button.normal` |
| `button-focused` | `#FFFFFF` | `#00AA00` | `-` | `ckv.button.focused`, `ckv.button.hovered` |
| `button-default` | `#55FFFF` | `#00AA00` | `-` | `ckv.button.default` |
| `button-mnemonic` | `#FFFF55` | `#00AA00` | `-` | `ckv.label.mnemonic` foreground on a button face |
| `button-disabled` | `#555555` | `#00AA00` | `-` | `ckv.button.disabled` |
| `button-shadow` | `#000000` | `#C8C8C8` | `-` | `ckv.button.shadow` |
| `input` | `#FFFFFF` | `#0000AA` | `-` | `ckv.input.normal` |
| `input-focused` | `#55FFFF` | `#0000AA` | `-` | `ckv.input.focused` |
| `input-caret` | `#55FFFF` | `#0000AA` | `reverse` | the caret and selected text of a focused field |
| `input-invalid` | `#FFFFFF` | `#AA0000` | `bold` | `ckv.input.invalid` |
| `input-disabled` | `#AAAAAA` | `#0000AA` | `-` | `ckv.input.disabled` |
| `option` | `#000000` | `#00AAAA` | `-` | `ckv.option.normal` |
| `option-focused` | `#FFFFFF` | `#00AAAA` | `-` | `ckv.option.focused` |
| `option-mnemonic` | `#FFFF55` | `#00AAAA` | `-` | `ckv.label.mnemonic` foreground on an option row |
| `list` | `#000000` | `#FFFFFF` | `-` | `ckv.list.normal` |
| `list-cursor` | `#000000` | `#00AA00` | `-` | `ckv.list.selected` |
| `list-cursor-inactive` | `#000000` | `#C8C8C8` | `-` | `ckv.list.selected.inactive` |
| `scroll` | `#55FFFF` | `#0000AA` | `-` | `ckv.scrollbar.track`, `ckv.scrollbar.thumb` |
<!-- classic-look:styles:end -->

## Desktop

The desktop is a light-gray ground carrying a blue texture: every desktop cell
holds U+2591 LIGHT SHADE `░` in `desktop` (blue on light gray). The pattern is
uniform, with no border and no gradient. Docked chrome (the menu bar and the
status line), windows and popups paint over it; shadows dim it (see
[Shadows](#shadows)).

## Window frames

### Frame families

Two families of window chrome share one geometry and differ in colour:

| Family | Used by | Active frame, title and interior | Inactive frame, title and interior |
|---|---|---|---|
| Document | `Window` by default | `doc-frame` (bold white on blue) | `doc-frame-inactive` (light gray on blue) |
| Dialog | Message boxes, descriptor dialogs, file dialogs, and any window given `set_role_override(dialog_frame, dialog_background, dialog_frame, dialog_background)` (the Gallery's Controls window) | `dlg-frame` (white on light gray) | `dlg-frame-inactive` (black on light gray) |

The frame style also fills the whole interior, so a window's unpainted
content cells carry its surface colour.

### Glyph sets

| Part | Active window (double) | Inactive window (single) |
|---|---|---|
| Top-left, top-right | `╔` U+2554, `╗` U+2557 | `┌` U+250C, `┐` U+2510 |
| Bottom-left, bottom-right | `╚` U+255A, `╝` U+255D | `└` U+2514, `┘` U+2518 |
| Horizontal, vertical | `═` U+2550, `║` U+2551 | `─` U+2500, `│` U+2502 |

The line weight, not only the colour, marks the active window, so the
distinction survives a monochrome terminal. Rounded and heavy frames exist in
the scene layer but no window draws them in Classic.

### Title

The title is centred on the top border with one padding cell each side, the
padding in the frame style and the text in the title style. It is centred on
the whole frame width, `start = (w - title_width) / 2` (local), shifted only as
far as needed to keep the padded title strictly between the close control and
the right-hand controls: the text may start at local column 6 at the earliest
and must end by local column `w - 10` when the window draws the minimize
control, `w - 7` when it draws only zoom, and `w - 3` when it draws neither. A
title that does not fit is elided with `…` U+2026. Titles are drawn only when
`w > 8`.

### Controls

All controls sit on the top border, each as three cells: `[` glyph `]`.

| Control | Glyph | Local columns | Drawn when |
|---|---|---|---|
| Close | `■` U+25A0 | 2–4 (one frame cell after the corner) | `w > 6` |
| Minimize | `_` U+005F | `w - 8` to `w - 6` | resizable, minimizable and `w >= 19` |
| Zoom | `↑` U+2191; `↕` U+2195 while zoomed | `w - 5` to `w - 3` (one frame cell before the corner) | resizable and `w > 8` |

On the **active** window the brackets are in the frame style and the glyph is
in `ckv.window.control`'s foreground and attributes on the frame's background:
`doc-control` (bold green on blue) or `dlg-control` (bold green on light
gray). On an **inactive** window the whole control, glyph included, is in the
inactive frame style: only the window the reader works in shows live
controls. A control held down with the pointer inside it draws all three cells
in `control-pressed` (bold blue on green). A fixed-size window draws the close
control only.

### Resize grip and footer

An active, resizable window marks its bottom-right corner as a grip: the
corner becomes the single-line `┘` and the cell to its left `─`, both in the
control style (`doc-control` on a document window). The grip is two cells
wide when `w >= 5` and one cell otherwise. While a resize drag is under way
the other three corners show the same single-line elbows (`┌─`, `─┐`, `└─`),
drawn in the frame style, not the control style. Inactive and fixed-size
windows show no grip: their corners are the ordinary corner glyph of their
line set.

A window footer is drawn left-aligned on the bottom border: a padding space at
local column 2, the text from local column 3 in the title style, and a padding
space after it; it is elided to `w - 8` cells on a resizable window and `w - 4`
on a fixed one.

## Junction rules

Junction merging follows D-036. Lines drawn by the same view in one paint
merge by connector direction into tees and crosses of the same line style:
a dropdown's separator runs the full popup width and turns the side borders
into `├` U+251C and `┤` U+2524. Lines of different views never merge: a
window drawn over another window's border replaces the covered cells with its
own glyphs, so an upper window's `║` stays `║` where a lower window's `─`
runs into it, and a submenu's `┌` corner replaces its parent dropdown's `│`
rather than forming a junction. The merge is decided by the stored connector
metadata of each cell and its paint scope, never by reading back the visible
glyph.

## Shadows

Shadow coverage follows ARCHITECTURE §3 and D-037.

- **Geometry.** A shadow-casting layer occupying columns `x..x+w-1` and rows
  `y..y+h-1` casts an L-shaped footprint of two non-overlapping rectangles
  (`scene::ShadowSpec{dx = 2, dy = 1}`): a right strip two columns wide,
  columns `x+w..x+w+1`, rows `y+1..y+h`; and a bottom strip one row tall,
  row `y+h`, columns `x+2..x+w+1`. The layer leaves the cells at `(x+w, y)`,
  `(x+w+1, y)`, `(x, y+h)` and `(x+1, y+h)` unshadowed.
- **Transform.** A shadowed cell keeps its glyph and its attributes; its
  foreground and background each have every RGB channel halved, rounding
  down (`scene::default_dim`). So the desktop's blue-on-light-gray becomes
  `#000055` on `#646464`, and a bold white-on-blue frame cell becomes bold
  `#7F7F7F` on `#000055`. Pixels of a raster under a shadow are halved by the
  presenter in the same way.
- **Casters.** Windows (document and dialog families) and dropdown menus,
  including submenus and context menus, cast shadows. A button's shadow is not
  a compositor shadow but glyphs of its own (see [Buttons](#buttons)).
- **What a shadow falls on.** Everything beneath the caster inside the
  desktop's content area: the desktop pattern and lower windows' frames,
  controls, titles and content. A later-painted, higher layer
  replaces shadowed cells normally, so a shadow never dims a window above its
  caster.
- **What it never falls on.** Docked chrome: the menu bar and the status line
  are outside the content area, so the footprint is clipped there, and a
  zoomed window, which fills the content area, shows no shadow at all.
- **Binary union.** A cell covered by several footprints, from one caster or
  several, is dimmed exactly once.

## Menu bar

- The menu bar is row 0, filled in `menu` (black on light gray).
- The first title starts at column 2, and consecutive titles are separated by
  two blank columns. Titles carry no brackets or rules.
- A title's mnemonic letter is in `menu-hotkey` (red); the rest is `menu`.
- While the bar holds focus, the highlighted title and one padding cell each
  side of it are in `menu-active` (black on green), the mnemonic in
  `menu-active-hotkey`; the other titles keep their normal styles.

## Dropdown menus

- **Placement.** A dropdown opens on the row below the bar with its left frame
  one column left of its title's first letter (the title's highlight padding
  cell), so the item text starts one column right of the title's first letter.
  It is clamped into the desktop.
- **Frame.** Single-line box glyphs in `menu`; the popup is exactly as wide as
  its widest item plus four cells (frame and one padding cell each side) and
  exactly as tall as its items plus two.
- **Items.** The label starts two cells right of the left frame (one padding
  cell). The item's mnemonic letter is `menu-hotkey`.
- **Highlight.** The highlighted item's whole interior row, padding included,
  is `menu-active` (black on green) with its mnemonic in `menu-active-hotkey`.
- **Accelerator column.** Chord hints are right-aligned so that every hint and
  every submenu arrow ends in the same column, the one before the right
  padding cell. A hint is separated from the widest label by at least two
  cells.
- **Submenu arrow.** An item that opens a submenu shows `►` U+25BA in the
  accelerator column, in the item's style.
- **Submenus.** A submenu's left frame lies on its parent's right frame
  column and its top frame on the parent item's row; the parent keeps its
  highlight on that item, and the submenu highlights its first item.
- **Check marks.** When any item carries a mark, every label shifts two cells
  right and the mark sits in the first interior cell after the padding: `x`
  for a checked switch, `•` U+2022 for the chosen radio item, and a blank for
  an unchecked switch or an unchosen radio item.
- **Separators.** A separator row is a single-line rule across the full popup
  width that joins the side frames as `├ … ┤` (see [Junction rules](#junction-rules)).
- **Disabled items.** A disabled item draws in `menu-disabled` (gray, dim) and
  its mnemonic in `menu-disabled-hotkey`; under the highlight it keeps the
  green bar in `menu-active-disabled`, with its mnemonic in
  `menu-active-disabled-hotkey`, so the reader sees where the highlight is.
- **Shadow.** Every dropdown casts the standard shadow.

## Status line

- The status line is the last row, filled in `status` (black on light gray),
  and is never shadowed.
- Items start at column 1. Each item is its chord followed by a space and its
  label; the chord is `status-hotkey` (red), the label `status`. Items are
  separated by two blank cells and no rule.
- A pressed item, with one padding cell each side, is `status-pressed` (black
  on green); its chord stays accented in `status-pressed-hotkey`.
- An unavailable item draws chord and label alike in `status-disabled` (gray,
  dim), with no chord accent.
- A hint follows the items: one cell after the last item's trailing padding
  cell comes the divider `│` U+2502 and a space, then the hint text, all in
  `status`.

## Buttons

A standard (not flat) button is two rows tall and `w` columns wide:

- **Face row.** Column 0 is a spacer in `button-shadow` (black on light gray);
  columns 1 to `w - 2` are the face; column `w - 1` is the shadow's end,
  `▄` U+2584 in `button-shadow`. The label is centred on the full width,
  `(w - label_width) / 2`, and its mnemonic is `button-mnemonic` (yellow).
- **Shadow row.** Two spacer cells, then `▀` U+2580 from column 2 to `w - 1`,
  all in `button-shadow`: the shadow falls one cell right and one row down.
- **Face styles.** `button` (black on green) at rest; `button-focused` (white
  on green) while focused or hovered; `button-default` (light cyan on green)
  marks the dialog's default button while it is not focused; a focused
  default button looks like any focused button.
- **Pressed.** The face moves one cell right (columns 2 to `w - 1`), the label
  with it, and both shadow glyph runs disappear; the vacated cells are
  `button-shadow` spacers.
- **Disabled.** `button-disabled` (dark gray on green), no mnemonic accent; the
  shadow glyphs stay.

## Input lines

- The field is one row of `input` (white on blue), text from the field's first
  column, with no padding cell or brackets.
- Focused, the value is `input-focused` (light cyan on blue); the caret cell is
  the same style in reverse video (`input-caret`), and selected text is
  reverse video as well. A field that gains focus with text offers it all
  selected, caret at the end.
- An invalid field (failed validator) is `input-invalid` (bold white on red);
  a disabled field is `input-disabled` (gray on blue) with no caret or
  selection; a password field echoes `*` per character.
- A label in front of a field is `label` with its mnemonic in
  `label-mnemonic` (yellow).

## Scrollbars

- Vertical: `▲` U+25B2 in the first cell, `▼` U+25BC in the last. Horizontal:
  `◄` U+25C4 first, `►` U+25BA last.
- The track between them is blank cells; its colour alone marks it.
- The thumb is `█` U+2588 where a cell is wholly thumb, and a half block
  where half of it is: `▀` U+2580 or `▄` U+2584 vertically, `▌` U+258C or
  `▐` U+2590 horizontally. A bar with nothing to scroll shows a thumb over the
  whole track.
- Every scrollbar cell is `scroll` (light cyan on blue).

## Focus cues

| What is active or focused | Cue |
|---|---|
| The active window | Its frame is double-line, its controls and grip are coloured; every other window is single-line with uncoloured controls |
| A menu bar | The highlighted title is `menu-active` |
| A button | Its face text is `button-focused` (white) |
| An input line | Its value is `input-focused` (light cyan) with a reverse-video caret cell |
| A list | The cursor row is a full-width `list-cursor` bar (green); unfocused, the row stays marked in `list-cursor-inactive` (light gray) |
| A check or radio group | The cursor row is `option-focused` (white on cyan) and the group caption is `caption-focused` (white) |

## Palette

Every standard role and the style Classic resolves it to, grouped by family.
This table is generated by `tools/docgen/generate_classic_palette_table`; the
`classic_look` gate fails when it differs from the program's output. Some
roles are dormant in the goldens above (a shadowed button, for instance,
shows its press through geometry, so `ckv.button.pressed` colours flat
buttons only).

<!-- classic-look:palette:begin -->
| Family | Role | Foreground | Background | Attributes |
|---|---|---|---|---|
| desktop | `ckv.desktop.background` | `#0000AA` | `#C8C8C8` | `-` |
| dialog | `ckv.dialog.frame` | `#FFFFFF` | `#C8C8C8` | `-` |
| dialog | `ckv.dialog.background` | `#000000` | `#C8C8C8` | `-` |
| label | `ckv.label.text` | `#000000` | `#C8C8C8` | `-` |
| label | `ckv.label.mnemonic` | `#FFFF55` | `#C8C8C8` | `-` |
| label | `ckv.label.disabled` | `#555555` | `#C8C8C8` | `-` |
| hotkey | `ckv.hotkey` | `#AA0000` | `#C8C8C8` | `-` |
| static | `ckv.static.text` | `#000000` | `#C8C8C8` | `-` |
| help | `ckv.help.text` | `#FFFF55` | `#0000AA` | `-` |
| button | `ckv.button.normal` | `#000000` | `#00AA00` | `-` |
| button | `ckv.button.focused` | `#FFFFFF` | `#00AA00` | `-` |
| button | `ckv.button.hovered` | `#FFFFFF` | `#00AA00` | `-` |
| button | `ckv.button.default` | `#55FFFF` | `#00AA00` | `-` |
| button | `ckv.button.shadow` | `#000000` | `#C8C8C8` | `-` |
| button | `ckv.button.pressed` | `#000000` | `#C8C8C8` | `-` |
| button | `ckv.button.disabled` | `#555555` | `#00AA00` | `-` |
| input | `ckv.input.normal` | `#FFFFFF` | `#0000AA` | `-` |
| input | `ckv.input.focused` | `#55FFFF` | `#0000AA` | `-` |
| input | `ckv.input.invalid` | `#FFFFFF` | `#AA0000` | `bold` |
| input | `ckv.input.disabled` | `#AAAAAA` | `#0000AA` | `-` |
| message | `ckv.message.info.text` | `#000000` | `#C8C8C8` | `-` |
| message | `ckv.message.warning.text` | `#000000` | `#C8C8C8` | `bold` |
| message | `ckv.message.error.text` | `#FFFFFF` | `#AA0000` | `bold` |
| message | `ckv.message.confirm.text` | `#000000` | `#C8C8C8` | `-` |
| window | `ckv.window.frame.active` | `#FFFFFF` | `#0000AA` | `bold` |
| window | `ckv.window.frame.inactive` | `#C8C8C8` | `#0000AA` | `-` |
| window | `ckv.window.title.active` | `#FFFFFF` | `#0000AA` | `bold` |
| window | `ckv.window.title.inactive` | `#C8C8C8` | `#0000AA` | `-` |
| window | `ckv.window.control` | `#00AA00` | `#0000AA` | `bold` |
| window | `ckv.window.control.pressed` | `#0000AA` | `#00AA00` | `bold` |
| window | `ckv.window.frame.moving` | `#00AA00` | `#0000AA` | `reverse` |
| calendar | `ckv.calendar.today` | `#FFFF55` | `#0000AA` | `bold` |
| calendar | `ckv.calendar.marked` | `#000000` | `#00AAAA` | `-` |
| menu | `ckv.menu.bar.normal` | `#000000` | `#C8C8C8` | `-` |
| menu | `ckv.menu.bar.active` | `#000000` | `#00AA00` | `-` |
| menu | `ckv.menu.dropdown.normal` | `#000000` | `#C8C8C8` | `-` |
| menu | `ckv.menu.dropdown.highlighted` | `#000000` | `#00AA00` | `-` |
| menu | `ckv.menu.dropdown.disabled` | `#AAAAAA` | `#C8C8C8` | `dim` |
| list | `ckv.list.normal` | `#000000` | `#FFFFFF` | `-` |
| list | `ckv.list.selected` | `#000000` | `#00AA00` | `-` |
| list | `ckv.list.selected.inactive` | `#000000` | `#C8C8C8` | `-` |
| list | `ckv.list.disabled` | `#555555` | `#FFFFFF` | `-` |
| table | `ckv.table.header` | `#FFFFFF` | `#C8C8C8` | `-` |
| cellgrid | `ckv.cellgrid.normal` | `#FFFF55` | `#0000AA` | `-` |
| cellgrid | `ckv.cellgrid.header` | `#0000AA` | `#C8C8C8` | `-` |
| cellgrid | `ckv.cellgrid.cursor` | `#FFFFFF` | `#00AA00` | `-` |
| cellgrid | `ckv.cellgrid.cursor.inactive` | `#000000` | `#C8C8C8` | `-` |
| cellgrid | `ckv.cellgrid.selection` | `#FFFFFF` | `#00AAAA` | `-` |
| memo | `ckv.memo.normal` | `#000000` | `#FFFFFF` | `-` |
| memo | `ckv.memo.focused` | `#000000` | `#FFFFFF` | `underline` |
| memo | `ckv.memo.invalid` | `#FFFFFF` | `#AA0000` | `bold` |
| memo | `ckv.memo.disabled` | `#555555` | `#FFFFFF` | `-` |
| option | `ckv.option.normal` | `#000000` | `#00AAAA` | `-` |
| option | `ckv.option.focused` | `#FFFFFF` | `#00AAAA` | `-` |
| option | `ckv.option.disabled` | `#555555` | `#00AAAA` | `-` |
| tooltip | `ckv.tooltip` | `#000000` | `#FFFF55` | `-` |
| scrollbar | `ckv.scrollbar.track` | `#55FFFF` | `#0000AA` | `-` |
| scrollbar | `ckv.scrollbar.thumb` | `#55FFFF` | `#0000AA` | `-` |
| image | `ckv.image.fallback` | `#000000` | `#C8C8C8` | `-` |
| canvas | `ckv.canvas.fallback` | `#000000` | `#C8C8C8` | `-` |
| textview | `ckv.textview.text` | `#000000` | `#C8C8C8` | `-` |
| flow | `ckv.flow.text` | `#000000` | `#C8C8C8` | `-` |
| statusline | `ckv.statusline.normal` | `#000000` | `#C8C8C8` | `-` |
| statusline | `ckv.statusline.disabled` | `#AAAAAA` | `#C8C8C8` | `dim` |
| statusline | `ckv.statusline.selected` | `#000000` | `#00AA00` | `-` |
| statusline | `ckv.statusline.selected.hotkey` | `#AA0000` | `#00AA00` | `-` |
| statusline | `ckv.statusline.selected.disabled` | `#AAAAAA` | `#00AA00` | `dim` |
| splitter | `ckv.splitter.normal` | `#FFFFFF` | `#C8C8C8` | `-` |
| splitter | `ckv.splitter.focused` | `#FFFFFF` | `#00AA00` | `-` |
| editor | `ckv.editor.text` | `#000000` | `#FFFFFF` | `-` |
| editor | `ckv.editor.gutter` | `#AAAAAA` | `#FFFFFF` | `-` |
| editor | `ckv.editor.selection` | `#FFFFFF` | `#0000AA` | `-` |
| editor | `ckv.editor.search` | `#FFFFFF` | `#AA0000` | `-` |
| editor | `ckv.editor.syntax.plain` | `#000000` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.keyword` | `#AA0000` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.type` | `#0000AA` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.property` | `#AA0000` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.string` | `#00AA00` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.number` | `#DC82D2` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.comment` | `#AAAAAA` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.command` | `#0000AA` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.operator` | `#000000` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.escape` | `#AA0000` | `#FFFFFF` | `-` |
| editor | `ckv.editor.syntax.error` | `#AA0000` | `#FFFFFF` | `bold` |
<!-- classic-look:palette:end -->

## Checklist

One checkable statement per item. Each `classic-check` line names a golden
(relative to `tests/golden/`) and either a run of cells, `row R col C "text"
styles`, or a rectangle, `rows R0-R1 cols C0-C1 ["glyph"] style`, with styles
from the [style legend](#style-legend) (`name*N` repeats a name). The
`classic_look` gate verifies every line.

<!-- classic-look:checklist:begin -->
1. **Desktop pattern.** Every desktop cell is `░` U+2591 in `desktop`, on an
   empty desktop and in the Gallery.

   ```classic-check
   appearance/Desktop/normal.classic.dump rows 0-11 cols 0-39 "░" desktop
   gallery_no_graphics.dump rows 1-1 cols 0-79 "░" desktop
   hello_initial.dump rows 1-22 cols 0-79 "░" desktop
   ```

2. **Active document frame.** An active document window is framed with the
   double-line set in `doc-frame`.

   ```classic-check
   appearance/Window/normal.classic.dump row 1 col 2 "╔═" doc-frame
   appearance/Window/normal.classic.dump row 1 col 35 "╗" doc-frame
   appearance/Window/normal.classic.dump rows 2-8 cols 2-2 "║" doc-frame
   appearance/Window/normal.classic.dump rows 2-8 cols 35-35 "║" doc-frame
   appearance/Window/normal.classic.dump row 9 col 2 "╚═" doc-frame
   gallery_no_graphics.dump row 6 col 20 "╔═" doc-frame
   gallery_no_graphics.dump rows 7-20 cols 20-20 "║" doc-frame
   gallery_no_graphics.dump row 21 col 20 "╚═" doc-frame
   ```

3. **Inactive document frame.** An inactive document window is framed with the
   single-line set in `doc-frame-inactive`, its controls included.

   ```classic-check
   appearance/Window/inactive.classic.dump row 1 col 2 "┌─[■]─" doc-frame-inactive
   appearance/Window/inactive.classic.dump row 1 col 28 "[_][↑]─┐" doc-frame-inactive
   appearance/Window/inactive.classic.dump rows 2-8 cols 2-2 "│" doc-frame-inactive
   appearance/Window/inactive.classic.dump row 9 col 2 "└" doc-frame-inactive
   ```

4. **Active dialog frame.** An active dialog-family window is double-line in
   `dlg-frame` with its close glyph in `dlg-control`.

   ```classic-check
   appearance/MessageBox/error.classic.dump row 2 col 7 "╔═[■]" dlg-frame*3,dlg-control,dlg-frame
   appearance/MessageBox/error.classic.dump row 2 col 31 "╗" dlg-frame
   appearance/MessageBox/error.classic.dump row 8 col 7 "╚═══════════════════════╝" dlg-frame
   hello_greeting.dump row 8 col 27 "╔═[■]" dlg-frame*3,dlg-control,dlg-frame
   ```

5. **Inactive dialog frame.** An inactive dialog-family window is single-line
   in `dlg-frame-inactive`, controls and title included (the Gallery's
   Controls window).

   ```classic-check
   gallery_no_graphics.dump row 2 col 2 "┌─[■]──────────" dlg-frame-inactive
   gallery_no_graphics.dump row 2 col 17 " Controls " dlg-frame-inactive
   gallery_no_graphics.dump row 2 col 27 "───────[_][↑]─┐" dlg-frame-inactive
   gallery_no_graphics.dump row 11 col 2 "└─────────────────" dlg-frame-inactive
   ```

6. **Title placement.** The title is centred on the frame with one padding
   cell each side in the title style, and elided with `…` when it does not fit
   between the controls.

   ```classic-check
   appearance/Window/normal.classic.dump row 1 col 15 " Report " doc-frame
   gallery_no_graphics.dump row 6 col 31 " Sixel Demo " doc-frame
   hello_greeting.dump row 8 col 32 " Hello, World! " dlg-frame
   appearance/Window/narrow.classic.dump row 1 col 7 " Quarterl… " doc-frame
   ```

7. **Close control.** The close control is `[■]` at local columns 2–4, the
   brackets in the frame style and `■` in the control style.

   ```classic-check
   appearance/Window/normal.classic.dump row 1 col 4 "[■]" doc-frame,doc-control,doc-frame
   gallery_no_graphics.dump row 6 col 22 "[■]" doc-frame,doc-control,doc-frame
   ```

8. **Minimize and zoom controls.** A resizable window ends its top border with
   `[_]` at local `w-8..w-6`, `[↑]` at `w-5..w-3`, one frame cell and the
   corner; the glyphs are in the control style.

   ```classic-check
   appearance/Window/normal.classic.dump row 1 col 28 "[_][↑]═╗" doc-frame,doc-control,doc-frame*2,doc-control,doc-frame*3
   gallery_no_graphics.dump row 6 col 46 "[_][↑]═╗" doc-frame,doc-control,doc-frame*2,doc-control,doc-frame*3
   ```

9. **Zoomed glyph.** A zoomed window shows `↕` in place of `↑`.

   ```classic-check
   appearance/Window/zoomed.classic.dump row 0 col 32 "[_][↕]═╗" doc-frame,doc-control,doc-frame*2,doc-control,doc-frame*3
   ```

10. **Fixed-size windows.** A fixed-size window draws the close control only,
    no minimize or zoom control, and plain corners with no grip.

    ```classic-check
    appearance/Window/fixed.classic.dump row 1 col 22 " ════════════╗" doc-frame
    appearance/Window/fixed.classic.dump row 9 col 33 "══╝" doc-frame
    hello_greeting.dump row 14 col 50 "═╝" dlg-frame
    ```

11. **Pressed control.** A control held down draws all three cells in
    `control-pressed`.

    ```classic-check
    appearance/Window/pressed.classic.dump row 1 col 4 "[■]" control-pressed
    appearance/Window/pressed.classic.dump row 1 col 28 "[_][↑]" doc-frame,doc-control,doc-frame*2,doc-control,doc-frame
    ```

12. **Resize grip.** An active resizable window marks its bottom-right corner
    with the single-line grip `─┘` in the control style; its other corners stay
    double-line.

    ```classic-check
    appearance/Window/normal.classic.dump row 9 col 33 "═─┘" doc-frame,doc-control*2
    appearance/Window/normal.classic.dump row 1 col 2 "╔" doc-frame
    gallery_no_graphics.dump row 21 col 51 "═─┘" doc-frame,doc-control*2
    ```

13. **Grips during a resize.** While a resize is under way all four corners
    show two-cell single-line elbows, three in the frame style and the
    bottom-right in the control style.

    ```classic-check
    appearance/Window/resizing.classic.dump row 1 col 2 "┌─" doc-frame
    appearance/Window/resizing.classic.dump row 1 col 36 "─┐" doc-frame
    appearance/Window/resizing.classic.dump row 10 col 2 "└─═" doc-frame
    appearance/Window/resizing.classic.dump row 10 col 35 "═─┘" doc-frame,doc-control*2
    ```

14. **No grip on an inactive window.** An inactive window's bottom-right corner
    is its plain single-line corner in the frame style (here seen under the
    active window's shadow).

    ```classic-check
    appearance/Window/active.classic.dump row 10 col 36 "─┘" shadow-on-doc-frame-inactive
    ```

15. **Footer.** A footer is drawn on the bottom border from local column 2 as
    padding, text and padding in the title style.

    ```classic-check
    appearance/Window/footer.classic.dump row 9 col 2 "╚═ 2 of 7 ═" doc-frame
    ```

16. **Same-view junctions merge.** A dropdown's separator joins its own frame
    as `├` and `┤`.

    ```classic-check
    hello_file_menu.dump row 3 col 1 "├────────────────────┤" menu
    hello_file_menu.dump row 5 col 1 "├────────────────────┤" menu
    ```

17. **Cross-view lines never merge.** A higher window's border replaces a lower
    window's crossing line, and a submenu's corner replaces its parent's side
    frame, with no junction glyph.

    ```classic-check
    window_z_order_junction.dump row 5 col 2 "└─────────║" doc-frame-inactive*10,doc-frame
    appearance/DropdownMenu/submenu.classic.dump row 6 col 17 "► ┌" menu-active*2,menu
    ```

18. **Shadow geometry.** A window's shadow is a two-column strip on its right
    from one row below its top to one row below its bottom, and a one-row strip
    below it from two columns right of its left edge; the cells at its top-right
    and bottom-left stay unshadowed.

    ```classic-check
    appearance/Window/normal.classic.dump rows 2-10 cols 36-37 "░" shadow-on-desktop
    appearance/Window/normal.classic.dump rows 10-10 cols 4-35 "░" shadow-on-desktop
    appearance/Window/normal.classic.dump row 1 col 36 "░░" desktop
    appearance/Window/normal.classic.dump row 10 col 2 "░░" desktop
    appearance/Window/normal.classic.dump rows 0-11 cols 38-39 "░" desktop
    gallery_no_graphics.dump rows 7-22 cols 54-55 "░" shadow-on-desktop
    gallery_no_graphics.dump rows 22-22 cols 22-53 "░" shadow-on-desktop
    gallery_no_graphics.dump row 6 col 54 "░░" desktop
    gallery_no_graphics.dump row 22 col 20 "░░" desktop
    ```

19. **Shadow transform.** A shadowed cell keeps its glyph and attributes and
    halves every colour channel: on the desktop pattern, on an inactive frame,
    on an active frame and title, and on a window control.

    ```classic-check
    appearance/Window/active.classic.dump row 3 col 36 "─┐" shadow-on-doc-frame-inactive
    appearance/DropdownMenu/normal.classic.dump row 2 col 20 "s " shadow-on-doc-frame
    appearance/DropdownMenu/submenu.classic.dump row 9 col 33 "═─" shadow-on-doc-frame,shadow-on-doc-control
    hello_file_menu.dump rows 2-8 cols 23-24 "░" shadow-on-desktop
    ```

20. **Popup shadows.** A dropdown menu casts the standard shadow onto the
    window beneath it.

    ```classic-check
    appearance/DropdownMenu/normal.classic.dump rows 2-7 cols 20-21 shadow-on-doc-frame
    appearance/DropdownMenu/normal.classic.dump row 8 col 3 "                   " shadow-on-doc-frame
    appearance/DropdownMenu/normal.classic.dump row 8 col 2 "║" doc-frame
    ```

21. **Binary shadow union.** Where a dialog's shadow and its owner window's
    shadow overlap, the cells are dimmed once, not twice.

    ```classic-check
    appearance/MessageBox/active.classic.dump rows 3-9 cols 31-32 "░" shadow-on-desktop
    appearance/MessageBox/active.classic.dump rows 9-9 cols 3-32 "░" shadow-on-desktop
    ```

22. **Shadows spare docked chrome.** A zoomed window casts no visible shadow and
    the status line below it keeps its own styles.

    ```classic-check
    appearance/Application/normal.classic.dump row 10 col 38 "─┘" doc-control
    appearance/Application/normal.classic.dump row 11 col 0 " F1 Help  Ctrl+S Save  Alt+X Quit" status,status-hotkey*2,status*7,status-hotkey*6,status*7,status-hotkey*5,status*5
    ```

23. **Menu bar layout.** The menu bar is row 0 in `menu`; titles start at column
    2, two columns apart, each with its mnemonic in `menu-hotkey`.

    ```classic-check
    appearance/MenuBar/normal.classic.dump row 0 col 0 "  File  View  Window  " menu*2,menu-hotkey,menu*5,menu-hotkey,menu*5,menu-hotkey,menu*7
    gallery_no_graphics.dump row 0 col 0 "  File  Window  " menu*2,menu-hotkey,menu*5,menu-hotkey,menu*7
    hello_initial.dump row 0 col 0 "  File  " menu*2,menu-hotkey,menu*5
    ```

24. **Menu bar highlight.** The highlighted title and one padding cell each side
    are `menu-active`, its mnemonic `menu-active-hotkey`.

    ```classic-check
    appearance/MenuBar/focused.classic.dump row 0 col 0 "  File  View" menu,menu-active,menu-active-hotkey,menu-active*4,menu,menu-hotkey,menu*3
    ```

25. **Dropdown placement and frame.** A dropdown opens on row 1 with its left
    frame one column left of its title's first letter, framed single-line in
    `menu`, as wide as its widest item plus four.

    ```classic-check
    hello_file_menu.dump row 1 col 1 "┌────────────────────┐" menu
    hello_file_menu.dump row 7 col 1 "└────────────────────┘" menu
    appearance/DropdownMenu/marks.classic.dump row 1 col 7 "┌────────────────┐" menu
    ```

26. **Dropdown items and highlight.** Labels start after one padding cell; the
    highlighted item's whole interior, padding included, is `menu-active` with
    its mnemonic in `menu-active-hotkey`; other mnemonics are `menu-hotkey`.

    ```classic-check
    hello_file_menu.dump row 2 col 1 "│ Greeting...  Alt+G │" menu,menu-active,menu-active-hotkey,menu-active*18,menu
    hello_file_menu.dump row 4 col 1 "│ About...        F1 │" menu*2,menu-hotkey,menu*18,menu
    ```

27. **Accelerator column.** Chord hints are right-aligned to the column before
    the right padding cell, and a submenu arrow `►` ends in the same column.

    ```classic-check
    hello_file_menu.dump row 6 col 16 "Alt+X │" menu*7
    appearance/DropdownMenu/normal.classic.dump row 2 col 12 "Ctrl+O │" menu-active*7,menu
    appearance/DropdownMenu/normal.classic.dump row 6 col 1 "│ Recent        ► │" menu*7,menu-hotkey,menu*11
    ```

28. **Submenu placement.** A submenu's left frame lies on its parent's right
    frame column, its top frame on the parent item's row, and its first item is
    highlighted.

    ```classic-check
    appearance/DropdownMenu/submenu.classic.dump row 6 col 19 "┌────────────┐" menu
    appearance/DropdownMenu/submenu.classic.dump row 7 col 19 "│ notes.md   │" menu,menu-active*12,menu
    appearance/DropdownMenu/submenu.classic.dump row 6 col 2 " Recen" menu-active*6
    ```

29. **Check marks.** With a mark column, a checked switch shows `x`, the chosen
    radio item `•`, and unmarked items a blank, one cell after the padding; the
    labels start two cells further right.

    ```classic-check
    appearance/DropdownMenu/marks.classic.dump row 2 col 7 "│ x Word wrap    │" menu,menu-active*3,menu-active-hotkey,menu-active*12,menu
    appearance/DropdownMenu/marks.classic.dump row 3 col 7 "│   Line numbers │" menu*4,menu-hotkey,menu*12,menu
    appearance/DropdownMenu/marks.classic.dump row 5 col 7 "│ • Compact      │" menu*4,menu-hotkey,menu*12,menu
    ```

30. **Menu separators.** A separator is a full-width single rule joining the
    side frames.

    ```classic-check
    appearance/DropdownMenu/normal.classic.dump row 4 col 1 "├─────────────────┤" menu
    appearance/DropdownMenu/marks.classic.dump row 4 col 7 "├────────────────┤" menu
    ```

31. **Disabled menu items.** A disabled item is `menu-disabled` with its
    mnemonic in `menu-disabled-hotkey`; under the highlight it keeps the green
    bar in `menu-active-disabled`.

    ```classic-check
    appearance/DropdownMenu/normal.classic.dump row 5 col 2 " Print    Ctrl+P " menu-disabled,menu-disabled-hotkey,menu-disabled*15
    appearance/DropdownMenu/unavailable.classic.dump row 5 col 2 " Print    Ctrl+P " menu-active-disabled,menu-active-disabled-hotkey,menu-active-disabled*15
    ```

32. **Status line layout.** The status line is the last row in `status`; items
    start at column 1 as chord and label, chords in `status-hotkey`, two blank
    cells apart.

    ```classic-check
    appearance/StatusLine/normal.classic.dump row 11 col 0 " F1 Help  Ctrl+S Save  Alt+X Quit      " status,status-hotkey*2,status*7,status-hotkey*6,status*7,status-hotkey*5,status*11
    gallery_no_graphics.dump row 23 col 0 " F10 Menu  Alt+X Quit " status,status-hotkey*3,status*7,status-hotkey*5,status*6
    hello_initial.dump row 23 col 0 " Alt+X Quit  F10 Menu " status,status-hotkey*5,status*7,status-hotkey*3,status*6
    ```

33. **Status hint.** A hint follows the last item's trailing padding as `│`, a
    space and the hint text, in `status`.

    ```classic-check
    appearance/StatusLine/hint.classic.dump row 11 col 32 "t │ Save" status
    ```

34. **Pressed status item.** A pressed item and one padding cell each side are
    `status-pressed`, its chord `status-pressed-hotkey`.

    ```classic-check
    appearance/StatusLine/pressed.classic.dump row 11 col 0 " F1 Help  Ctrl+S" status-pressed,status-pressed-hotkey*2,status-pressed*6,status,status-hotkey*6
    ```

35. **Unavailable status item.** An unavailable item draws chord and label in
    `status-disabled` with no chord accent.

    ```classic-check
    appearance/StatusLine/unavailable.classic.dump row 11 col 21 "  Ctrl+P Print  " status*2,status-disabled*12,status*2
    ```

36. **Button face and shadow.** A button's face row is a spacer, the face and a
    closing `▄`; its second row is two spacers and a `▀` run; the spacers and
    shadow glyphs are `button-shadow`, the label centred with its mnemonic in
    `button-mnemonic`.

    ```classic-check
    appearance/Button/normal.classic.dump row 3 col 9 "    Save   ▄" button-shadow,button*3,button-mnemonic,button*6,button-shadow
    appearance/Button/normal.classic.dump row 4 col 9 "  ▀▀▀▀▀▀▀▀▀▀" button-shadow
    hello_greeting.dump row 13 col 29 "  ▀▀▀▀▀▀▀▀" button-shadow
    ```

37. **Default-button marking.** The default button's text is `button-default`
    (light cyan) while another control has focus; other buttons are `button`.

    ```classic-check
    appearance/Button/default.classic.dump row 3 col 10 "   Save   " button-default*3,button-mnemonic,button-default*6
    rootdialog_initial.dump row 9 col 1 "    OK   ▄    Cancel ▄" button-shadow,button-default*3,button-mnemonic,button-default*4,button-shadow*4,button,button-mnemonic,button*5,button,button-shadow
    gallery_no_graphics.dump row 6 col 4 "   Greet   ▄" button-shadow,button-default*2,button-mnemonic,button-default*7,button-shadow
    ```

38. **Focused button.** A focused button's text is `button-focused` (white).

    ```classic-check
    appearance/Button/focused.classic.dump row 3 col 10 "   Save   " button-focused*3,button-mnemonic,button-focused*6
    hello_greeting.dump row 12 col 29 "    OK   ▄" button-shadow,button-focused*8,button-shadow
    ```

39. **Pressed button.** A pressed button's face moves one cell right and both
    shadow runs disappear.

    ```classic-check
    appearance/Button/pressed.classic.dump row 3 col 9 "     Save   " button-shadow*2,button-focused*3,button-mnemonic,button-focused*6
    appearance/Button/pressed.classic.dump row 4 col 9 "            " button-shadow
    ```

40. **Disabled button.** A disabled button is `button-disabled` with no
    mnemonic accent and keeps its shadow.

    ```classic-check
    appearance/Button/disabled.classic.dump row 3 col 10 "   Save   ▄" button-disabled*10,button-shadow
    ```

41. **Input line.** An input line is `input` from its first column, with no
    brackets or padding; an invalid one is `input-invalid`, a disabled one
    `input-disabled`, a password one echoes `*`.

    ```classic-check
    appearance/InputLine/normal.classic.dump row 3 col 3 " report.txt          " dlg-frame,input*20
    appearance/InputLine/invalid.classic.dump row 3 col 4 "12x" input-invalid
    appearance/InputLine/disabled.classic.dump row 3 col 4 "report.txt" input-disabled
    appearance/InputLine/password.classic.dump row 3 col 4 "******" input
    ```

42. **Focused input line.** A focused field's value is `input-focused`, its
    caret cell and selected text `input-caret` (reverse video).

    ```classic-check
    appearance/InputLine/focused.classic.dump row 3 col 4 "report.txt  " input-caret*11,input-focused
    appearance/InputLine/selected.classic.dump row 3 col 4 "report.txt" input-focused*7,input-caret*3
    gallery_no_graphics.dump row 4 col 12 "   " dlg-frame-inactive,input-caret,input-focused
    ```

43. **Label mnemonic.** A label is `label` with its mnemonic in
    `label-mnemonic`.

    ```classic-check
    appearance/Label/normal.classic.dump row 3 col 4 "Name:" label-mnemonic,label*4
    gallery_no_graphics.dump row 4 col 4 "Name:" label-mnemonic,label*4
    ```

44. **Vertical scrollbar.** A vertical scrollbar is `▲`, blank track cells, a
    `█` thumb and `▼`, all in `scroll`.

    ```classic-check
    appearance/Scrollbar/normal.classic.dump row 2 col 26 "▲" scroll
    appearance/Scrollbar/normal.classic.dump row 3 col 26 " " scroll
    appearance/Scrollbar/normal.classic.dump row 4 col 26 "█" scroll
    appearance/Scrollbar/normal.classic.dump rows 5-6 cols 26-26 " " scroll
    appearance/Scrollbar/normal.classic.dump row 7 col 26 "▼" scroll
    appearance/Scrollbar/full.classic.dump rows 3-6 cols 26-26 "█" scroll
    ```

45. **Half-cell thumbs and the horizontal bar.** A half-covered cell shows a
    half block; a horizontal bar runs `◄`, track, thumb and `►`.

    ```classic-check
    appearance/Scrollbar/half-cell.classic.dump row 3 col 26 "▄" scroll
    appearance/Scrollbar/horizontal.classic.dump row 7 col 2 "◄         █████▌       ►" scroll
    appearance/ListView/normal.classic.dump row 4 col 26 "▀" scroll
    ```

46. **List focus cue.** A focused list's cursor row is a full-width
    `list-cursor` bar; unfocused, it is `list-cursor-inactive`; other rows are
    `list`.

    ```classic-check
    appearance/ListView/focused.classic.dump row 3 col 2 "capabilities.md         " list-cursor
    appearance/ListView/normal.classic.dump row 3 col 2 "capabilities.md         " list-cursor-inactive
    appearance/ListView/normal.classic.dump row 2 col 2 "boot-sequence.md        " list
    ```

47. **Option-group focus cue.** In a focused check or radio group, the cursor
    row is `option-focused` and the caption `caption-focused`; unfocused, the
    rows are `option` and the caption `label`, mnemonics yellow throughout.

    ```classic-check
    appearance/CheckGroup/focused.classic.dump row 3 col 3 "Compilation" label-mnemonic,caption-focused*10
    appearance/CheckGroup/focused.classic.dump row 4 col 3 " [X] Optimize" option-focused*5,option-mnemonic,option-focused*7
    appearance/CheckGroup/focused.classic.dump row 5 col 3 " [ ] Debug info" option*5,option-mnemonic,option*9
    appearance/CheckGroup/normal.classic.dump row 3 col 3 "Compilation" label-mnemonic,label*10
    appearance/RadioGroup/focused.classic.dump row 5 col 3 " (•) Shared" option-focused*6,option-mnemonic,option-focused*4
    ```
<!-- classic-look:checklist:end -->

## Sign-off

The owner signs the Classic look off against the checklist above for the
revision named. A later change to this specification or to any cited golden
needs a new row.

| Date | Revision | Signature | Notes |
|---|---|---|---|
| | | | |
