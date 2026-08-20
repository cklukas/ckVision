// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Whether THIS terminal can show a picture, and what it said when asked.
//
// A window that shows a widget's cell fallback where a picture belongs has
// exactly two possible causes — the host cannot draw one, or it refused
// the one it was given — and they look identical on screen. This runs the
// same probe an application does, waits for the answers, and prints the
// evidence: run it in the terminal that misbehaves and paste what it says.
//
// It writes the report only after the terminal session has been restored,
// so redirecting stdout to a file yields plain text with nothing from the
// session's own escape sequences in it.
#include <cstdio>
#include <string>

#include "cvision/term/capability_report.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"

namespace {

// Long enough for a terminal to answer DA1 and XTSMGRAPHICS, short enough
// that a terminal which answers neither does not keep anybody waiting.
constexpr std::int64_t kProbeWindowNanos = 500'000'000;

std::string verdict(const ckv::term::Capabilities& caps) {
    if (!caps.sixel_graphics)
        return "VERDICT: this terminal does not report Sixel graphics, so every picture arrives as\n"
               "         the widget's cell fallback. Nothing an application does can change that;\n"
               "         try a terminal that answers DA1 with parameter 4 (xterm -ti vt340, iTerm2,\n"
               "         WezTerm, Ghostty, foot, mlterm).\n";
    std::string text =
        "VERDICT: this terminal draws Sixel graphics. A picture that still does not appear is\n"
        "         being refused rather than unsupported";
    if (caps.sixel_max_geometry.width > 0 || caps.sixel_max_geometry.height > 0)
        text += ", and the usual reason is size: images\n         larger than " +
                std::to_string(caps.sixel_max_geometry.width) + "x" +
                std::to_string(caps.sixel_max_geometry.height) +
                " px are dropped, not scaled.\n";
    else
        text += "; this terminal states no maximum picture\n         size, so size is not the reason.\n";
    return text;
}

}  // namespace

int main() {
    std::string report;
    {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock);
        const std::int64_t deadline = clock.now_nanos() + kProbeWindowNanos;
        while (clock.now_nanos() < deadline) (void)terminal.poll(deadline);
        report = ckv::term::capability_report_text(terminal.capabilities(), terminal.size()) + "\n" +
                 verdict(terminal.capabilities());
    }
    std::fputs(report.c_str(), stdout);
    return 0;
}
