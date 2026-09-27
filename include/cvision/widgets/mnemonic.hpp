// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Shared '&'-marked mnemonic parsing for every widget that shows an
// '&'-marked caption: labels, buttons, menus, tabs, option groups and status
// items (classic CUA convention: "&Save" marks 'S' as the mnemonic; "&&" is
// a literal ampersand). This header is parsing only. Key-driven activation
// lives with the widgets: activate_label_mnemonic and
// activate_control_mnemonic in widgets/label.hpp route Alt+letter to a
// label's buddy or a Button, and menus and tab controls match their own
// captions.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ckv::widgets {

// A caption split into what is drawn and which grapheme is accented.
struct MnemonicText {
    // The text to draw, the mnemonic grapheme (whole, never a partial
    // cluster), and that grapheme's byte offset in `display`, which is
    // std::string::npos when there is no mnemonic.
    std::string display;                                    // '&' markers stripped
    std::string mnemonic;                                    // the marked grapheme, empty if none
    std::size_t mnemonic_byte_offset = std::string::npos;    // offset of `mnemonic` within `display`
};

// Parses `raw`: "&&" becomes a literal '&', and a single '&' marks the
// grapheme after it. Only the first marked grapheme becomes the mnemonic;
// every marker is stripped from the display text, including later ones and a
// trailing '&' that marks nothing. Pure and deterministic.
MnemonicText parse_mnemonic(std::string_view raw);

}  // namespace ckv::widgets
