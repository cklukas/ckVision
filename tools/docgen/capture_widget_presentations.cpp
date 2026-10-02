// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
// Documentation figures reuse the executable appearance specimens. Each
// geometry variant uses Classic so color cannot obscure the layout change.
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "appearance_matrix.hpp"
#include "frame_svg.hpp"

namespace {
struct Figure { std::string_view widget; std::string_view state; std::string_view filename; };
constexpr std::array figures{
    Figure{"Button", "normal", "widget-button-presentation-normal"},
    Figure{"Button", "flat-normal", "widget-button-presentation-flat-normal"},
    Figure{"Button", "padded-normal", "widget-button-presentation-padded-normal"},
    Figure{"Button", "outlined-normal", "widget-button-presentation-outlined-normal"},
    Figure{"InputLine", "normal", "widget-inputline-presentation-normal"},
    Figure{"InputLine", "padded-normal", "widget-inputline-presentation-padded-normal"},
    Figure{"InputLine", "underlined-normal", "widget-inputline-presentation-underlined-normal"},
    Figure{"ComboBox", "normal", "widget-combobox-presentation-normal"},
    Figure{"ComboBox", "padded-normal", "widget-combobox-presentation-padded-normal"},
    Figure{"ComboBox", "underlined-normal", "widget-combobox-presentation-underlined-normal"},
    Figure{"SearchBox", "query", "widget-searchbox-presentation-query"},
    Figure{"SearchBox", "padded-query", "widget-searchbox-presentation-padded-query"},
    Figure{"SearchBox", "underlined-query", "widget-searchbox-presentation-underlined-query"},
    Figure{"Progress", "normal", "widget-progress-presentation-normal"},
    Figure{"Progress", "block-normal", "widget-progress-presentation-block-normal"},
    Figure{"Progress", "segmented-normal", "widget-progress-presentation-segmented-normal"},
    Figure{"Progress", "percentage", "widget-progress-presentation-percentage"},
    Figure{"Slider", "normal", "widget-slider-presentation-normal"},
    Figure{"Slider", "block-normal", "widget-slider-presentation-block-normal"},
    Figure{"Slider", "prominent-normal", "widget-slider-presentation-prominent-normal"},
    Figure{"Slider", "line-value-normal", "widget-slider-presentation-line-value-normal"},
    Figure{"CheckGroup", "normal", "widget-checkgroup-presentation-normal"},
    Figure{"CheckGroup", "boxed-normal", "widget-checkgroup-presentation-boxed-normal"},
    Figure{"CheckGroup", "buttons-normal", "widget-checkgroup-presentation-buttons-normal"},
    Figure{"RadioGroup", "normal", "widget-radiogroup-presentation-normal"},
    Figure{"RadioGroup", "boxed-normal", "widget-radiogroup-presentation-boxed-normal"},
    Figure{"RadioGroup", "buttons-normal", "widget-radiogroup-presentation-buttons-normal"},
    Figure{"SpinBox", "compact-normal", "widget-spinbox-presentation-compact-normal"},
    Figure{"SpinBox", "separate-normal", "widget-spinbox-presentation-separate-normal"},
    Figure{"SpinBox", "stacked-normal", "widget-spinbox-presentation-stacked-normal"},
    Figure{"StatusLine", "normal", "widget-statusline-presentation-normal"},
    Figure{"StatusLine", "grouped-normal", "widget-statusline-presentation-grouped-normal"},
    Figure{"BreadcrumbBar", "normal", "widget-breadcrumbbar-presentation-normal"},
    Figure{"BreadcrumbBar", "padded-normal", "widget-breadcrumbbar-presentation-padded-normal"},
    Figure{"BreadcrumbBar", "connected-normal", "widget-breadcrumbbar-presentation-connected-normal"},
    Figure{"Splitter", "normal", "widget-splitter-presentation-normal"},
    Figure{"Splitter", "grip-normal", "widget-splitter-presentation-grip-normal"},
    Figure{"Splitter", "gutter-normal", "widget-splitter-presentation-gutter-normal"},
    Figure{"ListView", "normal", "widget-listview-presentation-normal"},
    Figure{"ListView", "banded-normal", "widget-listview-presentation-banded-normal"},
    Figure{"Table", "normal", "widget-table-presentation-normal"},
    Figure{"Table", "banded-normal", "widget-table-presentation-banded-normal"},
    Figure{"Table", "divided-normal", "widget-table-presentation-divided-normal"},
    Figure{"Table", "banded-divided-normal", "widget-table-presentation-banded-divided-normal"},
    Figure{"NotificationCenter", "normal", "widget-notificationcenter-presentation-normal"},
    Figure{"NotificationCenter", "banners-normal", "widget-notificationcenter-presentation-banners-normal"},
    Figure{"NotificationCenter", "framed-normal", "widget-notificationcenter-presentation-framed-normal"},
    Figure{"PropertyInspector", "normal", "widget-propertyinspector-presentation-normal"},
    Figure{"PropertyInspector", "divided-normal", "widget-propertyinspector-presentation-divided-normal"},
    Figure{"PropertyInspector", "sectioned-normal", "widget-propertyinspector-presentation-sectioned-normal"},
    Figure{"PropertyInspector", "banded-normal", "widget-propertyinspector-presentation-banded-normal"},
    Figure{"Wizard", "compact-normal", "widget-wizard-presentation-compact-normal"},
    Figure{"Wizard", "bands-normal", "widget-wizard-presentation-bands-normal"},
    Figure{"Wizard", "rail-normal", "widget-wizard-presentation-rail-normal"},
};
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: capture_widget_presentations <output-directory>\n");
        return 2;
    }
    try {
        using namespace ckv::docgen::appearance;
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        Catalog catalog;
        add_control_specimens(catalog);
        add_data_specimens(catalog);
        add_chrome_specimens(catalog);
        add_component_specimens(catalog);
        for (const auto& figure : figures) {
            const auto element = std::find_if(catalog.elements.begin(), catalog.elements.end(),
                [&](const Element& candidate) { return candidate.name == figure.widget; });
            if (element == catalog.elements.end()) throw std::runtime_error("Unknown presentation widget");
            const auto state = std::find_if(element->states.begin(), element->states.end(),
                [&](const State& candidate) { return candidate.name == figure.state; });
            if (state == element->states.end()) throw std::runtime_error("Unknown presentation state");
            Stage stage(state->screen, Scheme::Classic, state->graphics);
            state->build(stage);
            stage.step();
            ckv::docgen::FrameSvgOptions options;
            if (figure.widget == "StatusLine") {
                options.crop = ckv::Rect{0, state->screen.height - 1, state->screen.width, 1};
            } else if (const auto* window = stage.desktop().active_window()) {
                const auto rect = window->absolute_bounds();
                options.crop = ckv::Rect{rect.x - 1, rect.y - 1, rect.width + 3, rect.height + 3};
            }
            std::ofstream output(directory / (std::string(figure.filename) + ".svg"), std::ios::binary);
            output << ckv::docgen::render_virtual_display_svg(stage.terminal().display(), options);
            if (!output) throw std::runtime_error("Cannot write presentation figure");
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "capture_widget_presentations: %s\n", error.what());
        return 1;
    }
    return 0;
}
