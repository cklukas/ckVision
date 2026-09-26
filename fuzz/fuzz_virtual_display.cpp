// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <string>

#include "cvision/core/text.hpp"
#include "cvision/term/virtual_display.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    ckv::term::VirtualDisplay display(ckv::Size{80, 24});
    std::size_t offset = 0;
    while (offset < input.size() && display.valid()) {
        const std::size_t chunk = 1 + (static_cast<unsigned char>(input[offset]) % 31U);
        const std::size_t count = std::min(chunk, input.size() - offset);
        (void)display.feed(std::string_view(input).substr(offset, count));
        offset += count;
    }
    (void)display.finish();
    ckv::fuzz::require(display.pixel_size() == ckv::PixelSize{720, 432});
    // Whatever the stream, the title the model accepted is one a host could
    // not have misread: nothing text::sanitize_osc_text would remove.
    ckv::fuzz::require(ckv::text::sanitize_osc_text(display.window_title()) == display.window_title());
    return 0;
}
