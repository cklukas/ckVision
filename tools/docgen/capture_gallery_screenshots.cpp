// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Drives the same GalleryApp object graph the interactive
// examples/gallery binary and tests/test_gallery_smoke.cpp use, headlessly,
// and writes SVG screenshots of a few interesting states to
// docs/generated/screenshots/ — the images the example-apps
// documentation page embeds. Run via tools/docgen/generate_docs.sh,
// never committed as a manual step: regenerating docs means re-running
// this, not hand-editing an SVG.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "frame_svg.hpp"
#include "gallery_app.hpp"

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
    ckv::gallery::GalleryApp gallery(app);

    app.step(0);
    write_svg(out_dir, "gallery-initial", app.current_frame());

    app.dispatch(ckv::TextEvent{"Ada Lovelace", false});
    app.step(0);
    write_svg(out_dir, "gallery-typed-name", app.current_frame());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::F10, Modifier::None, ""}});
    app.step(0);
    write_svg(out_dir, "gallery-menu-open", app.current_frame());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    app.step(0);

    return 0;
}
