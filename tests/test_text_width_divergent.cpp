// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The known terminal-divergent width classes (the decision log D-019). Every case
// in this suite is one class on which real terminals disagree with ckVision's
// compiled-in policy, or with each other, and pins the width ckVision chooses.
// The suite is the machine-enumerable marker: `cvision_tests --suite
// test_text_width_divergent.cpp --list` names every divergent class, and CTest
// runs it as suite_test_text_width_divergent with the `d019` label. The
// "Column width" section of docs/text-width.md lists the same cases with the
// same widths, and doc_widget_coverage fails when the two sets differ.
//
// A pinned width here is ckVision's layout truth, not a claim about any
// terminal; agreeing with a particular terminal is the term-layer capability
// D-019 describes. Changing one of these widths changes the documented policy.
#include "cvision/core/text.hpp"

#include <initializer_list>
#include <string>

#include "cvision/testing/cktest.hpp"

namespace {

// UTF-8 encodings, one scalar per constant, derived from the UTF-8 encoding
// form (The Unicode Standard, section 3.9).
constexpr const char* kInvertedExclamation = "\xC2\xA1";  // U+00A1, East_Asian_Width=A
constexpr const char* kGreekAlpha = "\xCE\xB1";           // U+03B1, East_Asian_Width=A
constexpr const char* kBoxHorizontal = "\xE2\x94\x80";    // U+2500, East_Asian_Width=A
constexpr const char* kHeavyHeart = "\xE2\x9D\xA4";       // U+2764, East_Asian_Width=N, Extended_Pictographic
constexpr const char* kWatch = "\xE2\x8C\x9A";            // U+231A, East_Asian_Width=W, Emoji_Presentation
constexpr const char* kVS15 = "\xEF\xB8\x8E";             // U+FE0E text presentation selector
constexpr const char* kVS16 = "\xEF\xB8\x8F";             // U+FE0F emoji presentation selector
constexpr const char* kZwj = "\xE2\x80\x8D";              // U+200D ZERO WIDTH JOINER
constexpr const char* kMan = "\xF0\x9F\x91\xA8";          // U+1F468
constexpr const char* kWoman = "\xF0\x9F\x91\xA9";        // U+1F469
constexpr const char* kGirl = "\xF0\x9F\x91\xA7";         // U+1F467
constexpr const char* kFire = "\xF0\x9F\x94\xA5";         // U+1F525
constexpr const char* kIndicatorU = "\xF0\x9F\x87\xBA";   // U+1F1FA REGIONAL INDICATOR SYMBOL LETTER U
constexpr const char* kIndicatorS = "\xF0\x9F\x87\xB8";   // U+1F1F8 REGIONAL INDICATOR SYMBOL LETTER S
constexpr const char* kThumbsUp = "\xF0\x9F\x91\x8D";     // U+1F44D
constexpr const char* kMediumSkin = "\xF0\x9F\x8F\xBD";   // U+1F3FD, Emoji_Modifier (Grapheme_Cluster_Break=Extend)
constexpr const char* kKeycap = "\xE2\x83\xA3";           // U+20E3 COMBINING ENCLOSING KEYCAP
constexpr const char* kCombiningAcute = "\xCC\x81";       // U+0301
constexpr const char* kDevanagariKa = "\xE0\xA4\x95";     // U+0915
constexpr const char* kDevanagariAa = "\xE0\xA4\xBE";     // U+093E, Grapheme_Cluster_Break=SpacingMark
constexpr const char* kJamoL = "\xE1\x84\x80";            // U+1100 HANGUL CHOSEONG KIYEOK, East_Asian_Width=W
constexpr const char* kJamoV = "\xE1\x85\xA1";            // U+1161 HANGUL JUNGSEONG A, East_Asian_Width=N
constexpr const char* kJamoT = "\xE1\x86\xA8";            // U+11A8 HANGUL JONGSEONG KIYEOK, East_Asian_Width=N

std::string cat(std::initializer_list<const char*> parts) {
    std::string out;
    for (const char* part : parts) out += part;
    return out;
}

// The one grapheme `text` must be, and the width ckVision gives it.
bool is_one_cluster_of_width(const std::string& text, int width) {
    return ckv::text::split_graphemes(text).size() == 1 && ckv::text::grapheme_width(text) == width &&
           ckv::text::text_width(text) == width;
}

}  // namespace

// --- East-Asian-Ambiguous ----------------------------------------------------
// East_Asian_Width=A is one column by default (D-019). Terminals configured
// for a CJK locale draw these two columns wide.

CK_TEST(east_asian_ambiguous_punctuation_is_narrow) {
    CK_CHECK(ckv::text::codepoint_width(0x00A1) == 1);
    CK_CHECK(is_one_cluster_of_width(kInvertedExclamation, 1));
}

CK_TEST(east_asian_ambiguous_greek_letter_is_narrow) {
    CK_CHECK(ckv::text::codepoint_width(0x03B1) == 1);
    CK_CHECK(is_one_cluster_of_width(kGreekAlpha, 1));
}

CK_TEST(east_asian_ambiguous_box_drawing_is_narrow) {
    // Every frame ckVision draws is made of these; a terminal that draws them
    // wide breaks every frame, not one run.
    CK_CHECK(ckv::text::codepoint_width(0x2500) == 1);
    CK_CHECK(is_one_cluster_of_width(kBoxHorizontal, 1));
}

// --- Presentation ------------------------------------------------------------

CK_TEST(bare_neutral_pictographic_symbol_is_narrow) {
    // U+2764 alone is East_Asian_Width=N with text presentation by default.
    // Some terminals draw every Extended_Pictographic symbol wide.
    CK_CHECK(ckv::text::codepoint_width(0x2764) == 1);
    CK_CHECK(is_one_cluster_of_width(kHeavyHeart, 1));
}

CK_TEST(emoji_presentation_selector_widens_a_narrow_symbol) {
    // VS16 is Extend, so it joins the symbol's cluster (GB9) and asks for
    // emoji presentation. Terminals that size by code point keep one column.
    CK_CHECK(is_one_cluster_of_width(cat({kHeavyHeart, kVS16}), 2));
}

CK_TEST(default_emoji_presentation_symbol_is_wide) {
    // U+231A is East_Asian_Width=W since Unicode 9.0; terminals on older
    // width tables draw it one column.
    CK_CHECK(ckv::text::codepoint_width(0x231A) == 2);
    CK_CHECK(is_one_cluster_of_width(kWatch, 2));
}

CK_TEST(text_presentation_selector_narrows_a_wide_emoji) {
    // VS15 asks for text presentation; many terminals keep the emoji glyph
    // and its two columns.
    CK_CHECK(is_one_cluster_of_width(cat({kWatch, kVS15}), 1));
}

CK_TEST(text_presentation_selector_keeps_a_narrow_symbol_narrow) {
    CK_CHECK(is_one_cluster_of_width(cat({kHeavyHeart, kVS15}), 1));
}

CK_TEST(keycap_sequence_is_wide) {
    // Digit + VS16 + U+20E3 is one cluster; terminals sizing by code point
    // count the digit's single column.
    CK_CHECK(is_one_cluster_of_width(cat({"1", kVS16, kKeycap}), 2));
}

// --- ZWJ sequences -----------------------------------------------------------

CK_TEST(zwj_family_sequence_is_one_wide_cluster) {
    // GB11 keeps the sequence together. A terminal without the ligature draws
    // three people side by side: six columns.
    CK_CHECK(is_one_cluster_of_width(cat({kMan, kZwj, kWoman, kZwj, kGirl}), 2));
}

CK_TEST(zwj_sequence_led_by_a_narrow_symbol_is_wide) {
    // HEART ON FIRE: the lead is narrow on its own, but any ZWJ sequence with
    // an Extended_Pictographic member is two columns.
    CK_CHECK(is_one_cluster_of_width(cat({kHeavyHeart, kZwj, kFire}), 2));
}

// --- Regional indicators -----------------------------------------------------

CK_TEST(regional_indicator_pair_is_one_wide_flag) {
    // Terminals without flag support draw two letter symbols: four columns.
    CK_CHECK(is_one_cluster_of_width(cat({kIndicatorU, kIndicatorS}), 2));
}

CK_TEST(unpaired_regional_indicator_is_narrow) {
    // East_Asian_Width=N. Terminals draw a lone indicator as a boxed letter
    // one or two columns wide.
    CK_CHECK(ckv::text::codepoint_width(0x1F1FA) == 1);
    CK_CHECK(is_one_cluster_of_width(kIndicatorU, 1));
}

CK_TEST(unpaired_regional_indicator_with_emoji_selector_is_wide) {
    CK_CHECK(is_one_cluster_of_width(cat({kIndicatorU, kVS16}), 2));
}

CK_TEST(regional_indicator_pair_with_trailing_combining_mark_stays_wide) {
    CK_CHECK(is_one_cluster_of_width(cat({kIndicatorU, kIndicatorS, kCombiningAcute}), 2));
}

// --- Emoji modifiers ---------------------------------------------------------

CK_TEST(skin_tone_modifier_folds_into_a_wide_base) {
    // A terminal without modifier support draws the base and a colour swatch:
    // four columns.
    CK_CHECK(is_one_cluster_of_width(cat({kThumbsUp, kMediumSkin}), 2));
}

CK_TEST(lone_skin_tone_modifier_is_a_wide_swatch) {
    // Emoji_Modifier code points are Grapheme_Cluster_Break=Extend: after a
    // base they add no column, so the code point alone stays zero wide. With
    // no base the modifier begins its own cluster, which takes the
    // modifier's East_Asian_Width W, two columns, as terminals draw the
    // swatch (D-084). A terminal without emoji fonts may draw a one-column
    // replacement.
    CK_CHECK(ckv::text::codepoint_width(0x1F3FD) == 0);
    CK_CHECK(is_one_cluster_of_width(kMediumSkin, 2));
    // Text after the swatch starts two columns on, not on top of it.
    CK_CHECK(ckv::text::text_width(cat({kMediumSkin, "a"})) == 3);
}

// --- Cluster width from the lead code point ----------------------------------
// Outside the emoji rules above, a cluster is as wide as its first code point.
// Terminals that add the widths of the cluster's code points disagree.

CK_TEST(spacing_mark_cluster_takes_its_base_width) {
    // KA + vowel sign AA: the SpacingMark has its own advance in a proportional
    // font, and terminals summing code points give two columns.
    CK_CHECK(ckv::text::codepoint_width(0x093E) == 1);
    CK_CHECK(is_one_cluster_of_width(cat({kDevanagariKa, kDevanagariAa}), 1));
}

CK_TEST(decomposed_hangul_syllable_takes_its_leading_jamo_width) {
    // L (wide) + V + T (neutral): two columns, like the precomposed syllable.
    // Terminals that add the vowel and final jamo draw up to four.
    CK_CHECK(ckv::text::codepoint_width(0x1161) == 1);
    CK_CHECK(is_one_cluster_of_width(cat({kJamoL, kJamoV, kJamoT}), 2));
}
