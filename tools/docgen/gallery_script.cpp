// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "gallery_script.hpp"

#include <string_view>
#include <utility>

namespace ckv::docgen {

GalleryStage::GalleryStage(term::Capabilities profile, std::vector<ScriptBeat> beats)
    : terminal(Size{80, 24}, profile), player(terminal, app, std::move(beats)) {}

std::vector<ScriptBeat> gallery_picture_script() {
    const Point picture = GalleryStage::kPictureCell;
    return {
        {"initial", {}, "gallery_picture_initial"},
        // Nothing in a picture takes the focus, so a click on it focuses the
        // viewport around it, and the keys then scroll that: a row, then a page.
        {"scrolled_1", {press(picture), release(picture), key(Key::Down)}, "gallery_picture_scrolled_1"},
        {"scrolled_page", {key(Key::PageDown)}, "gallery_picture_scrolled_page"},
        {"menu", {alt("v"), key(Key::Right)}, "gallery_picture_menu"},
        // Esc closes one level: the submenu, the menu, then the bar. Closing
        // them leaves exactly the frame they opened over, so this beat is
        // pinned by that frame's files.
        {"closed", {key(Key::Escape), key(Key::Escape), key(Key::Escape)}, "gallery_picture_scrolled_page"},
    };
}

std::vector<ScriptBeat> gallery_scheme_script() {
    // View > Scheme, then the scheme's own mnemonic.
    const auto choose = [](std::string_view mnemonic) {
        return std::vector<term::TerminalEvent>{alt("v"), key(Key::Right), character(mnemonic)};
    };
    return {
        {"dark", choose("d"), "gallery_scheme_dark"},
        {"light", choose("l"), "gallery_scheme_light"},
        {"mono", choose("m"), "gallery_scheme_mono"},
        {"classic", choose("c"), "gallery_scheme_classic"},
    };
}

}  // namespace ckv::docgen
