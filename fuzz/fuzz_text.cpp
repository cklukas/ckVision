// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "cvision/core/text.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    std::size_t offset = 0;
    while (offset < input.size()) {
        const std::size_t next = ckv::text::grapheme_end(input, offset);
        ckv::fuzz::require(next > offset && next <= input.size());
        offset = next;
    }
    for (int width : {-1, 0, 1, 2, 80}) {
        const std::string clipped = ckv::text::clip_to_width(input, width);
        ckv::fuzz::require(ckv::text::text_width(clipped) <= (width < 0 ? 0 : width));
        (void)ckv::text::elide_to_width(input, width);
    }
    (void)ckv::text::sanitize_display_text(input);
    return 0;
}
