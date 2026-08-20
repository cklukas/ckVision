// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures FormsApp through public presentation and input APIs.  These images
// document real validation, modal, localization, and wizard states.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/core/key.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/input_line.hpp"
#include "forms_app.hpp"
#include "frame_svg.hpp"

namespace {
void write_svg(const std::filesystem::path& dir, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(dir / (name + ".svg"));
    out << ckv::docgen::render_virtual_display_svg(display);
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
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    ckv::forms::FormsApp forms(app);
    app.step(0);
    write_svg(out_dir, "forms-initial", term.display());

    forms.present_profile_dialog();
    app.step(0);
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
    app.step(0);
    write_svg(out_dir, "forms-invalid-dialog", term.display());
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}});
    app.step(0);

    forms.present_info_message();
    app.step(0);
    write_svg(out_dir, "forms-info-message", term.display());
    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}});
    app.step(0);

    forms.name_input()->set_text("Ada");
    app.step(0);
    write_svg(out_dir, "forms-wizard-ready", term.display());
    return 0;
}
