// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Grapheme segmentation and column-width computation — the single,
// central, testable width policy (the architecture §2). Coverage and
// methodology: docs/text-width.md. Terminal-agreement strategy for the
// real world (D-019) is a term-layer capability, layered on top of this.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ckv::text {

// Returns the byte offset of the end of the grapheme cluster starting
// at `pos` (a boundary — 0 or a previous return value). `pos` must be
// < text.size(). The returned offset is in (pos, text.size()].
std::size_t grapheme_end(std::string_view text, std::size_t pos) noexcept;

// Splits `text` into grapheme clusters. Convenience wrapper over
// `grapheme_end` for call sites that are not on a hot path (tests,
// one-shot layout); Painter-facing code iterates with `grapheme_end`
// directly to stay allocation-free.
std::vector<std::string_view> split_graphemes(std::string_view text) noexcept;

// Column width (0, 1, or 2) of a single Unicode scalar value.
int codepoint_width(char32_t cp) noexcept;

// Column width of one already-segmented grapheme cluster: not simply
// the first codepoint's width — regional-indicator pairs (flags) and
// ZWJ emoji sequences resolve to width 2 as a unit, and a cluster that
// begins with an emoji modifier takes the modifier's own width (2)
// although the modifier alone is a zero-width Extend (D-084).
int grapheme_width(std::string_view grapheme) noexcept;

// Sums grapheme_width over every cluster in `text`.
int text_width(std::string_view text) noexcept;

// Returns the longest prefix of `text` that consists only of complete
// grapheme clusters and occupies no more than `max_columns` terminal cells.
// A non-positive width yields an empty string. This is the sole text-emitter
// clipping primitive: callers must never truncate display text by bytes.
std::string clip_to_width(std::string_view text, int max_columns);

// Returns `text` unchanged when it fits. Otherwise returns a complete-
// grapheme prefix followed by `marker` (U+2026 by default), constrained to
// `max_columns` cells. If even `marker` cannot fit, it is itself clipped.
std::string elide_to_width(std::string_view text, int max_columns,
                           std::string_view marker = "\xE2\x80\xA6");

// Replaces every C0/C1 control character (including CR/LF/Tab — a Cell
// holds one grapheme, never a line; multi-line layout splits on '\n'
// before this boundary) with U+FFFD, so hostile or accidental control
// bytes in application-supplied display text can never reach the
// presenter as control data (the decision log D-040). Malformed UTF-8 is
// itself replaced per codepoint (see utf8::decode).
std::string sanitize_display_text(std::string_view text);

// The same guarantee for text on its way to a clipboard rather than into a
// cell, where the difference is that a clipboard holds documents: tab and
// line break are content there, not control data, and a copy that lost them
// would paste back as one run-on line. CR and CRLF both become a single LF —
// a line break is a line break, and a bare CR left in clipboard text behaves
// as Enter when it is pasted into a shell. Every other C0/C1 control, and
// malformed UTF-8, is replaced with U+FFFD exactly as above.
std::string sanitize_clipboard_text(std::string_view text);

// Makes `text` safe to embed in an OSC command string: a window title or a
// hyperlink target (the architecture §12: every OSC emission escapes or rejects
// terminator bytes). Every C0 and C1 control code point, DEL included, is
// dropped (ESC, BEL, CAN, SUB and U+009C ST among them), and malformed UTF-8,
// a lone 0x9C included, becomes U+FFFD. The result is well-formed UTF-8 in
// which no 7-bit, 8-bit or UTF-8 spelling of a string terminator survives.
// docs/terminal-host-integration.md#osc-emission-safety lists every emitter.
std::string sanitize_osc_text(std::string_view text);

}  // namespace ckv::text
