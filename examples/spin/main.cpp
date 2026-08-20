// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Terminal host for the Spin example.
//
// It reads two environment variables, because a graphics application is
// the one kind that cannot tell a host that will not draw a picture from
// one that was never asked properly:
//
//   CKVISION_FORCE_SIXEL=1   present pictures even though this terminal
//                            did not advertise Sixel. For a terminal that
//                            draws them but answers no probe — the
//                            override exists in the library
//                            (term::CapabilityOverrides) precisely so an
//                            application can offer this without patching
//                            capability detection.
//   CKVISION_FORCE_CELL=WxH  the character cell in pixels, when the
//                            terminal reports none and the assumed one is
//                            visibly the wrong shape.
//   CKVISION_SPIN_FPS=<n>    frames per second to aim for, in place of the
//                            example's own target. The pacer still widens
//                            it under load, so this raises the ceiling
//                            rather than removing the floor.
//   CKVISION_SPIN_PIXEL_RATE=<n>
//                            raster pixels per second this terminal can
//                            actually paint. The default is cautious; a
//                            host that keeps up takes far more.
//   CKVISION_NO_SYNC=1       stop bracketing frames as one atomic update
//                            (DEC 2026). For a host that answers the query
//                            and then applies a picture outside the bracket
//                            it applies the surrounding cells in.
//   CKVISION_OUTPUT_CAPTURE=<file>
//                            every byte written to the terminal, so a frame
//                            a host renders wrongly can be replayed into
//                            ckVision's own decoder.
//
// `ckvision_graphics_check` reports what this terminal actually said.
#include <cstdlib>
#include <cstdio>
#include <string>

#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "spin_app.hpp"

namespace {

std::string environment_value(const char* name) {
    const char* const value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}

ckv::term::CapabilityOverrides overrides_from_environment() {
    ckv::term::CapabilityOverrides overrides;
    const std::string force = environment_value("CKVISION_FORCE_SIXEL");
    if (force == "1" || force == "yes" || force == "on") overrides.sixel_graphics = true;
    const std::string no_sync = environment_value("CKVISION_NO_SYNC");
    if (no_sync == "1" || no_sync == "yes" || no_sync == "on") overrides.synchronized_output = false;

    const std::string cell = environment_value("CKVISION_FORCE_CELL");
    if (const std::size_t x = cell.find('x'); x != std::string::npos) {
        const int width = std::atoi(cell.substr(0, x).c_str());
        const int height = std::atoi(cell.substr(x + 1).c_str());
        if (width > 0 && height > 0) overrides.cell_pixels = ckv::Size{width, height};
    }
    return overrides;
}

}  // namespace

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    if (const ckv::term::CapabilityOverrides overrides = overrides_from_environment();
        overrides != ckv::term::CapabilityOverrides{})
        terminal.set_capability_overrides(overrides);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock);
    ckv::spin::SpinApp spin(app);
    if (const int fps = std::atoi(environment_value("CKVISION_SPIN_FPS").c_str()); fps > 0)
        spin.set_target_frame_interval(1'000'000'000LL / fps);
    if (const double rate = std::atof(environment_value("CKVISION_SPIN_PIXEL_RATE").c_str()); rate > 0.0)
        spin.set_raster_pixel_rate(rate);
    app.run();
    return 0;
}
