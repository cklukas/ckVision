// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Grapheme segmentation (UAX #29 core rules, curated scope) and column
// width (East Asian Width + emoji presentation policy). Table coverage
// and every simplification are documented in docs/text-width.md — read
// that file before changing anything here.
#include "cvision/core/text.hpp"

#include <cstdint>

#include "cvision/core/assert.hpp"
#include "cvision/core/utf8.hpp"

namespace ckv::text {
namespace {

struct Range {
    char32_t lo;
    char32_t hi;
};

bool in_ranges(char32_t cp, const Range* ranges, std::size_t count) noexcept {
    std::size_t left = 0;
    std::size_t right = count;
    while (left < right) {
        const std::size_t mid = left + (right - left) / 2;
        if (cp < ranges[mid].lo)
            right = mid;
        else if (cp > ranges[mid].hi)
            left = mid + 1;
        else
            return true;
    }
    return false;
}

// Grapheme_Cluster_Break=Control, minus CR/LF (their own categories):
// C0 controls, DEL, C1 controls, line/paragraph separators, and a
// curated set of zero-width format characters (docs/text-width.md).
constexpr Range kControlRanges[] = {
    {0x0000, 0x0009}, {0x000B, 0x001F}, {0x007F, 0x009F}, {0x200B, 0x200B},
    {0x200E, 0x200F}, {0x2028, 0x2029}, {0x2060, 0x2064}, {0x2066, 0x206F},
    {0xFEFF, 0xFEFF},
};

// Grapheme_Cluster_Break=Extend: combining marks (Latin/Cyrillic/Greek
// diacritics + symbol-combining blocks), ZWNJ, variation selectors, and
// emoji skin-tone modifiers. Indic/Arabic/Southeast-Asian combining
// marks are a known v1 gap — docs/text-width.md.
constexpr Range kExtendRanges[] = {
    {0x0300, 0x036F}, {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0x200C, 0x200C},
    {0x20D0, 0x20FF}, {0xFE00, 0xFE0F}, {0x1F3FB, 0x1F3FF}, {0xE0100, 0xE01EF},
};

// Grapheme_Cluster_Break=Prepend: a curated, high-confidence subset.
// Affects no case in the M1 acceptance corpus; included for GB9b
// correctness on the text that does use it.
constexpr Range kPrependRanges[] = {
    {0x0600, 0x0605}, {0x06DD, 0x06DD}, {0x070F, 0x070F},
    {0x08E2, 0x08E2}, {0x110BD, 0x110BD}, {0x110CD, 0x110CD},
};

// Extended_Pictographic (for GB11 emoji-ZWJ clustering only — not a
// GCB primary category, a codepoint can be Other and EP at once).
// Broad emoji-block approximation excluding the Regional Indicator
// block (classified separately) and the skin-tone-modifier range
// (already Extend); some non-emoji codepoints inside these blocks are
// over-included, which only ever prevents an incorrect break at a ZWJ
// boundary — docs/text-width.md.
constexpr Range kExtendedPictographicRanges[] = {
    {0x203C, 0x203C}, {0x2049, 0x2049}, {0x2122, 0x2122}, {0x2139, 0x2139},
    {0x2194, 0x21AA}, {0x231A, 0x231B}, {0x2328, 0x2328}, {0x23CF, 0x23CF},
    {0x23E9, 0x23FA}, {0x24C2, 0x24C2}, {0x25AA, 0x25FE}, {0x2600, 0x27BF},
    {0x2934, 0x2935}, {0x2B00, 0x2BFF}, {0x3030, 0x3030}, {0x303D, 0x303D},
    {0x3297, 0x3297}, {0x3299, 0x3299}, {0x1F000, 0x1F1E5}, {0x1F200, 0x1F3FA},
    {0x1F400, 0x1FFFF},
};

// East Asian Width = Wide or Fullwidth: the primary CJK/Hangul/Kana
// blocks, fullwidth forms, and the emoji planes that render wide on
// every mainstream terminal. Rarer wide scripts (Tangut, Nüshu, …) and
// the East-Asian-Ambiguous class (default policy: narrow, D-019) are
// out of v1 scope — docs/text-width.md.
constexpr Range kWideRanges[] = {
    {0x1100, 0x115F}, {0x2E80, 0x303E}, {0x3041, 0x33FF}, {0x3400, 0x4DBF},
    {0x4E00, 0x9FFF}, {0xA000, 0xA4CF}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF},
    {0xFE30, 0xFE4F}, {0xFF00, 0xFF60}, {0xFFE0, 0xFFE6}, {0x1F300, 0x1F64F},
    {0x1F680, 0x1F6FF}, {0x1F900, 0x1F9FF}, {0x1FA70, 0x1FAFF},
    {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD},
};

enum class Gcb : std::uint8_t {
    Other,
    CR,
    LF,
    Control,
    Extend,
    ZWJ,
    RegionalIndicator,
    Prepend,
    L,
    V,
    T,
    LV,
    LVT,
};

bool is_regional_indicator(char32_t cp) noexcept { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }

bool is_extended_pictographic(char32_t cp) noexcept {
    return in_ranges(cp, kExtendedPictographicRanges,
                      sizeof(kExtendedPictographicRanges) / sizeof(Range));
}

bool is_wide(char32_t cp) noexcept {
    return in_ranges(cp, kWideRanges, sizeof(kWideRanges) / sizeof(Range));
}

Gcb classify(char32_t cp) noexcept {
    if (cp == 0x000D) return Gcb::CR;
    if (cp == 0x000A) return Gcb::LF;
    if (cp == 0x200D) return Gcb::ZWJ;
    if (is_regional_indicator(cp)) return Gcb::RegionalIndicator;

    // Hangul jamo: algorithmic, exact (Unicode's own defining formula).
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0xA960 && cp <= 0xA97F)) return Gcb::L;
    if ((cp >= 0x1160 && cp <= 0x11A7) || (cp >= 0xD7B0 && cp <= 0xD7C6)) return Gcb::V;
    if ((cp >= 0x11A8 && cp <= 0x11FF) || (cp >= 0xD7CB && cp <= 0xD7FF)) return Gcb::T;
    if (cp >= 0xAC00 && cp <= 0xD7A3)
        return ((cp - 0xAC00) % 28 == 0) ? Gcb::LV : Gcb::LVT;

    if (in_ranges(cp, kControlRanges, sizeof(kControlRanges) / sizeof(Range))) return Gcb::Control;
    if (in_ranges(cp, kExtendRanges, sizeof(kExtendRanges) / sizeof(Range))) return Gcb::Extend;
    if (in_ranges(cp, kPrependRanges, sizeof(kPrependRanges) / sizeof(Range))) return Gcb::Prepend;
    return Gcb::Other;
}

bool is_control_for_sanitization(char32_t cp) noexcept {
    return cp <= 0x1F || (cp >= 0x7F && cp <= 0x9F);
}

}  // namespace

std::size_t grapheme_end(std::string_view text, std::size_t pos) noexcept {
    CKV_ASSERT(pos < text.size());

    std::size_t cur = pos;
    const char32_t cp1 = utf8::decode(text, cur);
    Gcb g1 = classify(cp1);
    bool ri_seen_odd = (g1 == Gcb::RegionalIndicator);
    // ep_run, evaluated at the CURRENT g1, means "the run ending at g1
    // (inclusive) matches Extended_Pictographic Extend*" — it must NOT
    // pass through a ZWJ (a ZWJ is neither EP nor Extend, so a ZWJ
    // breaks that specific pattern). zwj_valid is a one-step snapshot
    // taken exactly when g2 becomes a ZWJ, capturing "was *this* ZWJ
    // itself preceded by a valid EP Extend* run" — that is what GB11
    // ("EP Extend* ZWJ x EP") actually needs to test once the ZWJ
    // becomes g1 on the next iteration. Collapsing these into one
    // variable (an earlier version of this function did) lets ep_run
    // leak through a ZWJ, which wrongly keeps a *second*, unrelated ZWJ
    // "GB11-eligible" in chains like EP ZWJ ZWJ EP.
    bool ep_run = is_extended_pictographic(cp1);
    bool zwj_valid = false;

    while (cur < text.size()) {
        std::size_t next = cur;
        const char32_t cp2 = utf8::decode(text, next);
        const Gcb g2 = classify(cp2);
        const bool ep2 = is_extended_pictographic(cp2);

        bool do_break;
        if (g1 == Gcb::CR && g2 == Gcb::LF) {
            do_break = false;  // GB3
        } else if (g1 == Gcb::Control || g1 == Gcb::CR || g1 == Gcb::LF) {
            do_break = true;  // GB4
        } else if (g2 == Gcb::Control || g2 == Gcb::CR || g2 == Gcb::LF) {
            do_break = true;  // GB5
        } else if (g1 == Gcb::L && (g2 == Gcb::L || g2 == Gcb::V || g2 == Gcb::LV ||
                                     g2 == Gcb::LVT)) {
            do_break = false;  // GB6
        } else if ((g1 == Gcb::LV || g1 == Gcb::V) && (g2 == Gcb::V || g2 == Gcb::T)) {
            do_break = false;  // GB7
        } else if ((g1 == Gcb::LVT || g1 == Gcb::T) && g2 == Gcb::T) {
            do_break = false;  // GB8
        } else if (g2 == Gcb::Extend || g2 == Gcb::ZWJ) {
            do_break = false;  // GB9 (GB9a SpacingMark is out of v1 scope)
        } else if (g1 == Gcb::Prepend) {
            do_break = false;  // GB9b
        } else if (g1 == Gcb::ZWJ && zwj_valid && ep2) {
            do_break = false;  // GB11
        } else if (g1 == Gcb::RegionalIndicator && g2 == Gcb::RegionalIndicator &&
                   ri_seen_odd) {
            do_break = false;  // GB12/GB13
        } else {
            do_break = true;  // GB999
        }

        if (do_break) return cur;

        ri_seen_odd = (g2 == Gcb::RegionalIndicator) ? !ri_seen_odd : false;
        zwj_valid = (g2 == Gcb::ZWJ) ? ep_run : false;
        ep_run = ep2 || (ep_run && g2 == Gcb::Extend);
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
    if (grapheme.empty()) return 0;

    std::size_t pos = 0;
    const char32_t first = utf8::decode(grapheme, pos);

    // A flag is an RI pair possibly followed by trailing Extend/ZWJ
    // codepoints (GB9 legitimately attaches those to the cluster, e.g.
    // a combining mark or skin-tone modifier after a flag) — so the
    // check is "the first two codepoints are both RI", not "every
    // codepoint in the cluster is RI and there are exactly two".
    bool is_flag = false;
    if (is_regional_indicator(first) && pos < grapheme.size()) {
        const std::size_t before_second = pos;
        const char32_t second = utf8::decode(grapheme, pos);
        if (is_regional_indicator(second)) {
            is_flag = true;
        } else {
            pos = before_second;  // not a pair; let the scan below see it
        }
    }

    bool has_vs16 = (first == 0xFE0F);
    bool has_vs15 = (first == 0xFE0E);
    bool has_zwj = (first == 0x200D);
    while (pos < grapheme.size()) {
        const char32_t cp = utf8::decode(grapheme, pos);
        has_vs16 = has_vs16 || (cp == 0xFE0F);
        has_vs15 = has_vs15 || (cp == 0xFE0E);
        has_zwj = has_zwj || (cp == 0x200D);
    }

    if (has_vs16) return 2;  // emoji presentation selector forces wide
    if (has_vs15) return 1;  // text presentation selector forces narrow
    if (is_flag) return 2;   // flag: a leading regional-indicator pair
    if (has_zwj) return 2;   // emoji ZWJ sequence
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

std::string sanitize_display_text(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        const char32_t cp = utf8::decode(text, pos);
        if (is_control_for_sanitization(cp) || cp == utf8::replacement_char) {
            out += "\xEF\xBF\xBD";  // U+FFFD
        } else {
            out.append(text.substr(start, pos - start));
        }
    }
    return out;
}

}  // namespace ckv::text
