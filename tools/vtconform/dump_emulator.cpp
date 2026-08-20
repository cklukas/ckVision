// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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

// A colour is named as what it is. The emulator keeps a palette colour as its
// index, so the dump prints that index directly: the reference terminal
// reports indices too, and a comparison of indices is a comparison of what
// each side was told rather than of how each side chose to render it.
std::string colour_name(ckv::Color color, ckv::Color default_color) {
    if (color == default_color || color.is_default()) return "default";
    if (color.is_indexed()) return std::to_string(color.index());
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x", color.r(), color.g(), color.b());
    return buffer;
}

std::string attribute_names(ckv::Style style) {
    std::string out;
    const auto add = [&out](const std::string& name) {
        if (!out.empty()) out += "+";
        out += name;
    };
    if (has_attr(style.attrs, ckv::Attr::Bold)) add("bold");
    if (has_attr(style.attrs, ckv::Attr::Dim)) add("dim");
    if (has_attr(style.attrs, ckv::Attr::Italic)) add("italic");
    if (has_attr(style.attrs, ckv::Attr::Underline)) {
        std::string name = "underline";
        switch (style.underline) {
            case ckv::UnderlineShape::Straight: break;
            case ckv::UnderlineShape::Double: name += ":double"; break;
            case ckv::UnderlineShape::Curly: name += ":curly"; break;
            case ckv::UnderlineShape::Dotted: name += ":dotted"; break;
            case ckv::UnderlineShape::Dashed: name += ":dashed"; break;
        }
        if (!style.underline_color.is_default())
            name += "@" + colour_name(style.underline_color, ckv::Color::default_color());
        add(name);
    }
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
