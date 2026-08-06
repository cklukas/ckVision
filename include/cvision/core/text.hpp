// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Grapheme segmentation and column-width computation — the single,
// central, testable width policy (ARCHITECTURE.md §2). Coverage and
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
// ZWJ emoji sequences resolve to width 2 as a unit.
int grapheme_width(std::string_view grapheme) noexcept;

// Sums grapheme_width over every cluster in `text`.
int text_width(std::string_view text) noexcept;

// Replaces every C0/C1 control character (including CR/LF/Tab — a Cell
// holds one grapheme, never a line; multi-line layout splits on '\n'
// before this boundary) with U+FFFD, so hostile or accidental control
// bytes in application-supplied display text can never reach the
// presenter as control data (DECISIONS.md D-023). Malformed UTF-8 is
// itself replaced per codepoint (see utf8::decode).
std::string sanitize_display_text(std::string_view text);

}  // namespace ckv::text
