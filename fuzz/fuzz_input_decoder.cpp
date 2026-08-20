// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

#include "cvision/term/input_decoder.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    ckv::term::InputDecoder decoder;
    std::int64_t now = 0;
    std::size_t offset = 0;
    while (offset < input.size()) {
        const std::size_t chunk = 1 + (static_cast<unsigned char>(input[offset]) % 17U);
        const std::size_t count = std::min(chunk, input.size() - offset);
        const std::vector<ckv::term::TerminalEvent> events =
            decoder.feed(std::string_view(input).substr(offset, count), now);
        ckv::fuzz::require(events.size() <= input.size() + 1U);
        offset += count;
        now += 1'000'000;
    }
    (void)decoder.poll_timeout(now + ckv::term::kPasteTerminationQuietNanos + 1);
    (void)decoder.abort_paste();
    ckv::fuzz::require(!decoder.in_paste());
    return 0;
}
