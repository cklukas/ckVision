// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WP-25's visible activation contract: two genuinely overlapping windows
// begin with the front one active; a click on a descendant in the exposed
// part of the buried one raises/restyles it and routes later text to it.
#include <fstream>
#include <sstream>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/window.hpp"

using ckv::ManualClock;
using ckv::Point;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Desktop;
using ckv::widgets::InputLine;
using ckv::widgets::Window;

namespace {

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string capture(const Application& app) {
    return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

struct ActivationScript {
    ckv::term::HeadlessTerminal terminal{Size{60, 20}};
    ManualClock clock;
    Application app{terminal, clock};
    ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;
    Window* back = nullptr;
    Window* front = nullptr;
    InputLine* back_input = nullptr;

    ActivationScript() {
        app.theme() = make_classic_theme(app.roles(), roles);
        auto desktop_owner = std::make_unique<Desktop>(app.root().bounds());
        desktop = desktop_owner.get();
        app.root().add_child(std::move(desktop_owner));

        auto back_owner = std::make_unique<Window>("Back");
        back_owner->set_bounds(Rect{3, 3, 36, 12});
        auto back_input_owner = std::make_unique<InputLine>();
        back_input = back_input_owner.get();
        back_owner->set_content(std::move(back_input_owner));
        back = desktop->add_window(std::move(back_owner));

        auto front_owner = std::make_unique<Window>("Front");
        front_owner->set_bounds(Rect{15, 5, 36, 12});
        front_owner->set_content(std::make_unique<InputLine>());
        front = desktop->add_window(std::move(front_owner));
    }
};

}  // namespace

CK_TEST(overlapping_window_activation_script_pins_frame_roles_and_routes_text_to_the_clicked_descendant) {
    ActivationScript script;
    CK_CHECK(script.front->active());
    CK_CHECK(!script.back->active());
    script.app.step(0);  // initial dirty frame paints even without input work
    CK_CHECK(capture(script.app) == read_file("golden/window_activation_before.dump"));

    // This point lies in Back's InputLine but outside Front's bounds. The
    // windows still overlap substantially, so raising Back must restyle both
    // frames and move Back above the formerly active Front window.
    script.terminal.inject_event(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{5, 6},
                                                  std::nullopt, ckv::Modifier::None});
    CK_CHECK(script.app.step(0));
    CK_CHECK(script.back->active());
    CK_CHECK(!script.front->active());
    CK_CHECK(script.desktop->active_window() == script.back);
    CK_CHECK(script.app.focused() == script.back_input);
    CK_CHECK(capture(script.app) == read_file("golden/window_activation_after.dump"));

    script.terminal.inject_event(ckv::TextEvent{"Ada", false});
    CK_CHECK(script.app.step(0));
    CK_CHECK(script.back_input->text() == "Ada");
}
