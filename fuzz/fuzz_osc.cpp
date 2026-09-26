// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/core/utf8.hpp"
#include "cvision/term/presenter.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const std::string escaped = ckv::term::sanitize_osc_text(input);
    ckv::fuzz::require(!ckv::fuzz::contains_terminal_control(escaped));
    // The whole contract: well-formed UTF-8 holding no C0 or C1 control code
    // point, so no 7-bit, 8-bit or UTF-8 spelling of a terminator survives.
    ckv::fuzz::require(ckv::utf8::is_valid(escaped));
    for (std::size_t pos = 0; pos < escaped.size();) {
        const char32_t cp = ckv::utf8::decode(escaped, pos);
        ckv::fuzz::require(cp > 0x1F && (cp < 0x7F || cp > 0x9F));
    }
    return 0;
}
