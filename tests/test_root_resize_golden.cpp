// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WP-24's literal resize acceptance: one desktop is driven only through
// HeadlessTerminal::resize and Application::step, then its composed frame is
// pinned after each resize. Geometry assertions make the individual policies
// explicit; the frame-size assertions prove a shrink has no stale cells beyond
// the terminal's current grid.
#include <fstream>
#include <sstream>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using ckv::ManualClock;
using ckv::Rect;
using ckv::Size;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::widgets::Desktop;
using ckv::widgets::DesktopGrowPolicy;
using ckv::widgets::Window;

namespace {

std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string capture(const Application& app) {
    return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

struct ResizeScript {
    ckv::term::HeadlessTerminal term{Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;
    Window* ordinary = nullptr;
    Window* zoomed = nullptr;
    Window* filling = nullptr;

    ResizeScript() {
        app.theme() = make_classic_theme(app.roles(), roles);
        auto owned_desktop = std::make_unique<Desktop>(app.root().bounds());
        desktop = owned_desktop.get();
        app.root().add_child(std::move(owned_desktop));
        desktop->dock_top(std::make_unique<ckv::ui::View>());
        desktop->dock_bottom(std::make_unique<ckv::ui::View>());

        auto ordinary_window = std::make_unique<Window>("Ordinary");
        ordinary_window->set_bounds(Rect{60, 15, 18, 7});
        ordinary = desktop->add_window(std::move(ordinary_window));

        auto zoomed_window = std::make_unique<Window>("Zoomed");
        zoomed_window->set_bounds(Rect{15, 4, 28, 12});
        zoomed = desktop->add_window(std::move(zoomed_window));
        zoomed->toggle_zoom(desktop->content_area());

        auto filling_window = std::make_unique<Window>("Keep filling");
        filling_window->set_bounds(Rect{2, 2, 16, 6});
        filling = desktop->add_window(std::move(filling_window));
        filling->set_grow_policy(DesktopGrowPolicy::KeepFilling);
    }

    std::string resize_and_capture(Size size) {
        term.resize(size);
        CK_CHECK(app.step(0));
        return capture(app);
    }
};

void check_reachable(const Window& window, Rect area) {
    const Rect bounds = window.bounds();
    CK_CHECK(bounds.x >= area.x);
    CK_CHECK(bounds.y >= area.y);
    CK_CHECK(bounds.x + bounds.width <= area.x + area.width);
    CK_CHECK(bounds.y + bounds.height <= area.y + area.height);
}

}  // namespace

CK_TEST(headless_resize_script_pins_each_step_and_covers_root_window_policies) {
    ResizeScript script;

    const std::string grown = script.resize_and_capture(Size{120, 40});
    const Rect grown_content{0, 1, 120, 38};
    CK_CHECK(script.desktop->bounds() == (Rect{0, 0, 120, 40}));
    CK_CHECK(script.desktop->top_dock()->bounds() == (Rect{0, 0, 120, 1}));
    CK_CHECK(script.desktop->bottom_dock()->bounds() == (Rect{0, 39, 120, 1}));
    CK_CHECK(script.zoomed->bounds() == grown_content);
    CK_CHECK(script.filling->bounds() == grown_content);
    check_reachable(*script.ordinary, grown_content);
    CK_CHECK(grown == read_file("golden/root_resize_grow.dump"));

    const std::string shrunk = script.resize_and_capture(Size{40, 10});
    const Rect shrunk_content{0, 1, 40, 8};
    CK_CHECK(script.desktop->bounds() == (Rect{0, 0, 40, 10}));
    CK_CHECK(script.desktop->top_dock()->bounds() == (Rect{0, 0, 40, 1}));
    CK_CHECK(script.desktop->bottom_dock()->bounds() == (Rect{0, 9, 40, 1}));
    CK_CHECK(script.zoomed->bounds() == shrunk_content);
    CK_CHECK(script.filling->bounds() == shrunk_content);
    check_reachable(*script.ordinary, shrunk_content);
    CK_CHECK(script.app.current_frame().size() == (Size{40, 10}));
    CK_CHECK(script.term.display().size() == (Size{40, 10}));
    CK_CHECK(shrunk == read_file("golden/root_resize_shrink.dump"));
}
