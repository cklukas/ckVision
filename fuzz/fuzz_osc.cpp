// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/term/presenter.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const std::string escaped = ckv::term::sanitize_osc_text(input);
    ckv::fuzz::require(!ckv::fuzz::contains_terminal_control(escaped));
    return 0;
}
