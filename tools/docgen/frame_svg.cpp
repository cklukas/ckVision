// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "frame_svg.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include <stdexcept>
#include <vector>

namespace ckv::docgen {
void verify_decoded_cells(const ui::Application& app, const term::HeadlessTerminal& terminal) {
    const auto composed = app.composed_surface().view(), decoded = terminal.display().frame();
    if (composed.size() != decoded.size()) throw std::runtime_error("decoded screenshot size differs");
    std::vector<std::pair<Style, Style>> styles;
    const auto projected = [&](const Style& style) {
        for (const auto& pair : styles) if (pair.first == style) return pair.second;
        term::VirtualDisplay projection({1, 1});
        if (!projection.feed(term::style_to_sgr(style, terminal.capabilities()) + " "))
            throw std::runtime_error("invalid screenshot style projection");
        const auto result = projection.frame().at({0, 0}).style();
        styles.emplace_back(style, result); return result;
    };
    for (int y = 0; y < composed.size().height; ++y) for (int x = 0; x < composed.size().width; ++x) {
        const Point point{x, y}; const auto& expected = composed.at(point); const auto& actual = decoded.at(point);
        if (expected.grapheme() != actual.grapheme() || expected.width() != actual.width() ||
            projected(expected.style()) != actual.style() || composed.link_target(point) != decoded.link_target(point))
            throw std::runtime_error("decoded screenshot differs at " + std::to_string(x) + "," + std::to_string(y));
    }
}

std::string render_virtual_display_svg(const term::VirtualDisplay& display,
                                       scene::SvgSnapshotOptions options) {
    options.cell_pixels = display.cell_pixels();
    return scene::render_svg_snapshot(display.frame(), options, &display.raster_plane());
}

}  // namespace ckv::docgen
