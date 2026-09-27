// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/osc_sequences.hpp"

#include <cstdint>

#include "cvision/core/base64.hpp"
#include "cvision/core/hyperlink.hpp"
#include "cvision/core/text.hpp"

namespace ckv::term {

// Both end at BEL, which the xterm control-sequences reference accepts as an
// OSC terminator alongside ST. Neither payload can hold a BEL or any other
// control, so the first BEL the host meets is always this one.
std::string osc_title_sequence(std::string_view title) {
    return "\x1B]0;" + text::sanitize_osc_text(title) + "\x07";
}

std::string osc_clipboard_sequence(std::string_view text) {
    return "\x1B]52;c;" + base64::encode(text) + "\x07";
}

// OSC 8 ends at ST, the terminator the hyperlink convention documents as
// standard. Neither parameter can hold a terminator: the id is hexadecimal
// and a valid target is printable ASCII.
std::string osc_hyperlink_open(std::string_view target) {
    if (!is_valid_hyperlink_target(target)) return {};
    std::string out = "\x1B]8;id=" + hyperlink_id(target) + ";";
    out += target;
    out += "\x1B\\";
    return out;
}

std::string_view osc_hyperlink_close() noexcept { return "\x1B]8;;\x1B\\"; }

std::string hyperlink_id(std::string_view target) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;  // FNV-1a 64-bit offset basis
    for (const char c : target) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 0x100000001b3ULL;
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = digits[hash & 0xFU];
        hash >>= 4U;
    }
    return out;
}

}  // namespace ckv::term
