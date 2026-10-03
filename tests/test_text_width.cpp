// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The M1 acceptance corpus (the roadmap M1 exit criteria): ASCII, East
// Asian wide, combining marks, zero-width joins, and emoji sequences.
// The terminal-divergent classes D-019 exists for are marked by their own
// suite, tests/test_text_width_divergent.cpp. Coverage rationale for every
// table entry used here: docs/text-width.md.
#include "cvision/core/text.hpp"

#include <string>

#include "cvision/testing/cktest.hpp"

namespace {

// UTF-8 byte sequences for the corpus, hand-verified against the UTF-8
// encoding algorithm (see the commit that introduced this file).
constexpr const char* kEAcute = "e\xCC\x81";                          // e + combining acute
constexpr const char* kZhong = "\xE4\xB8\xAD";                        // 中 U+4E2D (wide)
constexpr const char* kWen = "\xE6\x96\x87";                          // 文 U+6587 (wide)
constexpr const char* kManZwjWomanZwjGirl =                           // family emoji: 3 EP + 2 ZWJ
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
constexpr const char* kFlagUS = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";    // U+1F1FA U+1F1F8
constexpr const char* kFlagFR = "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7";    // U+1F1EB U+1F1F7
constexpr const char* kThumbsUpMediumSkin =                           // U+1F44D + U+1F3FD
    "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD";
constexpr const char* kHangulLVT =                                    // L + V + T, decomposed
    "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8";
constexpr const char* kHangulSyllable = "\xEA\xB0\x80";                // 가 U+AC00, precomposed

std::string cat(std::initializer_list<const char*> parts) {
    std::string out;
    for (const char* p : parts) out += p;
    return out;
}

}  // namespace

// --- ASCII -------------------------------------------------------------

CK_TEST(ascii_each_char_is_its_own_cluster_width_one) {
    const auto graphemes = ckv::text::split_graphemes("Hello");
    CK_CHECK(graphemes.size() == 5);
    for (const auto& g : graphemes) CK_CHECK(ckv::text::grapheme_width(g) == 1);
    CK_CHECK(ckv::text::text_width("Hello") == 5);
}

CK_TEST(every_ascii_pair_but_cr_lf_is_two_clusters_and_a_following_mark_joins_the_second) {
    // Every ordered pair of ASCII characters, NUL and DEL included, splits in
    // two except CR LF (GB3). A combining acute after the pair still joins its
    // second character (GB9) unless that character is a control (GB5).
    for (int first = 0; first < 0x80; ++first) {
        for (int second = 0; second < 0x80; ++second) {
            const std::string pair{static_cast<char>(first), static_cast<char>(second)};
            const std::size_t expected = first == '\r' && second == '\n' ? 2U : 1U;
            CK_CHECK(ckv::text::grapheme_end(pair, 0) == expected);
            const std::string marked = pair + "\xCC\x81";
            const bool second_is_control = second < 0x20 || second == 0x7F;
            if (expected == 1U)
                CK_CHECK(ckv::text::grapheme_end(marked, 1) == (second_is_control ? 2U : 4U));
        }
        const std::string alone{static_cast<char>(first)};
        CK_CHECK(ckv::text::grapheme_end(alone, 0) == 1U);
    }
}

CK_TEST(empty_text_has_zero_width_and_no_clusters) {
    CK_CHECK(ckv::text::text_width("") == 0);
    CK_CHECK(ckv::text::split_graphemes("").empty());
}

// --- East Asian wide -----------------------------------------------------

CK_TEST(cjk_ideographs_are_wide) {
    const std::string zhongwen = cat({kZhong, kWen});
    const auto graphemes = ckv::text::split_graphemes(zhongwen);
    CK_CHECK(graphemes.size() == 2);
    CK_CHECK(ckv::text::grapheme_width(graphemes[0]) == 2);
    CK_CHECK(ckv::text::grapheme_width(graphemes[1]) == 2);
    CK_CHECK(ckv::text::text_width(zhongwen) == 4);
}

CK_TEST(precomposed_hangul_syllable_is_wide) {
    CK_CHECK(ckv::text::split_graphemes(kHangulSyllable).size() == 1);
    CK_CHECK(ckv::text::text_width(kHangulSyllable) == 2);
}

// --- Combining marks -----------------------------------------------------

CK_TEST(combining_acute_accent_folds_into_base_cluster) {
    // "e" + U+0301 is ONE grapheme cluster (GB9: x Extend), not two, and
    // its width is the base letter's width, not 1+1.
    const auto graphemes = ckv::text::split_graphemes(kEAcute);
    CK_CHECK(graphemes.size() == 1);
    CK_CHECK(graphemes[0] == kEAcute);
    CK_CHECK(ckv::text::grapheme_width(kEAcute) == 1);
}

CK_TEST(precomposed_vs_decomposed_forms_agree_on_total_width) {
    // "café" precomposed (single U+00E9) vs decomposed ("cafe" + combining
    // acute): segmentation is not normalization-aware and doesn't need to
    // be — both forms must still report the same total width.
    const std::string precomposed = "caf\xC3\xA9";  // U+00E9 LATIN SMALL LETTER E WITH ACUTE
    const std::string decomposed = cat({"c", "a", "f", kEAcute});
    CK_CHECK(ckv::text::text_width(precomposed) == 4);
    CK_CHECK(ckv::text::text_width(decomposed) == 4);
    CK_CHECK(ckv::text::split_graphemes(decomposed).size() == 4);
}

// --- Zero-width and control characters ------------------------------------

CK_TEST(zero_width_joiner_alone_has_zero_codepoint_width) {
    CK_CHECK(ckv::text::codepoint_width(0x200D) == 0);
}

CK_TEST(control_character_is_its_own_zero_width_cluster) {
    const std::string text = cat({"A", "\x01", "B"});  // A, SOH, B
    const auto graphemes = ckv::text::split_graphemes(text);
    CK_CHECK(graphemes.size() == 3);  // Control breaks on both sides (GB4/GB5)
    CK_CHECK(ckv::text::grapheme_width(graphemes[1]) == 0);
    CK_CHECK(ckv::text::text_width(text) == 2);  // A(1) + control(0) + B(1)
}

CK_TEST(cr_lf_is_a_single_cluster) {
    const auto graphemes = ckv::text::split_graphemes("\r\n");
    CK_CHECK(graphemes.size() == 1);  // GB3
    CK_CHECK(graphemes[0] == "\r\n");
}

// --- Emoji sequences -------------------------------------------------------

CK_TEST(zwj_family_emoji_is_one_wide_cluster) {
    const auto graphemes = ckv::text::split_graphemes(kManZwjWomanZwjGirl);
    CK_CHECK(graphemes.size() == 1);  // GB11 keeps the whole ZWJ chain together
    CK_CHECK(graphemes[0] == kManZwjWomanZwjGirl);
    CK_CHECK(ckv::text::grapheme_width(kManZwjWomanZwjGirl) == 2);
}

CK_TEST(regional_indicator_pair_is_one_flag_cluster) {
    CK_CHECK(ckv::text::split_graphemes(kFlagUS).size() == 1);
    CK_CHECK(ckv::text::grapheme_width(kFlagUS) == 2);
}

CK_TEST(regional_indicator_pair_with_trailing_combining_mark_is_still_wide) {
    // GB9 legitimately attaches a trailing Extend codepoint to an RI
    // pair (e.g. a stray combining mark after a flag); the cluster is
    // still fundamentally a flag and must still report width 2 — width
    // must not be keyed on "exactly 2 codepoints, all RI".
    const std::string flag_plus_combining = cat({kFlagUS, "\xCC\x81"});  // + combining acute
    const auto graphemes = ckv::text::split_graphemes(flag_plus_combining);
    CK_CHECK(graphemes.size() == 1);
    CK_CHECK(ckv::text::grapheme_width(flag_plus_combining) == 2);
}

CK_TEST(four_regional_indicators_form_two_flags_not_one) {
    // GB12/13: RI pairs up, then breaks — this must not merge into a
    // single 4-codepoint cluster.
    const std::string two_flags = cat({kFlagUS, kFlagFR});
    const auto graphemes = ckv::text::split_graphemes(two_flags);
    CK_CHECK(graphemes.size() == 2);
    CK_CHECK(graphemes[0] == kFlagUS);
    CK_CHECK(graphemes[1] == kFlagFR);
    CK_CHECK(ckv::text::text_width(two_flags) == 4);
}

CK_TEST(emoji_skin_tone_modifier_folds_into_base_cluster) {
    const auto graphemes = ckv::text::split_graphemes(kThumbsUpMediumSkin);
    CK_CHECK(graphemes.size() == 1);  // GB9: skin-tone modifier is Extend
    CK_CHECK(ckv::text::grapheme_width(kThumbsUpMediumSkin) == 2);
}

CK_TEST(hangul_decomposed_jamo_sequence_is_one_wide_cluster) {
    const auto graphemes = ckv::text::split_graphemes(kHangulLVT);
    CK_CHECK(graphemes.size() == 1);  // GB6/GB7/GB8
    CK_CHECK(ckv::text::grapheme_width(kHangulLVT) == 2);
}

CK_TEST(a_second_unrelated_zwj_does_not_inherit_the_first_zwjs_gb11_eligibility) {
    // GB11 requires "EP Extend* ZWJ x EP" — the Extend* run is strictly
    // between the anchoring EP and the ZWJ that triggers the exception.
    // MAN ZWJ ZWJ WOMAN: the first ZWJ is validly EP-anchored (glues to
    // MAN via ordinary GB9, same as any Extend/ZWJ), but the SECOND ZWJ
    // is preceded only by the first ZWJ — not by EP Extend* — so GB11
    // does not grant an exception at the ZWJ-WOMAN boundary, and the two
    // must split into separate clusters.
    const std::string man = "\xF0\x9F\x91\xA8";
    const std::string zwj = "\xE2\x80\x8D";
    const std::string woman = "\xF0\x9F\x91\xA9";
    const std::string text = cat({man.c_str(), zwj.c_str(), zwj.c_str(), woman.c_str()});
    const auto graphemes = ckv::text::split_graphemes(text);
    CK_CHECK(graphemes.size() == 2);
    CK_CHECK(graphemes[0] == man + zwj + zwj);  // MAN+ZWJ+ZWJ glued by plain GB9
    CK_CHECK(graphemes[1] == woman);
}

// --- Grapheme-safe clipping --------------------------------------------------

CK_TEST(clip_to_width_never_splits_a_combining_grapheme_or_a_wide_cell) {
    const std::string text = cat({"A", kEAcute, kZhong, "B"});
    CK_CHECK(ckv::text::clip_to_width(text, 2) == cat({"A", kEAcute}));
    CK_CHECK(ckv::text::clip_to_width(text, 3) == cat({"A", kEAcute}));
    CK_CHECK(ckv::text::clip_to_width(text, 4) == cat({"A", kEAcute, kZhong}));
    CK_CHECK(ckv::text::text_width(ckv::text::clip_to_width(text, 4)) == 4);
}

CK_TEST(clip_to_width_keeps_emoji_sequences_atomic) {
    const std::string text = cat({"A", kManZwjWomanZwjGirl, "B"});
    CK_CHECK(ckv::text::clip_to_width(text, 2) == "A");
    CK_CHECK(ckv::text::clip_to_width(text, 3) == cat({"A", kManZwjWomanZwjGirl}));
    CK_CHECK(ckv::text::clip_to_width(text, 0).empty());
}

CK_TEST(full_unicode_properties_cover_script_marks_indic_conjuncts_and_rare_wide_characters) {
    const std::string devanagari_ka_aa = "\xE0\xA4\x95\xE0\xA4\xBE";  // KA + vowel sign AA (SpacingMark)
    const std::string hebrew_alef_sheva = "\xD7\x90\xD6\xB0";          // ALEF + SHEVA (Extend)
    const std::string arabic_alef_fatha = "\xD8\xA7\xD9\x8E";          // ALEF + FATHA (Extend)
    const std::string thai_ko_mai_tho = "\xE0\xB8\x81\xE0\xB9\x89";  // KO KAI + MAI THO (Extend)
    const std::string devanagari_conjunct = "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\x95";
    const std::string tangut = "\xF0\x97\x80\x80";  // U+17000, East_Asian_Width=W

    CK_CHECK(ckv::text::split_graphemes(devanagari_ka_aa).size() == 1);
    CK_CHECK(ckv::text::split_graphemes(hebrew_alef_sheva).size() == 1);
    CK_CHECK(ckv::text::split_graphemes(arabic_alef_fatha).size() == 1);
    CK_CHECK(ckv::text::split_graphemes(thai_ko_mai_tho).size() == 1);
    CK_CHECK(ckv::text::split_graphemes(devanagari_conjunct).size() == 1);
    CK_CHECK(ckv::text::codepoint_width(0x17000) == 2);
    CK_CHECK(ckv::text::codepoint_width(0x00A1) == 1);  // East-Asian-Ambiguous remains narrow by policy.
}

CK_TEST(elide_to_width_reserves_cells_for_a_complete_marker) {
    const std::string text = cat({"A", kEAcute, kZhong, "B"});
    CK_CHECK(ckv::text::elide_to_width(text, 3) == cat({"A", kEAcute, "\xE2\x80\xA6"}));
    CK_CHECK(ckv::text::text_width(ckv::text::elide_to_width(text, 3)) == 3);
    CK_CHECK(ckv::text::elide_to_width(text, 1) == "\xE2\x80\xA6");
}

CK_TEST(clipping_malformed_utf8_is_bounded_and_does_not_crash) {
    const std::string malformed = cat({"A", "\xE4\xB8", "B"});
    CK_CHECK(ckv::text::clip_to_width(malformed, 1) == "A");
    CK_CHECK(ckv::text::text_width(ckv::text::clip_to_width(malformed, 2)) == 2);
}

// --- Sanitization ------------------------------------------------------------

CK_TEST(sanitize_replaces_c0_and_c1_controls) {
    const std::string text = cat({"A", "\x07", "B", "\x1B", "C"});  // BEL, ESC
    const std::string sanitized = ckv::text::sanitize_display_text(text);
    CK_CHECK(sanitized == cat({"A", "\xEF\xBF\xBD", "B", "\xEF\xBF\xBD", "C"}));
}

CK_TEST(sanitize_replaces_tab_and_newline_too) {
    // Unlike paste sanitization (D-040 exempts tab/newline there), the
    // Cell-boundary rule has no exception: a Cell holds one grapheme, not
    // a line.
    const std::string sanitized = ckv::text::sanitize_display_text("a\tb\nc");
    CK_CHECK(sanitized == "a\xEF\xBF\xBD" "b\xEF\xBF\xBD" "c");
}

CK_TEST(sanitize_replaces_malformed_utf8) {
    const std::string sanitized = ckv::text::sanitize_display_text("A\x80z");
    CK_CHECK(sanitized == "A\xEF\xBF\xBDz");
}

CK_TEST(sanitize_replaces_a_truncated_trailing_sequence) {
    // decode() resyncs one byte at a time on malformed input (documented
    // in utf8.hpp): a 2-byte truncated 3-byte lead produces TWO
    // replacement characters, not one — the truncated lead byte, then
    // the orphaned continuation byte re-parsed on its own.
    const std::string sanitized =
        ckv::text::sanitize_display_text(cat({"A", "\xE4\xB8"}));
    CK_CHECK(sanitized == cat({"A", "\xEF\xBF\xBD", "\xEF\xBF\xBD"}));
}

CK_TEST(sanitize_replaces_an_embedded_nul_byte) {
    const std::string text = std::string("A") + '\0' + "B";
    CK_CHECK(text.size() == 3);
    const std::string sanitized = ckv::text::sanitize_display_text(text);
    CK_CHECK(sanitized == cat({"A", "\xEF\xBF\xBD", "B"}));
}

CK_TEST(sanitize_gives_each_malformed_byte_its_own_replacement) {
    const std::string sanitized =
        ckv::text::sanitize_display_text(cat({"A", "\x80", "\xFF", "z"}));
    CK_CHECK(sanitized == cat({"A", "\xEF\xBF\xBD", "\xEF\xBF\xBD", "z"}));
}

CK_TEST(sanitize_leaves_ordinary_unicode_untouched) {
    const std::string text = cat({"caf", kEAcute, kZhong});
    CK_CHECK(ckv::text::sanitize_display_text(text) == text);
}

CK_TEST(borrowed_clipping_preserves_whole_unicode_clusters_and_input_lifetime) {
    const std::string input = cat({"A", kEAcute, kZhong, kManZwjWomanZwjGirl, "Z"});
    CK_CHECK(ckv::text::clip_to_width_view(input, 0).empty());
    CK_CHECK(ckv::text::clip_to_width_view(input, -1).empty());
    CK_CHECK(ckv::text::clip_to_width_view(input, 3) == cat({"A", kEAcute}));
    CK_CHECK(ckv::text::clip_to_width_view(input, 4) == cat({"A", kEAcute, kZhong}));
    CK_CHECK(ckv::text::clip_to_width_view(input, 100).data() == input.data());
    CK_CHECK(ckv::text::clip_to_width_view(input, 100).size() == input.size());
}

CK_TEST(display_validation_matches_sanitization_for_controls_malformed_utf8_and_unicode) {
    for (int byte = 0; byte <= 255; ++byte) {
        const std::string input(1, static_cast<char>(byte));
        CK_CHECK(ckv::text::is_sanitized_display_text(input) == (ckv::text::sanitize_display_text(input) == input));
    }
    const std::string clean = cat({"A", kEAcute, kZhong, kManZwjWomanZwjGirl, "\xEF\xBF\xBD"});
    CK_CHECK(ckv::text::is_sanitized_display_text(clean));
    CK_CHECK(ckv::text::is_sanitized_display_text(""));
    CK_CHECK(!ckv::text::is_sanitized_display_text("\xC2\x80"));
    CK_CHECK(!ckv::text::is_sanitized_display_text("\xE4\xB8"));
    CK_CHECK(!ckv::text::is_sanitized_display_text("\xC0\xAF"));
}
