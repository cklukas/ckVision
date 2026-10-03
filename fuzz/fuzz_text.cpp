// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "cvision/core/text.hpp"
#include "cvision/core/utf8.hpp"
#include "fuzz_common.hpp"

namespace {

// The architecture §12's display-text neutralization: well-formed UTF-8 with
// no control code point at all, and — for clipboard text — none but the tab
// and line feed that are document content there.
void require_neutralized(const std::string& text, bool keep_tab_and_line_feed) {
    ckv::fuzz::require(ckv::utf8::is_valid(text));
    for (std::size_t pos = 0; pos < text.size();) {
        const char32_t cp = ckv::utf8::decode(text, pos);
        if (keep_tab_and_line_feed && (cp == U'\t' || cp == U'\n')) continue;
        ckv::fuzz::require(cp > 0x1F && cp != 0x7F && (cp < 0x80 || cp > 0x9F));
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input = ckv::fuzz::decode_seed_escapes(data, size);
    std::size_t offset = 0;
    while (offset < input.size()) {
        const std::size_t next = ckv::text::grapheme_end(input, offset);
        ckv::fuzz::require(next > offset && next <= input.size());
        offset = next;
    }
    for (int width : {-1, 0, 1, 2, 80}) {
        const std::string clipped = ckv::text::clip_to_width(input, width);
        ckv::fuzz::require(ckv::text::clip_to_width_view(input, width) == clipped);
        ckv::fuzz::require(ckv::text::text_width(clipped) <= (width < 0 ? 0 : width));
        (void)ckv::text::elide_to_width(input, width);
    }
    const std::string sanitized = ckv::text::sanitize_display_text(input);
    ckv::fuzz::require(ckv::text::is_sanitized_display_text(input) == (sanitized == input));
    require_neutralized(sanitized, false);
    require_neutralized(ckv::text::sanitize_clipboard_text(input), true);
    return 0;
}
