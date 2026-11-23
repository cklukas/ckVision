// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/term/sixel_decoder.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const int width = size > 0 ? 1 + (data[0] % 128) : 64;
    const int height = size > 1 ? 1 + (data[1] % 128) : 64;
    const std::size_t max_pixels = 4096;

    ckv::term::SixelPalette palette;
    std::string error;
    const auto res = ckv::term::decode_sixel(input, ckv::PixelSize{width, height}, max_pixels, palette, error);
    if (res.has_value()) {
        const auto img_size = res->image.size();
        ckv::fuzz::require(img_size.width > 0 && img_size.height > 0);
        ckv::fuzz::require(static_cast<std::size_t>(img_size.width) *
                               static_cast<std::size_t>(img_size.height) <=
                           max_pixels);
    } else {
        ckv::fuzz::require(!error.empty());
    }
    // Exercise stateful palette reuse on a second call.
    std::string error2;
    (void)ckv::term::decode_sixel(input, ckv::PixelSize{width, height}, max_pixels, palette, error2);
    return 0;
}
