// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the File Browser example's M10 behaviour
// (WP-22 lazy population, WP-19 Splitter), driven through the real
// FileBrowserApp object graph over a MemoryFileSystem. Input enters only
// through the HeadlessTerminal and Application::step: the initial frame; Down,
// Down and Right to select and lazily expand "beta"; then a mouse drag of the
// Splitter's divider three cells to the right.
// tests/test_filebrowser_smoke.cpp runs the same script and compares each
// composed frame with these dumps.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/filesystem.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/splitter.hpp"
#include "filebrowser_app.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const char* name, const ckv::ui::Application& app) {
    std::ofstream output(directory / name, std::ios::binary);
    output << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
}

void press(ckv::term::HeadlessTerminal& terminal, ckv::ui::Application& app, ckv::Key key) {
    terminal.inject_event(ckv::KeyEvent{ckv::KeyChord{key, ckv::Modifier::None, ""}});
    app.step(0);
}

void inject_mouse(ckv::term::HeadlessTerminal& terminal, ckv::ui::Application& app, ckv::MouseAction action,
                  ckv::Point cell) {
    terminal.inject_event(
        ckv::MouseEvent{action, ckv::MouseButton::Left, cell, std::nullopt, ckv::Modifier::None});
    app.step(0);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    ckv::MemoryFileSystem fs;
    fs.add_directory("/root");
    fs.add_directory("/root/alpha");
    fs.add_file("/root/alpha/a1.txt");
    fs.add_file("/root/alpha/a2.txt");
    fs.add_directory("/root/beta");
    fs.add_file("/root/beta/b1.txt");
    fs.add_directory("/root/beta/nested");
    fs.add_file("/root/readme.txt");

    ckv::term::HeadlessTerminal terminal(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::filebrowser::FileBrowserApp browser(app, fs, "/root");

    app.step(0);
    write_dump(directory, "filebrowser_initial.dump", app);

    press(terminal, app, ckv::Key::Down);   // alpha
    press(terminal, app, ckv::Key::Down);   // beta
    press(terminal, app, ckv::Key::Right);  // expand beta: listed now, on demand
    write_dump(directory, "filebrowser_expanded.dump", app);

    const ckv::Rect splitter = browser.splitter()->absolute_bounds();
    const ckv::Point divider{splitter.x + browser.splitter()->split_position(),
                             splitter.y + splitter.height / 2};
    inject_mouse(terminal, app, ckv::MouseAction::Down, divider);
    inject_mouse(terminal, app, ckv::MouseAction::Move, ckv::Point{divider.x + 3, divider.y});
    inject_mouse(terminal, app, ckv::MouseAction::Up, ckv::Point{divider.x + 3, divider.y});
    write_dump(directory, "filebrowser_splitter_moved.dump", app);
    return 0;
}
