// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <string>

#include "cvision/core/hyperlink.hpp"
#include "cvision/core/text.hpp"
#include "cvision/core/utf8.hpp"
#include "cvision/term/osc_sequences.hpp"
#include "fuzz_common.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    const std::string escaped = ckv::text::sanitize_osc_text(input);
    ckv::fuzz::require(!ckv::fuzz::contains_terminal_control(escaped));
    // The whole contract: well-formed UTF-8 holding no C0 or C1 control code
    // point, so no 7-bit, 8-bit or UTF-8 spelling of a terminator survives.
    ckv::fuzz::require(ckv::utf8::is_valid(escaped));
    for (std::size_t pos = 0; pos < escaped.size();) {
        const char32_t cp = ckv::utf8::decode(escaped, pos);
        ckv::fuzz::require(cp > 0x1F && (cp < 0x7F || cp > 0x9F));
    }

    // A hyperlink target is validated rather than sanitized (D-088): the
    // same input either becomes one OSC 8 whose only ESC bytes are its
    // introducer and ST and whose body is printable ASCII, or nothing at all
    // — and a link table admits exactly the targets that can be sent.
    const std::string opening = ckv::term::osc_hyperlink_open(input);
    ckv::fuzz::require(opening.empty() == !ckv::is_valid_hyperlink_target(input));
    if (!opening.empty()) {
        ckv::fuzz::require(opening.starts_with("\x1B]8;id=") && opening.ends_with("\x1B\\"));
        for (std::size_t i = 1; i + 2 < opening.size(); ++i) {
            const auto byte = static_cast<unsigned char>(opening[i]);
            ckv::fuzz::require(byte > 0x20 && byte < 0x7F);
        }
    }
    ckv::LinkTable links;
    const ckv::LinkId id = links.acquire(input);
    ckv::fuzz::require((id != ckv::kNoLink) == ckv::is_valid_hyperlink_target(input));
    ckv::fuzz::require(id == ckv::kNoLink || links.target(id) == input);
    return 0;
}
