// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "cvision/core/text.hpp"
#include "cvision/widgets/syntax_profile.hpp"
#include "fuzz_common.hpp"

namespace {
bool is_boundary(std::string_view text, std::size_t position) {
    if (position == 0U || position == text.size()) return true;
    for (std::size_t offset = 0; offset < text.size();) {
        offset = ckv::text::grapheme_end(text, offset);
        if (offset == position) return true;
        if (offset > position) return false;
    }
    return false;
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);
    ckv::widgets::SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    constexpr std::string_view names[] = {"f.json", "f.yaml", "f.sh", "f.txt"};
    for (const std::string_view name : names) {
        const ckv::widgets::LanguageProfile& profile = registry.detect(
            ckv::widgets::LanguageDetectionInput{std::nullopt, std::string(name), "#!/usr/bin/env bash", input.substr(0, 512U)});
        std::string state;
        std::size_t start = 0;
        while (true) {
            const std::size_t newline = input.find('\n', start);
            const std::string_view line = std::string_view(input).substr(
                start, newline == std::string::npos ? std::string::npos : newline - start);
            const ckv::widgets::SyntaxLineResult result = profile.highlight_line(line, state);
            for (const ckv::widgets::SyntaxSpan& span : result.spans) {
                ckv::fuzz::require(span.begin_byte < span.end_byte && span.end_byte <= line.size());
                ckv::fuzz::require(is_boundary(line, span.begin_byte) && is_boundary(line, span.end_byte));
            }
            state = result.next_state;
            if (newline == std::string::npos) break;
            start = newline + 1U;
        }
    }
    return 0;
}
