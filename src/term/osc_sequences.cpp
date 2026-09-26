// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/osc_sequences.hpp"

#include "cvision/core/base64.hpp"
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

}  // namespace ckv::term
