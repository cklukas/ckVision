// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// TextView links as terminal hyperlinks (D-088), as an event script: a
// TextView on a host that renders OSC 8 holds a link that wraps onto a second
// line, a link with a non-ASCII letter the Presenter must re-address around,
// an application-internal link and a hostile target that must stay off the
// terminal, and a second span to the first link's target. Tab and Enter
// follow links exactly as before; scrolling moves the linked cells, and the
// cells scrolled onto lose their links. Played by
// tests/test_text_view_hyperlinks.cpp and by generate_event_script_goldens,
// which pins both the composed frame and the display decoded from the bytes
// the Presenter wrote.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/capabilities.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "event_script.hpp"

namespace ckv::widgets {
class TextView;
}  // namespace ckv::widgets

namespace ckv::docgen {

// The script's host: headless_no_graphics_profile() (TrueColor, so the
// decoded display can be compared with the composed frame cell for cell) with
// OSC 8 hyperlinks rendered.
term::Capabilities hyperlink_host() noexcept;

// A 32x7 terminal whose root holds one focused, word-wrapping TextView of
// 24x5 cells at (1, 1), with every activated target recorded in order.
class HyperlinkStage {
public:
    explicit HyperlinkStage(std::vector<ScriptBeat> beats);

    term::HeadlessTerminal terminal{Size{32, 7}, hyperlink_host()};
    ManualClock clock;
    ui::Application app{terminal, clock};
    ScriptPlayer player;

    widgets::TextView& view() noexcept { return *view_; }
    const std::vector<std::string>& activated() const noexcept { return activated_; }

    // The targets the view holds. kGuide and kCafe are terminal hyperlinks;
    // kInternal names a page to the application only, and kHostile tries
    // to end the OSC early and start a title of its own.
    static constexpr std::string_view kGuide = "https://example.org/guide";
    static constexpr std::string_view kCafe = "https://example.org/caf%C3%A9";
    static constexpr std::string_view kInternal = "notes.md";
    static constexpr std::string_view kHostile = "https://x.test/\x1B]0;owned\x07";

private:
    widgets::TextView* view_ = nullptr;
    std::vector<std::string> activated_;
};

// initial, next_link, followed, clicked, scrolled.
std::vector<ScriptBeat> text_view_hyperlink_script();

// The display HeadlessTerminal decoded from the bytes written so far, as a
// golden dump: what a terminal shows, links included.
std::string capture_presented(const term::HeadlessTerminal& terminal);
// The file the presented dump of a beat's `golden` is pinned in:
// "<name>.dump" becomes "<name>_presented.dump".
std::string presented_golden_name(std::string_view golden);

}  // namespace ckv::docgen
