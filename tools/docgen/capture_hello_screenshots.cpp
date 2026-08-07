// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Same discipline as capture_gallery_screenshots.cpp, for HelloApp —
// the screenshots docs/turbo-vision-conversion.md embeds.
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
void write_svg(const std::filesystem::path& dir, const std::string& name, const ckv::FrameView& frame) {
    const std::string svg = ckv::docgen::render_frame_svg(frame);
    std::ofstream out(dir / (name + ".svg"));
    out << svg;
    std::fprintf(stderr, "wrote %s (%dx%d cells)\n", (dir / (name + ".svg")).c_str(), frame.size().width,
                 frame.size().height);
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);

    app.step(0);
    write_svg(out_dir, "hello-initial", app.current_frame());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    app.step(0);
    write_svg(out_dir, "hello-greeting", app.current_frame());

    return 0;
}
