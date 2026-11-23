// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/widgets/date_time.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    if (const auto d = ckv::widgets::parse_iso_date(input)) {
        ckv::fuzz::require(ckv::widgets::is_valid_date(*d));
        const std::string formatted = ckv::widgets::format_iso_date(*d);
        const auto roundtrip = ckv::widgets::parse_iso_date(formatted);
        ckv::fuzz::require(roundtrip.has_value() && *roundtrip == *d);
    }
    if (const auto t = ckv::widgets::parse_iso_time(input)) {
        ckv::fuzz::require(ckv::widgets::is_valid_time(*t));
        const std::string formatted = ckv::widgets::format_iso_time(*t, true);
        const auto roundtrip = ckv::widgets::parse_iso_time(formatted);
        ckv::fuzz::require(roundtrip.has_value() && *roundtrip == *t);
    }
    ckv::widgets::DateFormat fmt{};
    (void)ckv::widgets::parse_date(input, fmt);
    return 0;
}
