// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/widgets/syntax_profile.hpp"
#include "fuzz_common.hpp"

namespace {
// Which byte offsets of `line`, 0 through its size, are grapheme-cluster boundaries.
std::vector<bool> boundaries(std::string_view line) {
    std::vector<bool> result(line.size() + 1U, false);
    result[0] = true;
    for (std::size_t offset = 0; offset < line.size();) {
        offset = ckv::text::grapheme_end(line, offset);
        result[offset] = true;
    }
    return result;
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    ckv::widgets::SyntaxProfileRegistry registry;
    ckv::widgets::register_standard_syntax_profiles(registry);
    // Each standard profile by id, with the detection input filled as TextEditor
    // fills it: the text's first 512 bytes and its first line.
    constexpr std::string_view ids[] = {"json", "yaml", "bash", "markdown", "sql", "plain"};
    const std::string first_line = input.substr(0, input.find('\n'));
    for (const std::string_view id : ids) {
        const ckv::widgets::LanguageProfile& profile = registry.detect(
            ckv::widgets::LanguageDetectionInput{std::string(id), "", input.substr(0, 512U), first_line});
        ckv::fuzz::require(profile.id == id);
        std::string state;
        std::size_t start = 0;
        while (true) {
            const std::size_t newline = input.find('\n', start);
            const std::string_view line = std::string_view(input).substr(
                start, newline == std::string::npos ? std::string::npos : newline - start);
            const ckv::widgets::SyntaxLineResult result = profile.highlight_line(line, state);
            const std::vector<bool> boundary = boundaries(line);
            for (const ckv::widgets::SyntaxSpan& span : result.spans) {
                ckv::fuzz::require(span.begin_byte < span.end_byte && span.end_byte <= line.size());
                ckv::fuzz::require(boundary[span.begin_byte] && boundary[span.end_byte]);
            }
            state = result.next_state;
            if (newline == std::string::npos) break;
            start = newline + 1U;
        }
    }
    return 0;
}
