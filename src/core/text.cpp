// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Unicode 15.1 extended-grapheme segmentation (UAX #29) and terminal-column
// width policy. The generated UCD property tables are the sole character-data
// authority; their provenance and regeneration are in docs/text-width.md.
#include "cvision/core/text.hpp"

#include <cstdint>

#include "cvision/core/assert.hpp"
#include "cvision/core/generated_unicode_15_1.hpp"
#include "cvision/core/utf8.hpp"

namespace ckv::text {
namespace {

using unicode_15_1::Range;

template <std::size_t N> bool in_ranges(char32_t cp, const Range (&ranges)[N]) noexcept {
    std::size_t left = 0;
    std::size_t right = N;
    while (left < right) {
        const std::size_t mid = left + (right - left) / 2;
        if (cp < ranges[mid].first)
            right = mid;
        else if (cp > ranges[mid].last)
            left = mid + 1;
        else
            return true;
    }
    return false;
}

enum class Gcb : std::uint8_t {
    Other,
    CR,
    LF,
    Control,
    Extend,
    ZWJ,
    RegionalIndicator,
    Prepend,
    SpacingMark,
    L,
    V,
    T,
    LV,
    LVT,
};

Gcb classify(char32_t cp) noexcept {
    using namespace unicode_15_1;
    if (in_ranges(cp, kCr))
        return Gcb::CR;
    if (in_ranges(cp, kLf))
        return Gcb::LF;
    if (in_ranges(cp, kControl))
        return Gcb::Control;
    if (in_ranges(cp, kExtend))
        return Gcb::Extend;
    if (in_ranges(cp, kZwj))
        return Gcb::ZWJ;
    if (in_ranges(cp, kRegionalIndicator))
        return Gcb::RegionalIndicator;
    if (in_ranges(cp, kPrepend))
        return Gcb::Prepend;
    if (in_ranges(cp, kSpacingmark))
        return Gcb::SpacingMark;
    if (in_ranges(cp, kL))
        return Gcb::L;
    if (in_ranges(cp, kV))
        return Gcb::V;
    if (in_ranges(cp, kT))
        return Gcb::T;
    if (in_ranges(cp, kLv))
        return Gcb::LV;
    if (in_ranges(cp, kLvt))
        return Gcb::LVT;
    return Gcb::Other;
}

bool is_extended_pictographic(char32_t cp) noexcept {
    return in_ranges(cp, unicode_15_1::kExtendedPictographic);
}

bool is_wide(char32_t cp) noexcept {
    return in_ranges(cp, unicode_15_1::kEastAsianWide);
}

// Emoji_Modifier (UTS #51): the five skin-tone modifiers, and nothing else.
bool is_emoji_modifier(char32_t cp) noexcept {
    return cp >= 0x1F3FB && cp <= 0x1F3FF;
}

bool is_incb_consonant(char32_t cp) noexcept {
    return in_ranges(cp, unicode_15_1::kIncbConsonant);
}

bool is_incb_linker(char32_t cp) noexcept {
    return in_ranges(cp, unicode_15_1::kIncbLinker);
}

bool is_incb_extend(char32_t cp) noexcept {
    return in_ranges(cp, unicode_15_1::kIncbExtend);
}

bool is_control_for_sanitization(char32_t cp) noexcept {
    return cp <= 0x1F || (cp >= 0x7F && cp <= 0x9F);
}

struct IncbRun {
    bool has_consonant = false;
    bool has_linker = false;

    void consume(char32_t cp) noexcept {
        if (is_incb_consonant(cp)) {
            has_consonant = true;
            has_linker = false;
        } else if (has_consonant && (is_incb_extend(cp) || is_incb_linker(cp))) {
            has_linker = has_linker || is_incb_linker(cp);
        } else {
            has_consonant = false;
            has_linker = false;
        }
    }

    [[nodiscard]] bool joins(char32_t cp) const noexcept {
        return has_consonant && has_linker && is_incb_consonant(cp);
    }
};

} // namespace

std::size_t grapheme_end(std::string_view text, std::size_t pos) noexcept {
    CKV_ASSERT(pos < text.size());

    // Two ASCII characters always have a boundary between them except CR LF
    // (GB3): no ASCII character is Extend, ZWJ, SpacingMark, Prepend, a
    // Regional Indicator, Hangul or an InCB consonant, so GB999 or GB4/GB5
    // decides. This is the common step of every walk over source text.
    const auto first = static_cast<unsigned char>(text[pos]);
    if (first < 0x80U) {
        if (pos + 1U == text.size()) return text.size();
        const auto second = static_cast<unsigned char>(text[pos + 1U]);
        if (second < 0x80U && !(first == '\r' && second == '\n')) return pos + 1U;
    }

    std::size_t cur = pos;
    const char32_t cp1 = utf8::decode(text, cur);
    Gcb g1 = classify(cp1);
    bool ri_seen_odd = g1 == Gcb::RegionalIndicator;

    // `ep_run` recognizes the UAX #29 GB11 left context
    // Extended_Pictographic Extend*. `zwj_valid` is a one-step snapshot so
    // that an unrelated second ZWJ cannot inherit the first one's context.
    bool ep_run = is_extended_pictographic(cp1);
    bool zwj_valid = false;
    IncbRun incb_run;
    incb_run.consume(cp1);

    while (cur < text.size()) {
        std::size_t next = cur;
        const char32_t cp2 = utf8::decode(text, next);
        const Gcb g2 = classify(cp2);
        const bool ep2 = is_extended_pictographic(cp2);

        bool do_break;
        if (g1 == Gcb::CR && g2 == Gcb::LF) {
            do_break = false; // GB3
        } else if (g1 == Gcb::Control || g1 == Gcb::CR || g1 == Gcb::LF) {
            do_break = true; // GB4
        } else if (g2 == Gcb::Control || g2 == Gcb::CR || g2 == Gcb::LF) {
            do_break = true; // GB5
        } else if (g1 == Gcb::L &&
                   (g2 == Gcb::L || g2 == Gcb::V || g2 == Gcb::LV || g2 == Gcb::LVT)) {
            do_break = false; // GB6
        } else if ((g1 == Gcb::LV || g1 == Gcb::V) && (g2 == Gcb::V || g2 == Gcb::T)) {
            do_break = false; // GB7
        } else if ((g1 == Gcb::LVT || g1 == Gcb::T) && g2 == Gcb::T) {
            do_break = false; // GB8
        } else if (g2 == Gcb::Extend || g2 == Gcb::ZWJ) {
            do_break = false; // GB9
        } else if (g2 == Gcb::SpacingMark) {
            do_break = false; // GB9a
        } else if (g1 == Gcb::Prepend) {
            do_break = false; // GB9b
        } else if (incb_run.joins(cp2)) {
            do_break = false; // GB9c
        } else if (g1 == Gcb::ZWJ && zwj_valid && ep2) {
            do_break = false; // GB11
        } else if (g1 == Gcb::RegionalIndicator && g2 == Gcb::RegionalIndicator && ri_seen_odd) {
            do_break = false; // GB12/GB13
        } else {
            do_break = true; // GB999
        }

        if (do_break)
            return cur;

        ri_seen_odd = (g2 == Gcb::RegionalIndicator) ? !ri_seen_odd : false;
        zwj_valid = (g2 == Gcb::ZWJ) ? ep_run : false;
        ep_run = ep2 || (ep_run && g2 == Gcb::Extend);
        incb_run.consume(cp2);
        g1 = g2;
        cur = next;
    }
    return text.size();
}

std::vector<std::string_view> split_graphemes(std::string_view text) noexcept {
    std::vector<std::string_view> result;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t end = grapheme_end(text, pos);
        result.push_back(text.substr(pos, end - pos));
        pos = end;
    }
    return result;
}

int codepoint_width(char32_t cp) noexcept {
    const Gcb g = classify(cp);
    if (g == Gcb::Control || g == Gcb::CR || g == Gcb::LF || g == Gcb::Extend || g == Gcb::ZWJ)
        return 0;
    return is_wide(cp) ? 2 : 1;
}

int grapheme_width(std::string_view grapheme) noexcept {
    if (grapheme.empty())
        return 0;

    std::size_t pos = 0;
    const char32_t first = utf8::decode(grapheme, pos);
    bool is_flag = false;
    if (classify(first) == Gcb::RegionalIndicator && pos < grapheme.size()) {
        const std::size_t before_second = pos;
        const char32_t second = utf8::decode(grapheme, pos);
        is_flag = classify(second) == Gcb::RegionalIndicator;
        if (!is_flag) pos = before_second;
    }

    bool has_vs16 = first == 0xFE0F;
    bool has_vs15 = first == 0xFE0E;
    bool has_zwj = first == 0x200D;
    bool has_extended_pictographic = is_extended_pictographic(first);
    while (pos < grapheme.size()) {
        const char32_t cp = utf8::decode(grapheme, pos);
        has_vs16 = has_vs16 || cp == 0xFE0F;
        has_vs15 = has_vs15 || cp == 0xFE0E;
        has_zwj = has_zwj || cp == 0x200D;
        has_extended_pictographic = has_extended_pictographic || is_extended_pictographic(cp);
    }

    if (has_vs16)
        return 2;
    if (has_vs15)
        return 1;
    if (is_flag)
        return 2;
    if (has_zwj && has_extended_pictographic)
        return 2;
    // A modifier is Extend, so it folds into a preceding base and adds no
    // column there. With no base it begins its own cluster, and a terminal
    // draws it as a swatch: the cluster takes the modifier's own
    // East_Asian_Width rather than the zero an Extend lead would give it
    // (D-084).
    if (is_emoji_modifier(first))
        return is_wide(first) ? 2 : 1;
    return codepoint_width(first);
}

int text_width(std::string_view text) noexcept {
    int total = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t end = grapheme_end(text, pos);
        total += grapheme_width(text.substr(pos, end - pos));
        pos = end;
    }
    return total;
}

std::string_view clip_to_width_view(std::string_view text, int max_columns) noexcept {
    if (max_columns <= 0)
        return {};

    int used = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t end = grapheme_end(text, pos);
        const int width = grapheme_width(text.substr(pos, end - pos));
        if (width > max_columns - used)
            break;
        used += width;
        pos = end;
    }
    return text.substr(0, pos);
}

std::string clip_to_width(std::string_view text, int max_columns) {
    return std::string(clip_to_width_view(text, max_columns));
}

std::string elide_to_width(std::string_view text, int max_columns, std::string_view marker) {
    if (max_columns <= 0)
        return {};
    if (text_width(text) <= max_columns)
        return std::string(text);

    const int marker_width = text_width(marker);
    if (marker_width <= 0 || marker_width > max_columns)
        return clip_to_width(marker, max_columns);
    std::string result = clip_to_width(text, max_columns - marker_width);
    result.append(marker);
    return result;
}

bool is_sanitized_display_text(std::string_view text) noexcept {
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        const char32_t cp = utf8::decode(text, pos);
        if (is_control_for_sanitization(cp)) return false;
        if (cp == utf8::replacement_char && text.substr(start, pos - start) != "\xEF\xBF\xBD") return false;
    }
    return true;
}

std::string sanitize_display_text(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        const char32_t cp = utf8::decode(text, pos);
        if (is_control_for_sanitization(cp) || cp == utf8::replacement_char) {
            out += "\xEF\xBF\xBD"; // U+FFFD
        } else {
            out.append(text.substr(start, pos - start));
        }
    }
    return out;
}

std::string sanitize_clipboard_text(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        const char32_t cp = utf8::decode(text, pos);
        if (cp == U'\n' || cp == U'\t') {
            out += static_cast<char>(cp);
        } else if (cp == U'\r') {
            // One line break, however it was spelled: a lone CR and the CR of
            // a CRLF pair both end a line and do nothing else.
            out += '\n';
            if (pos < text.size() && text[pos] == '\n') ++pos;
        } else if (is_control_for_sanitization(cp) || cp == utf8::replacement_char) {
            out += "\xEF\xBF\xBD"; // U+FFFD
        } else {
            out.append(text.substr(start, pos - start));
        }
    }
    return out;
}

std::string sanitize_osc_text(std::string_view text) {
    // Only text survives. ST ends an OSC — ESC \ in a 7-bit code, 09/12 in an
    // 8-bit one (ECMA-48 §8.3.143) — xterm also ends one at BEL, and CAN and
    // SUB cancel a sequence in progress. ECMA-48 §8.3.89 admits no other
    // control into a command string either, and a title is one line, so every
    // C0 and C1 control goes. A UTF-8 receiver meets C1 as the code points
    // U+0080..U+009F; a lone byte in 0x80..0x9F is malformed UTF-8 and becomes
    // U+FFFD, whose encoding holds no byte below 0xA0. No spelling of a
    // terminator gets through, and text whose encoding merely contains such a
    // byte as a continuation (U+201C is E2 80 9C) is kept whole.
    std::string out;
    out.reserve(text.size());
    for (std::size_t pos = 0; pos < text.size();) {
        const char32_t cp = utf8::decode(text, pos);
        if (is_control_for_sanitization(cp)) continue;
        utf8::encode(cp, out);
    }
    return out;
}

} // namespace ckv::text
