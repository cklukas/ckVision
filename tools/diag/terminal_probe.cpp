// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// What ckVision's own terminal layer decodes from THIS terminal. Run it in
// the terminal that misbehaves and click a few times: it reports the
// capabilities the probe established and every mouse event as decoded,
// which is the difference between diagnosing and guessing.
#include <cstdio>
#include <string>

#include "cvision/core/clock.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    std::printf("click anywhere; press q to quit\r\n");
    std::fflush(stdout);

    bool reported = false;
    for (int i = 0; i < 100000; ++i) {
        const std::int64_t now = clock.now_nanos();
        for (const auto& ev : terminal.poll(now + 100'000'000LL)) {
            if (const auto* c = std::get_if<ckv::term::CapabilityChangedEvent>(&ev)) {
                const auto& k = c->capabilities;
                std::printf("CAPS  sixel=%d pixel_mouse=%d cell=%dx%d text_area=%dx%d\r\n",
                            k.sixel_graphics, k.pixel_mouse, k.cell_pixels.width, k.cell_pixels.height,
                            k.text_area_pixels.width, k.text_area_pixels.height);
                reported = true;
            } else if (const auto* m = std::get_if<ckv::MouseEvent>(&ev)) {
                std::printf("MOUSE cell=(%d,%d) pixel=%s action=%d button=%d\r\n", m->cell.x, m->cell.y,
                            m->pixel ? (std::to_string(m->pixel->x) + "," + std::to_string(m->pixel->y)).c_str()
                                     : "none",
                            static_cast<int>(m->action), static_cast<int>(m->button));
            } else if (const auto* k = std::get_if<ckv::KeyEvent>(&ev)) {
                if (k->chord.text == "q") {
                    std::printf("bye\r\n");
                    std::fflush(stdout);
                    return 0;
                }
                std::printf("KEY   %s\r\n", ckv::format(k->chord).c_str());
            }
            std::fflush(stdout);
        }
        if (!reported && i == 20) {
            const auto& k = terminal.capabilities();
            std::printf("CAPS(no change event) sixel=%d pixel_mouse=%d cell=%dx%d text_area=%dx%d\r\n",
                        k.sixel_graphics, k.pixel_mouse, k.cell_pixels.width, k.cell_pixels.height,
                        k.text_area_pixels.width, k.text_area_pixels.height);
            std::fflush(stdout);
            reported = true;
        }
    }
    return 0;
}
