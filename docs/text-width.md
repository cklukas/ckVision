# ckVision Text Width and Grapheme Segmentation, v1

`ckv::text` (`include/cvision/core/text.hpp`, `src/core/text.cpp`)
implements grapheme cluster segmentation and column-width computation —
the library's internal width truth (ARCHITECTURE.md §2). Agreement with
what a *real terminal* actually renders is a separate, later concern
handled as a probed capability in the `term` layer (DECISIONS.md D-019);
this document covers only the library's own tables and algorithm.

## Grapheme segmentation

Implements the core boundary rules of Unicode Standard Annex #29
(GB3–GB13, GB999) with a hand-curated property table rather than the
full Unicode Character Database. Supported categories: `CR`, `LF`,
`Control`, `Extend`, `ZWJ`, `Regional_Indicator`, `Prepend`, and the
Hangul jamo classes `L`/`V`/`T`/`LV`/`LVT` (the last computed
algorithmically from Unicode's own Hangul decomposition formula — exact,
not table-driven).

**Known v1 gap: `SpacingMark` (GB9a) is not implemented.** This category
covers spacing combining marks in scripts such as Devanagari, Bengali,
and other Brahmic scripts. Text using them is not corrupted — the mark
simply segments as its own single-codepoint grapheme cluster instead of
merging with its base consonant, which over-segments those scripts but
never miscounts width or crashes. Extending coverage is tracked
informally; revisit if or when a real corpus needs it.

`Extend` coverage: Latin/Cyrillic/Greek-style combining diacritics
(Combining Diacritical Marks and its Extended/Supplement blocks, plus
the Combining Diacritical Marks for Symbols block), `ZWNJ`, Unicode
variation selectors (`FE00`–`FE0F`, the Supplement block), and emoji
skin-tone modifiers. Combining marks for Arabic, Hebrew, and Southeast
Asian scripts are **not yet tabulated** — same graceful degradation as
the `SpacingMark` gap (over-segmentation, not corruption).

`Extended_Pictographic` (used only for the GB11 emoji-ZWJ-sequence rule,
never as a grapheme category by itself) is a block-level approximation
of the well-known emoji ranges, deliberately excluding the Regional
Indicator block and the skin-tone-modifier range so those stay under
their own precise rules. Some non-emoji codepoints inside the broad
blocks are over-included; the only consequence is an occasional
unnecessary non-break at a ZWJ boundary, never a wrong break.

## Column width

`codepoint_width` returns 0 for `Control`/`CR`/`LF`/`Extend`/`ZWJ`, else
2 if the codepoint falls in the East-Asian-Wide table, else 1.

The wide table covers the primary CJK ideograph blocks (main + Extension
A, plus the supplementary-plane extensions), Hiragana/Katakana/Bopomofo/
Hangul-compatibility-Jamo/CJK-symbols-and-punctuation, precomposed
Hangul syllables, CJK compatibility ideographs and forms, fullwidth
forms, and the emoji planes that render wide on every mainstream
terminal (`1F300`–`1F9FF`ish, `1FA70`–`1FAFF`). Rarer wide scripts
(Tangut, Nüshu, and similar) are **out of v1 scope** — narrower default
by omission, not corruption.

**East-Asian-Ambiguous defaults to narrow (width 1)** — no separate
table exists for that class in v1, matching the documented D-019 default
policy. A future capability-driven override (D-OPEN-7) would widen this
class per-terminal, not per-library-default.

`grapheme_width` computes the width of an already-segmented cluster, not
merely its first codepoint's width, because three cases resolve
differently at the cluster level:

- **Emoji presentation selectors** (`FE0F`/`FE0E`, "VS16"/"VS15"):
  override the base codepoint's width outright — `FE0F` forces width 2
  (emoji presentation), `FE0E` forces width 1 (text presentation). This
  is the textbook D-019 divergence case: real terminals disagree on
  these, sometimes even with the selector present — see
  `tests/test_text_width.cpp` for cases marked `// D-019 divergent`.
- **Regional-indicator pairs** (flags): a cluster that *starts with* two
  regional-indicator codepoints is width 2, even if GB9 has legitimately
  attached a trailing `Extend`/`ZWJ` codepoint (a stray combining mark,
  say) — the check is "leading RI pair", not "every codepoint in the
  cluster is RI and there are exactly two". Each RI codepoint is
  individually narrow (`East_Asian_Width=Neutral`) per the Unicode data
  itself; only the paired cluster is wide.
- **ZWJ sequences** (family emoji, profession emoji, …): any cluster
  containing a ZWJ is treated as width 2.

## Sanitization

`sanitize_display_text` replaces every C0 control (`00`–`1F`), `DEL`
(`7F`), and C1 control (`80`–`9F`) — plus any byte sequence that failed
UTF-8 decoding — with U+FFFD. This is the text-to-Cell boundary
neutralization required by DECISIONS.md D-040: hostile or accidental
control bytes in application-supplied display text can never reach the
presenter as control data. Unlike bracketed-paste sanitization (which
exempts tab and newline because paste inserts multi-line text), this
boundary replaces *every* control character without exception, because
a `Cell` holds exactly one grapheme cluster — an application that wants
multi-line layout splits on `\n` before this boundary, not after it.

## Methodology and regeneration

The tables here are hand-curated from well-established, stable Unicode
block structure rather than machine-generated from the raw Unicode
Character Database files, because exact byte-for-byte UCD extraction
was not practical to verify in the environment this module was
authored in. Every range is chosen conservatively (prefer omission —
falls back to width 1 / `Other`/`Gcb::Other` — over a guessed
inclusion that could misrender). If this module is ever regenerated
from the authoritative UCD (`EastAsianWidth.txt`, `GraphemeBreakProperty
.txt`, `emoji-data.txt`), replace the range tables in `src/core/text.cpp`
wholesale and re-run `tests/test_text_width.cpp`, which pins the
documented corpus byte-exactly and will catch any behavioral drift.
