// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/core/golden.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const ckv::golden::ParseResult parsed = ckv::golden::parse(input);
    if (parsed) {
        const std::string canonical = ckv::golden::serialize(*parsed.document);
        const ckv::golden::ParseResult reparsed = ckv::golden::parse(canonical);
        ckv::fuzz::require(static_cast<bool>(reparsed));
        ckv::fuzz::require(ckv::golden::serialize(*reparsed.document) == canonical);
    }
    return 0;
}
