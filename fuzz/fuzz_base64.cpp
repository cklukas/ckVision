// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "cvision/core/base64.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view raw(reinterpret_cast<const char*>(data), size);
    const std::string encoded = ckv::base64::encode(raw);
    std::string roundtrip;
    ckv::fuzz::require(ckv::base64::decode(encoded, roundtrip));
    ckv::fuzz::require(roundtrip == raw);
    std::string arbitrary;
    (void)ckv::base64::decode(raw, arbitrary);
    return 0;
}
