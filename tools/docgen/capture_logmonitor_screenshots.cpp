// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../../tests/logmonitor_test_fixture.hpp"
#include "../../tests/native_log_source_test_fixture.hpp"
#include "frame_svg.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include "cvision/term/presenter.hpp"
namespace {
void capture(const std::filesystem::path& output, const std::filesystem::path& goldens,
             const std::string& name, ckv::ui::Application& app, ckv::term::HeadlessTerminal& terminal, const std::string& actual) {
    std::ifstream input(goldens / (name + ".dump"), std::ios::binary);
    const std::string expected((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!input || actual != expected) throw std::runtime_error("log-monitor screenshot does not match pinned golden: " + name);
    const auto composed = app.composed_surface().view(), decoded = terminal.display().frame();
    if (composed.size() != decoded.size()) throw std::runtime_error("decoded log-monitor frame size differs");
    std::vector<std::pair<ckv::Style, ckv::Style>> projected_styles;
    const auto projected_style = [&](const ckv::Style& style) {
        for (const auto& entry : projected_styles) if (entry.first == style) return entry.second;
        ckv::term::VirtualDisplay projection({1, 1});
        if (!projection.feed(ckv::term::style_to_sgr(style, terminal.capabilities()) + " ")) throw std::runtime_error("invalid style projection");
        const auto result = projection.frame().at({0, 0}).style(); projected_styles.emplace_back(style, result); return result;
    };
    for (int row = 0; row < composed.size().height; ++row) for (int column = 0; column < composed.size().width; ++column) {
        const ckv::Point point{column, row}; const auto& expected_cell = composed.at(point); const auto& actual_cell = decoded.at(point);
        if (expected_cell.grapheme() != actual_cell.grapheme() || expected_cell.width() != actual_cell.width() ||
            projected_style(expected_cell.style()) != actual_cell.style() || composed.link_target(point) != decoded.link_target(point))
            throw std::runtime_error("decoded log-monitor frame differs from pinned capability-projected composition: " + name + " at " + std::to_string(column) + "," + std::to_string(row));
    }
    std::string figure = name; std::replace(figure.begin(), figure.end(), '_', '-');
    std::ofstream image(output / (figure + ".svg"), std::ios::binary);
    image << ckv::docgen::render_virtual_display_svg(terminal.display());
    if (!image) throw std::runtime_error("cannot write log-monitor screenshot");
}
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path output(argv[1]), goldens(CKV_LOGMONITOR_GOLDENS);
    std::filesystem::create_directories(output);
    for (auto name : ckv::testing::logmonitor_golden_names) ckv::testing::with_logmonitor_scenario(name, [&](auto& fixture) {
        capture(output, goldens, "logmonitor_" + std::string(name), fixture.app, fixture.terminal, fixture.golden());
    });
    for (auto name : ckv::testing::native_log_golden_names) {
        ckv::testing::NativeLogFixture fixture{std::string(name)}; fixture.monitor.show_details(); fixture.settle();
        capture(output, goldens, "logmonitor_native_" + std::string(name), fixture.app, fixture.terminal, fixture.golden());
    }
    return 0;
}
