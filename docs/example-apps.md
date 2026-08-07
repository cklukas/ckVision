# Example applications

ckVision ships example applications the way tvision-sixel does —
small, complete programs that exercise the library end to end rather
than isolated widget demos. Each one lives under `examples/<name>/`
as a plain library-linkable static target (`<name>_app`) plus a thin
`main.cpp` that attaches it to a real terminal, so the exact same
object graph an interactive user runs is also what the headless test
suite drives — a documentation screenshot or a test failure both
describe the real application, never a simplified stand-in.

This page is generated documentation in the sense that its screenshots
are: they come from `tools/docgen/capture_gallery_screenshots`, which
builds an example app on a `HeadlessTerminal`, scripts a short
interaction, and renders the actually-composed frame to SVG via
`tools/docgen/frame_svg`. Regenerate them (and this page's HTML/PDF
output) with `tools/docgen/generate_docs.sh` — never hand-edit an SVG
under `docs/generated/`.

## Gallery (`examples/gallery`)

Gallery is the baseline example: a `Desktop` with a menu bar, a status
line, and two windows, driven entirely through the public `ui::View` /
`ui::Application` / `widgets::*` surface — nothing in `gallery_app.cpp`
reaches past those headers.

- **Menu bar** (`File`, `Window`) bound to real `ui::CommandRegistry`
  commands, activated via F10 (a bound command, not a MenuBar special
  case — see `include/cvision/widgets/menu.hpp`'s own file comment for
  why), with Esc restoring whatever was focused before activation.
- **Controls window**: a `Label` with a mnemonic, an `InputLine`, and a
  default `Button` — Tab traversal, typed text, and a click all reach
  their target through the ordinary focus/dispatch machinery
  (`ui::Application::dispatch`), the same path any application's own
  widgets use.
- **Sixel Demo window**: an `ImageView` showing a generated RGBA
  gradient, proving the raster path — `Painter::draw_image` through
  `scene::Compositor`'s occlusion slicing through `term::Presenter`'s
  Sixel encoder — actually reaches terminal bytes when the terminal
  capability advertises Sixel support.
- **Window shadows**: both windows cast a real, composited shadow
  (`widgets::Desktop::paint_children` interleaves
  `Painter::transform_style` with the z-ordered window paint — see
  that method's doc comment for why a naive whole-tree post-pass
  cannot get shadow occlusion right).
- **Move/resize**: `Window`'s own mouse handling supports dragging the
  title bar to move and dragging an edge/corner to resize, clamped to
  `Window`'s declared min/max size.

### What's verified, and how

Every claim above is backed by a real, green test — not just "the code
exists":

| Behavior | Test |
|---|---|
| Both windows render, with their titles | `test_gallery_smoke.cpp`: `the_gallery_renders_both_windows_titles_on_first_frame` |
| Menu bar + status line render | `the_menu_bar_and_status_line_both_render` |
| Typed keyboard input reaches the field and repaints | `typing_into_the_name_field_reaches_it_and_repaints` |
| F10 activates the menu bar via the command keymap | `f10_activates_the_menu_bar_via_the_command_keymap` |
| Esc after F10 restores prior focus | `escape_after_f10_returns_focus_to_where_it_was` |
| Alt+X quits via the documented shortcut | `alt_x_quits_via_the_status_lines_documented_shortcut` |
| A mouse click on Greet opens a message box with the typed name | `clicking_greet_opens_a_message_box_that_renders_the_typed_name` |
| Sixel bytes actually reach the terminal under full capabilities | `the_image_window_content_reaches_the_terminal_as_sixel_data_under_full_capabilities` |
| Window shadow cells are actually dimmed (exact expected style) | `tests/test_desktop.cpp`: `a_shadow_casting_windows_footprint_is_dimmed_on_the_desktop` |
| A higher window is never dimmed by a lower window's shadow | `a_higher_window_painted_afterward_is_not_dimmed_by_a_lower_windows_shadow` |

### Screenshots

Initial frame — menu bar, status line, both windows, the "Controls"
window's shadow visible under the "Sixel Demo" window's left edge:

![Gallery: initial frame](generated/screenshots/gallery-initial.svg)

After typing a name into the input field:

![Gallery: typed name](generated/screenshots/gallery-typed-name.svg)

With the menu bar activated (F10):

![Gallery: menu open](generated/screenshots/gallery-menu-open.svg)

## Running it yourself

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/examples/ckvision_gallery
```

Requires a real terminal (macOS Terminal.app, iTerm2, or any VT100+
terminal); Sixel image content additionally requires a Sixel-capable
terminal (iTerm2 with Sixel enabled, or a recent xterm build). On an
unsupported terminal, `ImageView`'s mandatory fallback (D-017) still
renders equivalent cell content in place of the image — never a blank
or broken region.

## Adding another example

1. `examples/<name>/<name>_app.{hpp,cpp}` — the object graph, taking
   `ui::Application&` in its constructor, exposing whatever accessors
   a headless test needs (see `gallery_app.hpp`'s own accessor list for
   the pattern).
2. `examples/<name>/main.cpp` — a `PosixTerminal` + `PosixClock` +
   `Application`, a `step()` loop, nothing else.
3. `examples/CMakeLists.txt` — one more `add_library(<name>_app
   STATIC ...)` / `add_executable(ckvision_<name> ...)` pair, following
   `gallery_app`'s.
4. `tests/test_<name>_smoke.cpp` — drive the SAME `<name>_app` target
   headlessly; this is what makes the example a tested artifact rather
   than a demo nobody runs in CI.
5. A section on this page, plus a `capture_<name>_screenshots` tool
   under `tools/docgen/` if the example's states are worth showing.
