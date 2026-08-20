// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Shared '&'-marked mnemonic parsing for Label and Button (classic CUA
// convention: "&Save" underlines 'S' as the mnemonic; "&&" is a
// literal ampersand). Visual parsing/rendering only — key-driven
// mnemonic ACTIVATION (jumping focus to a label's buddy, or firing a
// button, from anywhere in a window) is accelerator-table machinery
// that lands together with M5's menu system, which needs the identical
// window/application-scoped accelerator scan. Documented scope gap,
// not an oversight: a mnemonic with nothing to route it to yet would
// be unfalsifiable by test.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ckv::widgets {

struct MnemonicText {
    std::string display;                                    // '&' markers stripped
    std::string mnemonic;                                    // the marked grapheme, empty if none
    std::size_t mnemonic_byte_offset = std::string::npos;    // offset of `mnemonic` within `display`
};

MnemonicText parse_mnemonic(std::string_view raw);

}  // namespace ckv::widgets
