// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/mnemonic.hpp"

#include "cvision/core/text.hpp"

namespace ckv::widgets {

MnemonicText parse_mnemonic(std::string_view raw) {
    MnemonicText result;
    result.display.reserve(raw.size());
    bool marked_next = false;
    for (std::size_t i = 0; i < raw.size();) {
        if (raw[i] == '&') {
            if (i + 1 < raw.size() && raw[i + 1] == '&') {
                result.display.push_back('&');  // "&&" -> literal '&'
                i += 2;
                continue;
            }
            marked_next = true;
            ++i;
            continue;
        }
        const std::size_t end = text::grapheme_end(raw, i);
        const std::string_view grapheme = raw.substr(i, end - i);
        if (marked_next && result.mnemonic.empty()) {
            result.mnemonic_byte_offset = result.display.size();
            result.mnemonic = std::string(grapheme);
        }
        marked_next = false;
        result.display.append(grapheme);
        i = end;
    }
    return result;
}

}  // namespace ckv::widgets
