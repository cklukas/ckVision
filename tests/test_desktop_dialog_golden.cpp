// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// WP-26's visible ownership contract: a standard dialog attached through
// Desktop's generic View API is active, participates in F6 cycling, then
// closes through terminal input without leaving a stale window or focus.
#include <fstream>
#include <sstream>

#include "cvision/testing/cktest.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/message_box.hpp"

namespace {

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string capture(const ckv::ui::Application& app) {
    return ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

struct DialogScript {
    ckv::term::HeadlessTerminal terminal{ckv::Size{60, 20}};
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    ckv::widgets::Desktop* desktop = nullptr;
    ckv::widgets::Window* background = nullptr;
    ckv::widgets::Window* dialog = nullptr;
    ckv::widgets::InputLine* background_focus = nullptr;

    DialogScript() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        auto desktop_owned =
            std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
        desktop = desktop_owned.get();
        app.root().add_child(std::move(desktop_owned));

        auto background_owned = std::make_unique<ckv::widgets::Window>("Workspace");
        background_owned->set_bounds(ckv::Rect{3, 3, 34, 12});
        auto input_owned = std::make_unique<ckv::widgets::InputLine>();
        background_focus = input_owned.get();
        background_owned->set_content(std::move(input_owned));
        background = desktop->add_window(std::move(background_owned));
        app.set_focus(background_focus);

        const ckv::widgets::MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Notice",
                                                              "Saved successfully.",
                                                              ckv::widgets::MessageBoxButtons::Ok};
        auto handle = ckv::widgets::make_message_box(descriptor, roles, app, background_focus, nullptr);
        handle.window->set_bounds(ckv::Rect{16, 6, 30, 8});
        dialog = static_cast<ckv::widgets::Window*>(desktop->add_child(std::move(handle.window)));
        app.set_focus(handle.initial_focus);
    }
};

}  // namespace

CK_TEST(desktop_dialog_script_pins_generic_attach_cycling_and_close_frames) {
    DialogScript script;
    script.app.step(0);
    CK_CHECK(script.dialog->active());
    CK_CHECK(script.desktop->active_window() == script.dialog);
    CK_CHECK(capture(script.app) == read_file("golden/desktop_dialog_open.dump"));

    script.terminal.inject_event(
        ckv::KeyEvent{ckv::KeyChord{ckv::Key::F6, ckv::Modifier::None, ""}});
    CK_CHECK(script.app.step(0));
    CK_CHECK(script.desktop->active_window() == script.background);
    CK_CHECK(capture(script.app) == read_file("golden/desktop_dialog_cycled.dump"));

    script.terminal.inject_event(
        ckv::KeyEvent{ckv::KeyChord{ckv::Key::F6, ckv::Modifier::None, ""}});
    CK_CHECK(script.app.step(0));
    CK_CHECK(script.desktop->active_window() == script.dialog);

    script.terminal.inject_event(
        ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    CK_CHECK(script.app.step(0));
    CK_CHECK(script.desktop->windows().size() == 1);
    CK_CHECK(script.desktop->active_window() == script.background);
    CK_CHECK(script.app.focused() == script.background_focus);
    CK_CHECK(capture(script.app) == read_file("golden/desktop_dialog_closed.dump"));
}
