// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <array>
#include <cstdint>

#include "cvision/testing/cktest.hpp"

#include "cvision/widgets/syntax_cache.hpp"

using ckv::widgets::LanguageDetection;
using ckv::widgets::LanguageDetectionInput;
using ckv::widgets::LanguageProfile;
using ckv::widgets::SyntaxCache;
using ckv::widgets::SyntaxLineResult;

CK_TEST(syntax_cache_relexes_to_a_fixed_point_without_rehighlighting_an_unchanged_suffix) {
    std::size_t calls = 0;
    LanguageProfile profile{
        "counting", "Counting", [](const LanguageDetectionInput&) { return LanguageDetection{}; },
        [&calls](std::string_view, std::string_view state) {
            ++calls;
            return SyntaxLineResult{{}, std::string(state)};
        }};
    SyntaxCache cache;
    CK_CHECK(cache.update(profile, {"first", "middle", "tail"}).line_count == 3U);
    CK_CHECK(calls == 3U);
    calls = 0;
    const auto changed = cache.update(profile, {"first", "changed", "tail"});
    CK_CHECK(changed.first_line == 1U);
    CK_CHECK(changed.line_count == 2U);
    CK_CHECK(changed.reached_fixed_point);
    CK_CHECK(calls == 2U);
    calls = 0;
    CK_CHECK(cache.update(profile, {"first", "changed", "tail"}).line_count == 0U);
    CK_CHECK(calls == 0U);
}

CK_TEST(syntax_cache_propagates_multiline_state_until_the_old_state_converges) {
    LanguageProfile profile{
        "stateful", "Stateful", [](const LanguageDetectionInput&) { return LanguageDetection{}; },
        [](std::string_view line, std::string_view state) {
            if (line == "open") return SyntaxLineResult{{}, "open"};
            if (line == "close") return SyntaxLineResult{{}, {}};
            return SyntaxLineResult{{}, std::string(state)};
        }};
    SyntaxCache cache;
    CK_CHECK(cache.update(profile, {"open", "body", "close", "tail"}).line_count == 4U);
    const auto changed = cache.update(profile, {"plain", "body", "close", "tail"});
    CK_CHECK(changed.first_line == 0U);
    CK_CHECK(changed.line_count == 4U);
    CK_CHECK(cache.line(1)->incoming_state.empty());
    CK_CHECK(cache.line(3)->outgoing_state.empty());
}

CK_TEST(syntax_cache_exposes_deterministic_bounded_continuation_without_a_worker) {
    std::size_t calls = 0;
    LanguageProfile profile{
        "bounded", "Bounded", [](const LanguageDetectionInput&) { return LanguageDetection{}; },
        [&calls](std::string_view line, std::string_view state) {
            ++calls;
            return SyntaxLineResult{{}, line == "open" ? "open" : std::string(state)};
        }};
    SyntaxCache cache;
    CK_CHECK(cache.update(profile, {"open", "body", "tail"}).reached_fixed_point);
    calls = 0;
    const auto first = cache.update_bounded(profile, {"plain", "body", "tail"}, 1U);
    CK_CHECK(first.first_line == 0U);
    CK_CHECK(first.line_count == 1U);
    CK_CHECK(!first.reached_fixed_point);
    CK_CHECK(cache.has_pending_work());
    const auto second = cache.update_bounded(profile, {"plain", "body", "tail"}, 1U);
    CK_CHECK(second.first_line == 1U);
    CK_CHECK(second.line_count == 1U);
    CK_CHECK(!second.reached_fixed_point);
    const auto third = cache.update_bounded(profile, {"plain", "body", "tail"}, 1U);
    CK_CHECK(third.first_line == 2U);
    CK_CHECK(third.line_count == 1U);
    CK_CHECK(third.reached_fixed_point);
    CK_CHECK(!cache.has_pending_work());
    CK_CHECK(calls == 3U);
}

namespace {

// Marks each line that reads `marked_text` as one keyword span and carries the state through unchanged.
LanguageProfile marking_profile(std::string id, std::string_view marked_text) {
    return LanguageProfile{
        std::move(id), "Marking", [](const LanguageDetectionInput&) { return LanguageDetection{}; },
        [marked = std::string(marked_text)](std::string_view line, std::string_view state) {
            SyntaxLineResult result{{}, std::string(state)};
            if (line == marked)
                result.spans.push_back(ckv::widgets::SyntaxSpan{0, line.size(), ckv::widgets::SyntaxTokenKind::Keyword});
            return result;
        }};
}

}  // namespace

CK_TEST(syntax_cache_relexes_every_changed_line_of_an_update_with_separated_edits) {
    const LanguageProfile profile = marking_profile("marking", "x");
    SyntaxCache cache;
    CK_CHECK(cache.update(profile, {"a", "b", "c", "d", "e"}).reached_fixed_point);
    // Two edits with an unchanged line between them: the first line after the first edit is a
    // fixed point, but the lexing beyond it is not current.
    CK_CHECK(cache.update(profile, {"A", "b", "c", "x", "e"}).reached_fixed_point);
    CK_CHECK(cache.line(0)->text == "A");
    CK_CHECK(cache.line(3)->text == "x");
    CK_CHECK(cache.line(3)->spans.size() == 1U);
    CK_CHECK(cache.update(profile, {"A", "b", "c", "x", "e"}).line_count == 0U);

    // Lines added at the end after an edit near the start are lexed too.
    CK_CHECK(cache.update(profile, {"a", "b", "c", "x", "e", "x"}).reached_fixed_point);
    CK_CHECK(cache.line(5)->text == "x");
    CK_CHECK(cache.line(5)->spans.size() == 1U);

    // So is every line bounded passes leave behind.
    CK_CHECK(!cache.update_bounded(profile, {"b", "b", "c", "d", "e", "d"}, 1U).reached_fixed_point);
    while (cache.has_pending_work()) (void)cache.update_bounded(profile, {"b", "b", "c", "d", "e", "d"}, 1U);
    CK_CHECK(cache.line(3)->text == "d" && cache.line(3)->spans.empty());
    CK_CHECK(cache.line(5)->text == "d" && cache.line(5)->spans.empty());
}

CK_TEST(syntax_cache_relexes_the_whole_document_for_a_new_profile) {
    SyntaxCache cache;
    CK_CHECK(cache.update(marking_profile("first", "y"), {"plain", "x"}).line_count == 2U);
    CK_CHECK(cache.line(1)->spans.empty());
    // The first line lexes identically under both profiles; the second does not.
    CK_CHECK(cache.update(marking_profile("second", "x"), {"plain", "x"}).line_count == 2U);
    CK_CHECK(cache.line(1)->spans.size() == 1U);
}

namespace {

// A stateful profile whose spans depend on the incoming state, so a line lexed under a stale
// state is visibly wrong: "open" and "close" toggle a block, and every other line inside one is
// a string.
LanguageProfile block_profile() {
    return LanguageProfile{
        "block", "Block", [](const LanguageDetectionInput&) { return LanguageDetection{}; },
        [](std::string_view line, std::string_view state) {
            SyntaxLineResult result{{}, std::string(state)};
            if (line == "open") result.next_state = "open";
            else if (line == "close") result.next_state.clear();
            else if (state == "open" && !line.empty())
                result.spans.push_back(ckv::widgets::SyntaxSpan{0, line.size(), ckv::widgets::SyntaxTokenKind::String});
            return result;
        }};
}

bool cache_matches_a_fresh_lexing(const SyntaxCache& cache, const LanguageProfile& profile,
                                  const std::vector<std::string>& source) {
    SyntaxCache fresh;
    (void)fresh.update(profile, source);
    if (cache.lines().size() != fresh.lines().size()) return false;
    for (std::size_t index = 0; index < source.size(); ++index) {
        const auto& left = cache.lines()[index];
        const auto& right = fresh.lines()[index];
        if (left.text != right.text || left.spans != right.spans || left.incoming_state != right.incoming_state ||
            left.outgoing_state != right.outgoing_state)
            return false;
    }
    return true;
}

}  // namespace

CK_TEST(syntax_cache_bounded_passes_stay_exact_across_line_insertions_and_removals) {
    // Line insertions and removals land before, inside and after an interrupted pass's resume
    // point, among runs of identical lines, and the cache must end every drained pass exactly
    // as a fresh lexing of the same source. A fixed-seed generator keeps the walk reproducible.
    const LanguageProfile profile = block_profile();
    const std::array<std::string_view, 4> alphabet = {"b", "b", "open", "close"};
    std::uint32_t seed = 20260926U;
    const auto next = [&seed](std::uint32_t bound) {
        seed = seed * 1664525U + 1013904223U;
        return (seed >> 8U) % bound;
    };
    std::vector<std::string> source(12, "b");
    SyntaxCache cache;
    (void)cache.update(profile, source);
    bool exact = true;
    for (int step = 0; step < 4000 && exact; ++step) {
        const std::uint32_t operation = next(3U);
        const std::size_t at = next(static_cast<std::uint32_t>(source.size() + 1U));
        const std::string text(alphabet[next(static_cast<std::uint32_t>(alphabet.size()))]);
        if (operation == 0U || source.size() < 2U) source.insert(source.begin() + static_cast<std::ptrdiff_t>(at), text);
        else if (operation == 1U) source.erase(source.begin() + static_cast<std::ptrdiff_t>(std::min(at, source.size() - 1U)));
        else source[std::min(at, source.size() - 1U)] = text;
        if (source.size() > 24U) source.resize(12U);
        (void)cache.update_bounded(profile, source, 1U + next(3U));
        if (!cache.has_pending_work()) exact = cache_matches_a_fresh_lexing(cache, profile, source);
        if (exact && next(4U) == 0U) {
            while (cache.has_pending_work()) (void)cache.update_bounded(profile, source, 1U + next(3U));
            exact = cache_matches_a_fresh_lexing(cache, profile, source);
        }
    }
    while (cache.has_pending_work()) (void)cache.update_bounded(profile, source, 2U);
    CK_CHECK(exact && cache_matches_a_fresh_lexing(cache, profile, source));
}
