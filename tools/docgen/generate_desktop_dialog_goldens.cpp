// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for WP-26's desktop/dialog ownership script.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/message_box.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const char* name, const ckv::ui::Application& app) {
    std::ofstream output(directory / name);
    output << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    ckv::term::HeadlessTerminal terminal(ckv::Size{60, 20});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto desktop_owned = std::make_unique<ckv::widgets::Desktop>(app.root().bounds());
    ckv::widgets::Desktop* const desktop = desktop_owned.get();
    app.root().add_child(std::move(desktop_owned));

    auto background_owned = std::make_unique<ckv::widgets::Window>("Workspace");
    background_owned->set_bounds(ckv::Rect{3, 3, 34, 12});
    auto input_owned = std::make_unique<ckv::widgets::InputLine>();
    ckv::widgets::InputLine* const background_focus = input_owned.get();
    background_owned->set_content(std::move(input_owned));
    desktop->add_window(std::move(background_owned));
    app.set_focus(background_focus);

    const ckv::widgets::MessageBoxDescriptor descriptor{ckv::widgets::MessageBoxKind::Info, "Notice",
                                                          "Saved successfully.",
                                                          ckv::widgets::MessageBoxButtons::Ok};
    auto handle = ckv::widgets::make_message_box(descriptor, roles, app, background_focus, nullptr);
    handle.window->set_bounds(ckv::Rect{16, 6, 30, 8});
    ckv::widgets::Window* const dialog =
        static_cast<ckv::widgets::Window*>(desktop->add_child(std::move(handle.window)));
    app.set_focus(handle.initial_focus);

    app.step(0);
    write_dump(directory, "desktop_dialog_open.dump", app);
    terminal.inject_event(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F6, ckv::Modifier::None, ""}});
    app.step(0);
    write_dump(directory, "desktop_dialog_cycled.dump", app);
    terminal.inject_event(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F6, ckv::Modifier::None, ""}});
    app.step(0);
    terminal.inject_event(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    app.step(0);
    write_dump(directory, "desktop_dialog_closed.dump", app);
    (void)dialog;
    return 0;
}
