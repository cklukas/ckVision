// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "cvision/core/key.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const std::optional<ckv::KeyChord> parsed = ckv::KeyChord::parse(input);
    if (parsed.has_value()) {
        const std::string formatted = parsed->to_string();
        ckv::fuzz::require(!formatted.empty());
        const std::optional<ckv::KeyChord> reparsed = ckv::KeyChord::parse(formatted);
        ckv::fuzz::require(reparsed.has_value());
    }
    return 0;
}
