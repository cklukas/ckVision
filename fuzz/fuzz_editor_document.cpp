// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cvision/core/text.hpp"
#include "cvision/widgets/editor_document.hpp"
#include "fuzz_common.hpp"

namespace {
std::vector<std::size_t> boundaries(std::string_view text) {
    std::vector<std::size_t> result{0};
    for (std::size_t offset = 0; offset < text.size();) {
        offset = ckv::text::grapheme_end(text, offset);
        result.push_back(offset);
    }
    return result;
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);
    ckv::widgets::EditorDocumentOptions options;
    options.invalid_utf8 = ckv::widgets::InvalidUtf8Policy::Replace;
    ckv::widgets::EditorDocument document(input, options);
    const std::size_t rounds = std::min<std::size_t>(size, 64U);
    for (std::size_t round = 0; round < rounds; ++round) {
        const std::string current = document.text();
        const std::vector<std::size_t> positions = boundaries(current);
        const std::size_t first_index = static_cast<unsigned char>(input[round]) % positions.size();
        const std::size_t second_index = static_cast<unsigned char>(input[(round + 1U) % size]) % positions.size();
        const std::size_t begin = positions[std::min(first_index, second_index)];
        const std::size_t end = positions[std::max(first_index, second_index)];
        const std::size_t replacement_begin = (round * 7U) % size;
        const std::size_t replacement_size = std::min<std::size_t>(size - replacement_begin, 32U);
        const auto first = document.position_at_byte(begin);
        const auto last = document.position_at_byte(end);
        ckv::fuzz::require(first.has_value() && last.has_value());
        const auto result = document.replace(ckv::widgets::DocumentRange{*first, *last},
                                             input.substr(replacement_begin, replacement_size));
        ckv::fuzz::require(static_cast<bool>(result));

        const std::string after = document.text();
        for (std::size_t offset = 0; offset < after.size();) {
            const std::size_t next = ckv::text::grapheme_end(after, offset);
            ckv::fuzz::require(next > offset && next <= after.size());
            ckv::fuzz::require(document.position_at_byte(offset).has_value());
            offset = next;
        }
        ckv::fuzz::require(document.position_at_byte(after.size()).has_value());
        ckv::fuzz::require(document.line_count() != 0U);
        if ((static_cast<unsigned char>(input[round]) & 1U) != 0U) {
            const bool undone = document.undo();
            if (undone) ckv::fuzz::require(document.redo());
        }
    }
    return 0;
}
