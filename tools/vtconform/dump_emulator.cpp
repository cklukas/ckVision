// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// One half of the VT conformance harness (tools/vtconform/README.md): feed a
// byte script to ckVision's embedded-terminal emulator and print what the
// screen became, in a form another emulator's screen can be printed in too.
//
// The point is to stop deciding terminal semantics by argument. "Does an
// erase keep the underline attribute?" is not a question about taste; it has
// an answer that established terminals already agree on, and the way to get
// it is to run the same bytes through them and look.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cvision/term/terminal_emulator.hpp"

namespace {

// The fixed xterm palette, so a resolved colour can be named as the index a
// script asked for. The emulator stores colours resolved to RGB; a dump that
// printed raw RGB could not be compared against an emulator that reports
// indices, and every difference would look like a difference of opinion
// rather than of fact.
int palette_index_of(ckv::Color color) {
    static const std::vector<ckv::Color> palette = [] {
        std::vector<ckv::Color> table{
            ckv::Color::rgb(0, 0, 0),        ckv::Color::rgb(205, 49, 49),   ckv::Color::rgb(13, 188, 121),
            ckv::Color::rgb(229, 229, 16),   ckv::Color::rgb(36, 114, 200),  ckv::Color::rgb(188, 63, 188),
            ckv::Color::rgb(17, 168, 205),   ckv::Color::rgb(229, 229, 229), ckv::Color::rgb(102, 102, 102),
            ckv::Color::rgb(241, 76, 76),    ckv::Color::rgb(35, 209, 139),  ckv::Color::rgb(245, 245, 67),
            ckv::Color::rgb(59, 142, 234),   ckv::Color::rgb(214, 112, 214), ckv::Color::rgb(41, 184, 219),
            ckv::Color::rgb(255, 255, 255)};
        const int levels[6] = {0, 95, 135, 175, 215, 255};
        for (int r = 0; r < 6; ++r)
            for (int g = 0; g < 6; ++g)
                for (int b = 0; b < 6; ++b)
                    table.push_back(ckv::Color::rgb(static_cast<std::uint8_t>(levels[r]),
                                                    static_cast<std::uint8_t>(levels[g]),
                                                    static_cast<std::uint8_t>(levels[b])));
        for (int i = 0; i < 24; ++i) {
            const auto v = static_cast<std::uint8_t>(8 + i * 10);
            table.push_back(ckv::Color::rgb(v, v, v));
        }
        return table;
    }();
    for (std::size_t i = 0; i < palette.size(); ++i)
        if (palette[i] == color) return static_cast<int>(i);
    return -1;
}

std::string colour_name(ckv::Color color, ckv::Color default_color) {
    if (color == default_color) return "default";
    const int index = palette_index_of(color);
    if (index >= 0) return std::to_string(index);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", color.r(), color.g(), color.b());
    return buffer;
}

std::string attribute_names(ckv::Style style) {
    std::string out;
    const auto add = [&out](const char* name) {
        if (!out.empty()) out += "+";
        out += name;
    };
    if (has_attr(style.attrs, ckv::Attr::Bold)) add("bold");
    if (has_attr(style.attrs, ckv::Attr::Dim)) add("dim");
    if (has_attr(style.attrs, ckv::Attr::Italic)) add("italic");
    if (has_attr(style.attrs, ckv::Attr::Underline)) add("underline");
    if (has_attr(style.attrs, ckv::Attr::Reverse)) add("reverse");
    if (has_attr(style.attrs, ckv::Attr::Strike)) add("strike");
    return out.empty() ? "-" : out;
}

}  // namespace

int main(int argc, char** argv) {
    int columns = 20;
    int rows = 3;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--columns") columns = std::atoi(argv[i + 1]);
        else if (flag == "--rows") rows = std::atoi(argv[i + 1]);
    }

    std::string script;
    for (int ch = std::getchar(); ch != EOF; ch = std::getchar())
        script.push_back(static_cast<char>(ch));

    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{columns, rows};
    ckv::term::TerminalEmulator emulator(profile);
    emulator.feed_output(script);

    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    // One line per run of identical styling, so a dump reads as "these cells
    // look like this" rather than as a wall of per-cell records.
    for (int y = 0; y < snapshot.cells.height; ++y) {
        int run_start = 0;
        for (int x = 1; x <= snapshot.cells.width; ++x) {
            const ckv::Cell& previous = snapshot.cell_buffer[static_cast<std::size_t>(y * snapshot.cells.width + run_start)];
            const bool ends = x == snapshot.cells.width ||
                              snapshot.cell_buffer[static_cast<std::size_t>(y * snapshot.cells.width + x)].style() !=
                                  previous.style();
            if (!ends) continue;
            std::string text;
            for (int i = run_start; i < x; ++i)
                text += snapshot.cell_buffer[static_cast<std::size_t>(y * snapshot.cells.width + i)].grapheme();
            std::printf("%d %d-%d fg=%s bg=%s attrs=%s text=[%s]\n", y, run_start, x - 1,
                        colour_name(previous.style().fg, profile.default_style.fg).c_str(),
                        colour_name(previous.style().bg, profile.default_style.bg).c_str(),
                        attribute_names(previous.style()).c_str(), text.c_str());
            run_start = x;
        }
    }
    return 0;
}
