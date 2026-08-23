// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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
