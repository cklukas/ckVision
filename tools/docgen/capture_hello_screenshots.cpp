// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Same discipline as capture_gallery_screenshots.cpp, for HelloApp —
// the screenshots docs/hello-example.md embeds.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "frame_svg.hpp"
#include "hello_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;

namespace {
void write_svg(const std::filesystem::path& dir, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    const std::string svg = ckv::docgen::render_virtual_display_svg(display);
    std::ofstream out(dir / (name + ".svg"));
    out << svg;
    std::fprintf(stderr, "wrote %s (%dx%d cells)\n", (dir / (name + ".svg")).string().c_str(),
                 display.size().width, display.size().height);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);

    app.step(0);
    write_svg(out_dir, "hello-initial", term.display());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::F10, Modifier::None, ""}});
    app.step(0);
    write_svg(out_dir, "hello-menu-open", term.display());
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    app.step(0);
    write_svg(out_dir, "hello-greeting", term.display());
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    return 0;
}
