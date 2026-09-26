// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "hyperlink_script.hpp"

#include <memory>
#include <utility>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/text_view.hpp"

namespace ckv::docgen {

term::Capabilities hyperlink_host() noexcept {
    term::Capabilities caps = term::headless_no_graphics_profile();
    caps.hyperlinks = true;
    return caps;
}

HyperlinkStage::HyperlinkStage(std::vector<ScriptBeat> beats)
    : player(terminal, app, std::move(beats)) {
    const ui::StandardRoles roles = ui::intern_standard_roles(app.roles());
    app.theme() = ui::make_classic_theme(app.roles(), roles);
    auto view = std::make_unique<widgets::TextView>();
    view->set_fills_root(false);
    view->set_bounds(Rect{1, 1, 24, 5});
    view->set_wrap_mode(widgets::WrapMode::Word);
    const auto plain = [](std::string text) {
        return widgets::TextSpan{std::move(text), Attr{}, std::nullopt};
    };
    const auto link = [](std::string text, std::string_view target) {
        return widgets::TextSpan{std::move(text), Attr{}, std::string(target)};
    };
    view->set_spans({plain("Read the full "), link("ckVision guide", kGuide), plain(" or a "),
                     link("caf\xC3\xA9", kCafe), plain(".\nSee "), link("notes", kInternal),
                     plain(" and "), link("this", kHostile), plain(".\nAgain: "),
                     link("the guide", kGuide), plain("\nline 4\nline 5\nline 6\nline 7")});
    view->on_link_activate = [this](const std::string& target) { activated_.push_back(target); };
    view_ = view.get();
    app.root().add_child(std::move(view));
    app.set_focus(view_);
}

std::vector<ScriptBeat> text_view_hyperlink_script() {
    return {
        {"initial", {}, "text_view_hyperlinks_initial.dump"},
        {"next_link", {key(Key::Tab)}, ""},
        {"followed", {key(Key::Enter)}, ""},
        // "notes" on the view's third row, which the first line's wrap put there.
        {"clicked", {press(Point{6, 3}), release(Point{6, 3})}, ""},
        {"scrolled", {key(Key::Down), key(Key::Down)}, "text_view_hyperlinks_scrolled.dump"},
    };
}

std::string capture_presented(const term::HeadlessTerminal& terminal) {
    const FrameView frame = terminal.display().frame();
    scene::Surface surface(frame.size());
    for (int y = 0; y < frame.size().height; ++y)
        for (int x = 0; x < frame.size().width; ++x)
            surface.set_cell(Point{x, y}, frame.at(Point{x, y}), frame.link_target(Point{x, y}));
    return golden::serialize(scene::capture(surface, terminal.display().cursor()));
}

std::string presented_golden_name(std::string_view golden) {
    constexpr std::string_view kSuffix = ".dump";
    std::string name(golden.substr(0, golden.size() - kSuffix.size()));
    return name + "_presented.dump";
}

}  // namespace ckv::docgen
