// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// One-shot generator for tests/golden/hello_*.dump — NOT run by the
// test suite or CI; run by hand (and only by hand, deliberately, same
// as any golden-fixture regeneration) whenever HelloApp's rendered
// output is meant to change, then the new fixture is reviewed and
// committed like any other source change.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "hello_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::ui::Application;

namespace {
void write_dump(const std::filesystem::path& dir, const std::string& name, Application& app) {
    const ckv::golden::Document doc = ckv::scene::capture(app.composed_surface(), app.current_cursor());
    const std::string text = ckv::golden::serialize(doc);
    std::ofstream out(dir / (name + ".dump"));
    out << text;
    std::fprintf(stderr, "wrote %s\n", (dir / (name + ".dump")).c_str());
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    ckv::hello::HelloApp hello(app);

    app.step(0);
    write_dump(out_dir, "hello_initial", app);

    app.dispatch(ckv::KeyEvent{KeyChord{Key::Char, Modifier::Alt, "g"}});
    app.step(0);
    write_dump(out_dir, "hello_greeting", app);

    return 0;
}
