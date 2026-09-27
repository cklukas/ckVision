# ckVision Golden Dump Format, version 1

The golden dump is the textual representation of a composed frame — the
specification medium of the project (the decision log D-014). Version 1 is
defined here and implemented in `<cvision/core/golden.hpp>`. It is a
line-oriented UTF-8 text format; the file must end with a final newline.

A dump in **canonical form** (exactly the shape the serializer emits)
round-trips byte-exactly: `serialize(parse(text)) == text`. Non-canonical
but valid input (lowercase hex colors) parses and normalizes.

## Structure

```
ckvision-golden 1
frame <cols> <rows>
cursor hidden | cursor <col> <row> <shape>
styles <count>
<index> fg <color> bg <color> attrs <attrs> [underline <shape>] [ulcolor <color>]
grid
|<row text>|                                    (<rows> lines)
stylemap
|<row style characters>|                        (<rows> lines)
link <col> <row> <cols> <target>
raster <id> anchor <col> <row> span <cols> <rows> pixels <w> <h> hash <hex>
end
```

- Tokens are separated by exactly one space. Lines by `\n`.
- Integers are canonical non-negative decimals: digits only, no sign,
  no leading zeros (the literal `0` excepted).
- `<color>`: `default`, `@<index>` (a palette entry, 0–255), or `#RRGGBB`
  (canonical: uppercase hex). The three are distinct facts and a dump keeps
  them apart: "the palette's red" is not the same thing as "this particular
  red", and only the first can be re-themed later.
- `<attrs>`: `-` for none, or a comma-separated subset of
  `bold,dim,italic,underline,reverse,strike` (no duplicates; order
  preserved as written).
- `underline <shape>` and `ulcolor <color>` refine an underline that is
  being drawn, and appear only on a style whose attrs include `underline`.
  `<shape>` is one of `double,curly,dotted,dashed`; the plain rule is what an
  underline is unless something says otherwise, and is spelled by its absence.
  So is a rule that simply follows the text, which is why `ulcolor default`
  is rejected rather than written. One appearance has exactly one spelling,
  which is what keeps the round trip byte-exact.
- `<shape>`: `block`, `bar`, or `underline`. Cursor coordinates are
  0-based cells and must lie inside the frame.
- `styles` declares `<count>` styles (0 <= count <= 62 in version 1),
  indexed contiguously from 0 in declaration order. A count of 0 is
  grammatical but unusable in practice: every stylemap cell must
  reference a declared style.
- `grid` holds one line per frame row between `|` delimiters. Row
  content is preserved as raw bytes; interior `|` characters are
  allowed (delimiters are the first and last character of the line).
- `stylemap` holds one character per cell from the alphabet
  `0-9A-Za-z` (index 0–61); every character must reference a declared
  style, and each line must have exactly `<cols>` characters.
- `link` records are optional, zero or more, after `stylemap` and before
  any `raster` record (D-088). Each is a run of `<cols>` cells of row `<row>`
  from column `<col>` (0-based, a wide glyph's continuation columns included)
  that are part of a hyperlink to `<target>`: which cells a terminal that
  renders hyperlinks would make clickable. The run must be non-empty and lie
  inside its row, and `<target>` must be a valid terminal hyperlink
  (`is_valid_hyperlink_target`: an absolute URI of printable ASCII with no
  space), which is also what makes it a single token. Runs are maximal and in
  row-major order: they may not overlap, and two runs that touch on one row
  must have different targets, so one frame has one spelling. A frame without
  links has no `link` line, which is why every dump written before links
  existed is still byte-identical; the format keeps version 1, as it did for
  D-080, because nothing outside this repository reads it before 1.0.
- `raster` records are optional, zero or more, after the `link` records:
  cell-anchored raster regions with a positive `<id>` unique within the
  dump, a 0-based anchor, a positive cell span (the region must lie
  entirely inside the frame), a positive pixel extent, and a non-empty
  lowercase-hex content hash. A region records no fallback state: its
  cells always hold the fallback, and whether pixels cover them is the
  Presenter's decision, which the rendered-graphics capture below tests
  (D-080).
- `end` terminates the document; nothing may follow it.

## Version-1 limitations (by design)

- Grid row content is raw bytes. The parser does not segment graphemes
  or check column widths (wide cells, continuation columns); the stylemap
  line length is the authoritative column count. A captured Surface is
  width-correct by construction, because the capture writes each cell's
  grapheme once and a wide cell's continuation column as nothing.
- At most 62 styles per frame. A later version adds a declared
  multi-character style column width if a real frame ever needs more.

## Script goldens and the presented display

A script golden is the composed frame (`Application::composed_surface()` and
its cursor) after one step of an event script whose input enters only through
`HeadlessTerminal` and `Application::step`. Each script has a generator under
`tools/docgen/generate_*.cpp` that runs the same script and writes the dumps
into `tests/golden/`; the test compares its own captures with those files, and
`generated_golden_bytes` regenerates every one of them and compares the bytes
on each host. A dump may also be a region of the frame: the Layouts container
suite pins each container family's own rectangle, so a change in one family's
layout shows up in that family's file alone.

A composed frame says what the application meant to show, not what the
terminal shows. The resize, window-resize, Layouts, File Browser and
shrink-storm scripts therefore also check, after every step, that the display
`HeadlessTerminal` decoded from the bytes actually written equals the composed
frame cell for cell, so a cell the Presenter left stale after a grow or a
shrink fails the script even where the golden is right. That comparison runs
on a TrueColor profile (`headless_no_graphics_profile()`): under a palette
depth the Presenter rightly writes the nearest index for an RGB style.

## Raster records versus rendered-graphics captures

The `raster` line is a symbolic scene/compositor oracle. It proves which image
slice is visible, where it is anchored, and its pixel extent and content hash.
It does **not** prove that Presenter
emitted valid Sixel, that a terminal decoded it, or that the pixels appeared at
the right place over the fallback cells.

D-035 therefore requires a separate virtual-display visual golden. That
capture is produced by feeding the exact `Terminal::write` byte stream into an
independently tested VT/Sixel decoder and comparing its resulting cell + RGBA
planes. Raster-bearing acceptance runs the same script twice: a fixed-metrics
Sixel profile must show decoded pixels, while NoGraphics must show the cell
fallback and zero raster pixels. In the Sixel capture, cells below every
visible raster slice must be style-preserving blanks—never fallback glyphs—and
the encoder-produced pixel extent must be fully opaque. The symbolic dump
remains intentionally diff-friendly; the decoded visual capture tests the
protocol path it cannot.

A composed frame's `raster` records come from the compositor, not from the
frame's surface, which holds cells only. For an application, the symbolic
frame dump is `scene::capture_frame(app.compositor(), app.current_cursor())`:
one record per visible, occlusion-sliced picture. Such a record is the slice
the reader sees: its anchor and span are the slice's visible cells, and its
pixel extent and hash are those of the part of the picture those cells show
— the visible cells' place within the picture's full anchor, in the
picture's own pixels (at least one each way). A picture shown whole records
the picture itself. One that is scrolled, clipped or covered records only
its visible part, so a picture scrolled under a fixed clip changes its record
even where its cells stay put (D-081), and the slices occlusion cuts one
picture into each name their own part of it. The appearance matrix
writes it as `<Element>/<state>.<scheme>.scene` for every state of a
raster-bearing element, beside that state's post-presenter `.dump`. Its gate
requires the `sixel` and `no-graphics` scenes to be byte-identical: the
graphics profile changes what the presenter emits, never the composed scene.

## Raw terminal-plane companion fixtures

`tests/golden/*.rgba` are byte-exact companions to the textual cell dumps.
Their format is the ASCII header `ckvision-rgba 1\n`, followed by a decimal
`<width> <height>\n` line and exactly `width * height * 4` bytes of
row-major RGBA pixels. Each channel is one unsigned byte; transparent pixels
are `(0, 0, 0, 0)`. The dimensions describe the complete decoded plane, not
only its opaque bounds. These files are test fixtures, separate from the
version-1 golden dump grammar above.

`generate_terminal_plane_goldens` obtains the cells and raster bytes from
`VirtualDisplay`, the embedded child emulator, Presenter output, the real
Gallery application object graph, and a child `TerminalView` in an
`ApplicationShell` window. The contained child fixtures pin the decoded outer
plane after initial paint, parent move and resize, overlap, child clear,
repaint, parent close, and a NoGraphics fallback. A separate contained-child
sequence pins initial, clipped shrink, move, restore, and several bounds changes
coalesced into one frame; its generator checks exact opaque-pixel counts and
rejects every pixel outside the current child content rectangle. The restored
and coalesced frames match the earlier enlarged frame byte for byte.
A three-stage two-child sequence pins simultaneous red and green contained
rasters, green after the red window closes, and green with a newly opened blue
window. The generator checks exact color counts and rejects opaque pixels
outside the visible child content rectangles at each stage.
`generated_golden_bytes` compares all of those bytes with the pinned fixtures
on each platform. The paired dumps make
the cell fallback under NoGraphics and the blank cells below visible Sixel
slices reviewable alongside the raw raster plane. The existing small visual
manifests still pin bounds and hashes as convenient diagnostics; they are not
the byte-equality oracle.

## Event scripts

`generate_event_script_goldens` writes the frames of the event scripts in
`tools/docgen/event_script.hpp`. An event script is a list of named beats of
terminal input. Its player injects each event through `HeadlessTerminal`, runs
`Application::step` for it, and runs one more step once the beat's input is
consumed, so posted work such as a closed window's detach has run before the
frame is taken. The generator and the test that owns a script play the same
beat list, and the test compares the frame of every beat that names a golden
with the pinned file. The scripts are:

- the M4 form demo (`rootdialog_*.dump`, `tests/test_rootdialog_smoke.cpp`);
- the M5 close-veto paths (`close_veto_*.dump`,
  `tests/test_close_veto_scripts.cpp`);
- the M5 keyboard-only and mouse-only menu scripts (`menu_*.dump`,
  `tests/test_menu_operability_scripts.cpp`);
- TextView links as terminal hyperlinks (`text_view_hyperlinks_*.dump`,
  `tests/test_text_view_hyperlinks.cpp`). Each pinned beat is written twice:
  the composed frame, and beside it as `*_presented.dump` the display decoded
  from the bytes the Presenter wrote on a host that renders OSC 8, whose
  `link` records are the hyperlinks a terminal would show.

Two routes that end on the same frame share one golden. The close control and
the Close chord both end on `close_veto_refused.dump`, and the keyboard and the
pointer both open the submenu as `menu_submenu.dump`. `generated_golden_bytes`
regenerates these frames on every host as well.

## Paired raster scripts

A raster-bearing event script is played twice in lockstep: on a Sixel profile
and on NoGraphics, both reporting the same fixed 3 × 6-pixel cell
(`tools/docgen/plane_capture.hpp`). Every beat whose golden names a file stem
`S` is pinned four ways:

| File | Holds |
|---|---|
| `S.scene` | the symbolic scene (`capture_frame`), which both runs must compose byte for byte |
| `S.dump` | the cells decoded from the Sixel run's bytes |
| `S.rgba` | the pixel plane decoded from the Sixel run's bytes |
| `S_no_graphics.dump` | the cells decoded from the NoGraphics run's bytes, whose plane holds no pixel |

A beat that must return to an earlier frame exactly names that frame's stem,
and the generator refuses to write when the frame differs. The paired scripts
are:

- the scrolled pictures of D-081 (`raster_scroll_*`,
  `tools/docgen/raster_scroll_script.hpp`, written by
  `generate_raster_scroll_goldens`, played by
  `tests/test_raster_scroll_golden.cpp`): a picture in a `ScrollViewport`
  and a picture inline in a `FlowView`, each scrolled by one row and then by
  three more;
- the Gallery's image demo (`gallery_picture_*`) and its four schemes chosen
  through View → Scheme (`gallery_scheme_*`), both in
  `tools/docgen/gallery_script.hpp`, written by `generate_gallery_goldens` and
  played by `tests/test_gallery_visual_golden.cpp`.

`generated_golden_bytes` regenerates all of them on each host. The tests add
what the bytes alone cannot say, from the decoded planes and the compositor's
slices (`tools/docgen/raster_checks.hpp`): a picture moved by exactly the rows
scrolled and every one of its pixels with it, no pixel outside the visible
slices and no hole inside one, and a covered picture unchanged in the open
and darker under a shadow.

## Virtual-display protocol subset

The virtual display is an intentionally bounded test decoder, not a general
terminal emulator. It accepts only output forms that `Presenter` is permitted
to emit: ECMA-48 CUP (`CSI H` and `CSI f`), SGR (`CSI m`), ED/EL (`CSI J` and
`CSI K`), SU/SD (`CSI S` and `CSI T`), and private cursor and synchronized
output modes (`CSI ?25 h/l`, `CSI ?2026 h/l`). It handles printable UTF-8
graphemes and rejects every other C0, ESC, CSI, and DCS form.

Its OSC forms are the three a session sends outside its frames, each ended by
BEL or `ESC \`: OSC 22 (pointer shape), OSC 0 (window title, kept as
`window_title()`), and OSC 52 on selection `c` (clipboard export, kept decoded
as `clipboard_text()`); and OSC 8, the hyperlink the Presenter opens and
closes around linked cells inside a frame. It holds them to the
[OSC emission safety](terminal-host-integration.md#osc-emission-safety)
contract: a title carrying any control code point, `DEL`, or malformed UTF-8,
and a clipboard payload that is not strict RFC 4648 base64, invalidate the
capture. Every other OSC is rejected.

OSC 8 is modelled as a host that renders hyperlinks: text printed while one is
open joins it, and `frame().link_target()` reports each cell's target, so a
test can assert which cells a terminal would make clickable and a dump of the
decoded display carries `link` records. It accepts only the form the Presenter
writes: `OSC 8 ; id=<hyperlink_id(target)> ; <target>` with a valid target to
open, `OSC 8 ; ;` to close, an open only while none is open and a close only
while one is. While a hyperlink is open only printable text, SGR and the
closing OSC 8 may follow; cursor addressing, erase, scroll, a mode change, a
DCS or another OSC invalidates the capture, as does a stream that finishes
with a hyperlink still open. Erased and overprinted cells lose their link;
scrolled cells keep it.

SGR is the one control it reads sub-parameters in, because it is the one the
Presenter writes them in: `4:0`–`4:5` for the shape of an underline, `58` and
`59` for the underline's own colour in either spelling, and `21` for the
double rule. A colon anywhere else is a malformed sequence, not an extension,
and invalidates the capture. Indexed colours decode back to indices: this
decoder is an oracle for what the Presenter wrote, and "the host was told
index 4" is the fact worth recording — resolving it here would invent a
palette the receiving terminal never consulted.

Its DCS form is published Sixel only: numeric DCS parameters with background
mode 0 or 1; color-register selection and HLS-free RGB definition (`#`);
raster attributes (`"`); repeat (`!`); carriage return (`$`); new sixel row
(`-`); and data bytes `?` through `~`. Raster pixels are clipped to the fixed
terminal pixel plane. Background mode 0 clears the declared raster extent
before painting, so a smaller replacement cannot leave stale pixels. Text,
erase, scroll, resize, and re-presentation clear or move their corresponding
pixel coverage deterministically.

Parser limits are part of the capture contract: CSI input is limited to 256
bytes, text and DCS input to 16 MiB, and a Sixel repeat to 1,000,000 columns.
An incomplete, malformed, unsupported, or over-limit stream invalidates the
capture rather than producing a plausible partial image. WP-33 owns fuzzing
this incremental parser; the hand-authored fixtures in
`tests/test_virtual_display*.cpp` cover its accepted subset and rejection
boundaries before that fuzz corpus exists.

## Example

```
ckvision-golden 1
frame 12 3
cursor 6 1 block
styles 2
0 fg #C0C0C0 bg #000080 attrs -
1 fg #FFFFFF bg #000080 attrs bold
grid
|+----------+|
|| Hello ck ||
|+----------+|
stylemap
|000000000000|
|011111111110|
|000000000000|
raster 1 anchor 2 1 span 4 2 pixels 64 32 hash a1b2c3d4
end
```

A style carrying a palette colour and a curly underline in the palette's
bright red — what a compiler's error mark looks like inside an embedded
terminal — is written:

```
3 fg @1 bg default attrs underline underline curly ulcolor @9
```
