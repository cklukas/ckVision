// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the hostile-content goldens (the architecture
// §12, WP-29/WP-30): the frame a hostile bracketed paste recovers into, and
// hostile display text painted through Painter::draw_text. The suites that
// compare against them are test_paste_security.cpp and
// test_golden_hostile_content.cpp; each builds the same scene through the
// same public calls.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string_view>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/input_decoder.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/input_line.hpp"

namespace {

bool write_dump(const std::filesystem::path& path, const ckv::golden::Document& document) {
    std::ofstream output(path, std::ios::binary);
    output << ckv::golden::serialize(document);
    return static_cast<bool>(output);
}

// An input line receives a paste whose payload carries a look-alike end
// marker and a Ctrl+Q chord behind it; the recovered text is what it shows.
ckv::golden::Document paste_recovery_frame() {
    // The smallest frame an Application renders normally (kHardFloorSize is 20x6).
    ckv::term::HeadlessTerminal terminal(ckv::Size{24, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto input = std::make_unique<ckv::widgets::InputLine>();
    input->set_fills_root(false);
    input->set_bounds(ckv::Rect{1, 2, 22, 1});
    auto* const input_ptr = input.get();
    app.root().add_child(std::move(input));
    app.set_focus(input_ptr);
    app.commands().declare(ckv::ui::CommandDescriptor{
        .key = "test.dangerous-action",
        .title = "Dangerous action",
        .chord = "Ctrl+Q",
        .handler = [] {},
    });

    ckv::term::InputDecoder decoder;
    std::vector<ckv::term::TerminalEvent> events = decoder.feed("\x1B[200~safe\x1B[201~\x11\x1B[201~", 0);
    auto completed = decoder.poll_timeout(ckv::term::kPasteTerminationQuietNanos);
    events.insert(events.end(), std::make_move_iterator(completed.begin()),
                  std::make_move_iterator(completed.end()));
    for (const ckv::term::TerminalEvent& event : events) app.dispatch(event);
    app.step(0);
    return ckv::scene::capture(app.composed_surface(), app.current_cursor());
}

// One hostile string per row: an erase-display CSI, an OSC 52 clipboard
// write, U+009B (CSI to a UTF-8 host), BEL/DEL/NUL, TAB and LF, raw 8-bit
// CSI and ST bytes, and malformed UTF-8.
ckv::golden::Document hostile_display_text_frame() {
    using namespace std::string_view_literals;
    constexpr std::string_view kRows[] = {
        "A\x1B[2JB",
        "C\x1B]52;c;aGk=\x07" "D",
        "E\xC2\x9B" "31mF",
        "G\x07H\x7FI\x00J"sv,
        "K\tL\nM",
        "N\x9B" "2JO\x9CP",
        "Q\xFFR\xC3S\xE2\x82T",
    };
    ckv::scene::Surface surface(ckv::Size{16, 7});
    ckv::scene::Painter painter(surface, ckv::Rect{0, 0, 16, 7});
    int row = 0;
    for (const std::string_view text : kRows) painter.draw_text(ckv::Point{0, row++}, text, ckv::Style{});
    return ckv::scene::capture(surface);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    const bool written = write_dump(directory / "paste_recovery.dump", paste_recovery_frame()) &&
                         write_dump(directory / "hostile_display_text.dump", hostile_display_text_frame());
    return written ? 0 : 1;
}
