# ckVision Golden Dump Format, version 1

The golden dump is the textual representation of a composed frame — the
specification medium of the project (DECISIONS.md D-014). Version 1 is
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
<index> fg <color> bg <color> attrs <attrs>     (one line per style)
grid
|<row text>|                                    (<rows> lines)
stylemap
|<row style characters>|                        (<rows> lines)
raster <id> anchor <col> <row> span <cols> <rows> pixels <w> <h> hash <hex> fallback <state>
end
```

- Tokens are separated by exactly one space. Lines by `\n`.
- Integers are canonical non-negative decimals: digits only, no sign,
  no leading zeros (the literal `0` excepted).
- `<color>`: `default` or `#RRGGBB` (canonical: uppercase hex).
- `<attrs>`: `-` for none, or a comma-separated subset of
  `bold,dim,italic,underline,reverse,strike` (no duplicates; order
  preserved as written).
- `<shape>`: `block`, `bar`, or `underline`. Cursor coordinates are
  0-based cells and must lie inside the frame.
- `styles` declares `<count>` styles (0 ≤ count ≤ 62 in version 1),
  indexed contiguously from 0 in declaration order. A count of 0 is
  grammatical but unusable in practice: every stylemap cell must
  reference a declared style.
- `grid` holds one line per frame row between `|` delimiters. Row
  content is preserved as raw bytes; interior `|` characters are
  allowed (delimiters are the first and last character of the line).
- `stylemap` holds one character per cell from the alphabet
  `0-9A-Za-z` (index 0–61); every character must reference a declared
  style, and each line must have exactly `<cols>` characters.
- `raster` records are optional, zero or more, after `stylemap`:
  cell-anchored raster regions with a positive `<id>` unique within the
  dump, a 0-based anchor, a positive cell span (the region must lie
  entirely inside the frame), a positive pixel extent, a non-empty
  lowercase-hex content hash, and fallback state `active` or `hidden`.
- `end` terminates the document; nothing may follow it.

## Version-1 limitations (by design)

- Grid row content is raw bytes; grapheme segmentation and column-width
  validation (wide cells, continuation columns) arrive with the core
  Unicode services in milestone M1 and the Surface capture in M2. Until
  then the stylemap line length is the authoritative column count.
- At most 62 styles per frame. A later version adds a declared
  multi-character style column width if a real frame ever needs more.

## Raster records versus rendered-graphics captures

The `raster` line is a symbolic scene/compositor oracle. It proves which image
slice is visible, where it is anchored, its pixel extent and content hash, and
whether a mandatory fallback exists. It does **not** prove that Presenter
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

## Virtual-display protocol subset

The virtual display is an intentionally bounded test decoder, not a general
terminal emulator. It accepts only output forms that `Presenter` is permitted
to emit: ECMA-48 CUP (`CSI H` and `CSI f`), SGR (`CSI m`), ED/EL (`CSI J` and
`CSI K`), SU/SD (`CSI S` and `CSI T`), and private cursor and synchronized
output modes (`CSI ?25 h/l`, `CSI ?2026 h/l`). It handles printable UTF-8
graphemes and rejects every other C0, ESC, CSI, and DCS form.

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
|╔══════════╗|
|║ Hello ck ║|
|╚══════════╝|
stylemap
|000000000000|
|011111111110|
|000000000000|
raster 1 anchor 2 1 span 4 2 pixels 64 32 hash a1b2c3d4 fallback active
end
```
