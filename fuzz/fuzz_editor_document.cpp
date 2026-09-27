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
    // Seeds spell malformed bytes as \\xNN escapes (fuzzing.md), so the
    // document and every choice below read the decoded bytes.
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const std::size_t length = input.size();
    ckv::widgets::EditorDocumentOptions options;
    options.invalid_utf8 = ckv::widgets::InvalidUtf8Policy::Replace;
    ckv::widgets::EditorDocument document(input, options);
    const std::size_t rounds = std::min<std::size_t>(length, 64U);
    for (std::size_t round = 0; round < rounds; ++round) {
        const std::string current = document.text();
        const std::vector<std::size_t> positions = boundaries(current);
        const std::size_t first_index = static_cast<unsigned char>(input[round]) % positions.size();
        const std::size_t second_index = static_cast<unsigned char>(input[(round + 1U) % length]) % positions.size();
        const std::size_t begin = positions[std::min(first_index, second_index)];
        const std::size_t end = positions[std::max(first_index, second_index)];
        const std::size_t replacement_begin = (round * 7U) % length;
        const std::size_t replacement_size = std::min<std::size_t>(length - replacement_begin, 32U);
        const auto first = document.position_at_byte(begin);
        const auto last = document.position_at_byte(end);
        ckv::fuzz::require(first.has_value() && last.has_value());
        const auto result = document.replace(ckv::widgets::DocumentRange{*first, *last},
                                             input.substr(replacement_begin, replacement_size));
        ckv::fuzz::require(static_cast<bool>(result));

        const std::string after = document.text();
        const std::vector<std::size_t> after_positions = boundaries(after);
        for (std::size_t index = 1; index < after_positions.size(); ++index)
            ckv::fuzz::require(after_positions[index] > after_positions[index - 1U]);
        ckv::fuzz::require(after_positions.back() == after.size());
        // position_at_byte walks a boundary's line from its start, so checking every
        // boundary would make a round quadratic in the line; up to 64 spread evenly
        // over the text, and its end, keep a round linear.
        const std::size_t stride = after_positions.size() / 64U + 1U;
        for (std::size_t index = 0; index < after_positions.size(); index += stride)
            ckv::fuzz::require(document.position_at_byte(after_positions[index]).has_value());
        ckv::fuzz::require(document.position_at_byte(after.size()).has_value());
        ckv::fuzz::require(document.line_count() != 0U);
        if ((static_cast<unsigned char>(input[round]) & 1U) != 0U) {
            // Each step reports its real span: the text before it with that span replaced by
            // the span's new bytes is exactly the text after it.
            const auto apply = [&document](const std::string& old_text, const ckv::widgets::DocumentChange& change) {
                const std::string now = document.text();
                return old_text.substr(0, change.replaced_begin_byte) +
                           now.substr(change.replaced_begin_byte, change.inserted_bytes) +
                           old_text.substr(change.replaced_end_byte) ==
                       now;
            };
            const std::string before_undo = document.text();
            if (const auto undone = document.undo()) {
                ckv::fuzz::require(apply(before_undo, *undone));
                const std::string before_redo = document.text();
                const auto redone = document.redo();
                ckv::fuzz::require(redone.has_value() && apply(before_redo, *redone));
            }
        }
    }
    return 0;
}
