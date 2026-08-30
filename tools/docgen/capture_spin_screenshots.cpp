// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures the Spin example under Sixel and NoGraphics capabilities. The
// VirtualDisplay receives Presenter bytes and decodes them independently;
// no capture code reads a rendered Image directly (D-035).
//
// The clock is a ManualClock, so the angle in every screenshot is a
// property of the capture rather than of when it ran: the same command
// produces the same picture on every host.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "cvision/term/headless_terminal.hpp"
#include "frame_svg.hpp"
#include "spin_app.hpp"

namespace {

void write_svg(const std::filesystem::path& dir, const std::string& name,
               const ckv::term::VirtualDisplay& display) {
    std::ofstream out(dir / (name + ".svg"), std::ios::binary);
    out << ckv::docgen::render_virtual_display_svg(display);
    std::fprintf(stderr, "wrote %s (%dx%d cells, raster=%s)\n", (dir / (name + ".svg")).string().c_str(),
                 display.size().width, display.size().height,
                 display.has_raster_pixels() ? "yes" : "no");
}

// Plays `frames` frames at the application's own target interval, so the
// captured picture is at a plausible angle AND its frame-rate readout
// shows the rate that interval actually is. Anything larger in one jump
// would be a truthful capture of a rate no reader ever sees.
void play(ckv::ManualClock& clock, ckv::ui::Application& app, ckv::spin::SpinApp& spin, int frames) {
    for (int frame = 0; frame < frames; ++frame) {
        spin.frames().wait_until_idle();
        app.step(clock.now_nanos());
        clock.advance(ckv::spin::kFrameIntervalNanos);
        for (std::size_t index = 0; index < spin.open_windows(); ++index)
            spin.view_at(index)->request_frame(spin.frames(), clock.now_nanos());
    }
    spin.frames().wait_until_idle();
    app.step(clock.now_nanos());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    {
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_sixel_profile());
        ckv::ManualClock clock;
        ckv::ui::Application app(term, clock);
        ckv::spin::SpinApp spin(app);
        play(clock, app, spin, 23);
        write_svg(out_dir, "spin-initial", term.display());

        // F10 activates the menu bar, the first Enter drops File open on its
        // first item, and the second opens that item's own submenu — which is
        // how the shape catalog is presented.
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F10, ckv::Modifier::None, ""}});
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
        app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Enter, ckv::Modifier::None, ""}});
        app.step(clock.now_nanos());
        write_svg(out_dir, "spin-menu", term.display());
        for (int level = 0; level < 3; ++level)
            app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::Escape, ckv::Modifier::None, ""}});
        app.step(clock.now_nanos());

        (void)spin.open_window(ckv::spin::ShapeId::Torus);
        (void)spin.open_window(ckv::spin::ShapeId::WireGlobe);
        spin.desktop().tile();
        play(clock, app, spin, 15);
        write_svg(out_dir, "spin-desktop", term.display());
    }

    {
        ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_no_graphics_profile());
        ckv::ManualClock clock;
        ckv::ui::Application app(term, clock);
        ckv::spin::SpinApp spin(app);
        play(clock, app, spin, 23);
        write_svg(out_dir, "spin-no-graphics", term.display());
    }
    return 0;
}
