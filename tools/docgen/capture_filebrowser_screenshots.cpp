// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// Same discipline as capture_gallery_screenshots.cpp, for the File
// Browser example — against a scripted MemoryFileSystem (not the real
// disk) so the screenshots are deterministic across machines.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "filebrowser_app.hpp"
#include "frame_svg.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::MemoryFileSystem;
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

MemoryFileSystem make_demo_tree() {
    MemoryFileSystem fs;
    fs.add_directory("/project");
    fs.add_directory("/project/src");
    fs.add_file("/project/src/main.cpp");
    fs.add_file("/project/src/app.cpp");
    fs.add_directory("/project/include");
    fs.add_file("/project/include/app.hpp");
    fs.add_directory("/project/tests");
    fs.add_file("/project/tests/test_app.cpp");
    fs.add_file("/project/README.md");
    fs.add_file("/project/CMakeLists.txt");
    return fs;
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
    MemoryFileSystem fs = make_demo_tree();
    ckv::filebrowser::FileBrowserApp browser(app, fs, "/project");

    app.step(0);
    write_svg(out_dir, "filebrowser-initial", app.current_frame());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});  // -> src
    app.step(0);
    write_svg(out_dir, "filebrowser-src-selected", app.current_frame());

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});  // expand src (no children: no-op)
    app.dispatch(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});   // -> include
    app.step(0);
    write_svg(out_dir, "filebrowser-include-selected", app.current_frame());

    return 0;
}
