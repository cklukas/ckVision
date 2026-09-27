// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Renders the appearance matrix (appearance_matrix.hpp) into a directory:
//
//   <out>/catalog.txt                      what was rendered, and what was exempt
//   <out>/<Element>/<state>.<scheme>.dump   the terminal's visible cells
//   <out>/<Element>/<state>.<scheme>.scene  a raster element's composed scene,
//                                           with its `raster` records
//   <out>/<Element>/<state>.rgba            a Sixel state's decoded pixel plane
//
// The `.dump` is what the presenter put on the terminal; the `.scene` is the
// symbolic oracle one step earlier (docs/golden-format.md), captured from the
// compositor so that the occlusion-sliced pictures are in it. Every state of
// a raster-bearing element has one.
//
// A Sixel state's picture does not depend on the scheme, so it is written
// once, and this generator fails unless all four schemes decoded exactly the
// same plane. A no-graphics state must leave the plane empty. Run by hand
// into tests/golden/appearance, reviewed, and committed;
// tests/check_appearance_matrix.py regenerates and compares on every host.
//
// `--svg <dir>` also writes each rendered frame as `<dir>/<Element>/<state>.
// <scheme>.svg` for a person to look at while reviewing the goldens. The
// pictures are not pinned: the dumps are the record, and the SVGs only show
// what they say.
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "appearance_matrix.hpp"
#include "frame_svg.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/surface.hpp"

namespace {

using namespace ckv;
using namespace ckv::docgen::appearance;

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    if (!output) throw std::runtime_error("cannot write " + path.string());
}

std::string visible_cells(const term::VirtualDisplay& display) {
    const FrameView frame = display.frame();
    scene::Surface surface(frame.size(), Cell{});
    for (int y = 0; y < frame.size().height; ++y)
        for (int x = 0; x < frame.size().width; ++x) surface.set_cell(Point{x, y}, frame.at(Point{x, y}), frame.link_target(Point{x, y}));
    return golden::serialize(scene::capture(surface, display.cursor()));
}

std::string plane_bytes(const Image& image) {
    std::string bytes = "ckvision-rgba 1\n" + std::to_string(image.width()) + ' ' + std::to_string(image.height()) + '\n';
    bytes.reserve(bytes.size() + static_cast<std::size_t>(image.width() * image.height() * 4));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const Image::Rgba pixel = image.pixel(x, y);
            bytes.push_back(static_cast<char>(pixel.r));
            bytes.push_back(static_cast<char>(pixel.g));
            bytes.push_back(static_cast<char>(pixel.b));
            bytes.push_back(static_cast<char>(pixel.a));
        }
    }
    return bytes;
}

std::string traits_text(const Traits& traits) {
    std::string text;
    if (traits.focusable) text += " focusable";
    if (traits.control) text += " control";
    if (traits.text) text += " text";
    if (traits.window) text += " window";
    if (traits.raster) text += " raster";
    return text.empty() ? " plain" : text;
}

void render(const std::filesystem::path& root, const std::optional<std::filesystem::path>& previews) {
    Catalog catalog;
    add_control_specimens(catalog);
    add_data_specimens(catalog);
    add_chrome_specimens(catalog);
    add_component_specimens(catalog);
    add_media_specimens(catalog);

    std::ostringstream index;
    index << "ckvision-appearance-catalog 1\n";
    index << "schemes";
    for (Scheme scheme : kSchemes) index << ' ' << scheme_name(scheme);
    index << '\n';
    for (const Element& element : catalog.elements) {
        index << "element " << element.name << ' ' << element.header << traits_text(element.traits) << '\n';
        const std::filesystem::path directory = root / element.name;
        std::filesystem::create_directories(directory);
        for (const State& state : element.states) {
            const bool sixel = state.graphics == Graphics::Sixel;
            std::optional<std::string> plane;
            for (Scheme scheme : kSchemes) {
                Stage stage(state.screen, scheme, state.graphics);
                state.build(stage);
                stage.step();
                const term::VirtualDisplay& display = stage.terminal().display();
                const std::string where = element.name + '/' + state.name + '.' + std::string(scheme_name(scheme));
                write_text(directory / (state.name + '.' + std::string(scheme_name(scheme)) + ".dump"),
                           visible_cells(display));
                if (element.traits.raster)
                    write_text(directory / (state.name + '.' + std::string(scheme_name(scheme)) + ".scene"),
                               golden::serialize(scene::capture_frame(stage.app().compositor(),
                                                                      stage.app().current_cursor())));
                if (previews) {
                    std::filesystem::create_directories(*previews / element.name);
                    write_text(*previews / element.name / (state.name + '.' + std::string(scheme_name(scheme)) + ".svg"),
                               docgen::render_virtual_display_svg(display));
                }
                if (sixel != display.has_raster_pixels())
                    throw std::runtime_error(where + (sixel ? ": the Sixel state decoded no picture"
                                                            : ": the no-graphics state left raster pixels"));
                if (!sixel) continue;
                std::string bytes = plane_bytes(display.raster_plane());
                if (plane && *plane != bytes)
                    throw std::runtime_error(where + ": the decoded picture differs between schemes");
                plane = std::move(bytes);
            }
            if (plane) write_text(directory / (state.name + ".rgba"), *plane);
            index << "state " << element.name << ' ' << state.name << ' ' << state.screen.width << 'x'
                  << state.screen.height << ' ' << (sixel ? "sixel" : "none") << '\n';
            if (!state.fixed_reason.empty())
                index << "fixed " << element.name << ' ' << state.name << ' ' << state.fixed_reason << '\n';
        }
    }
    for (const Exemption& exemption : catalog.exemptions)
        index << "exempt " << exemption.name << ' ' << exemption.reason << '\n';
    write_text(root / "catalog.txt", index.str());
}

}  // namespace

int main(int argc, char** argv) {
    const bool with_previews = argc == 4 && std::string_view(argv[2]) == "--svg";
    if (argc != 2 && !with_previews) {
        std::fprintf(stderr, "usage: generate_appearance_matrix <output-directory> [--svg <preview-directory>]\n");
        return 2;
    }
    try {
        const std::filesystem::path root = argv[1];
        std::filesystem::create_directories(root);
        render(root, with_previews ? std::optional<std::filesystem::path>(argv[3]) : std::nullopt);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "generate_appearance_matrix: %s\n", error.what());
        return 1;
    }
    return 0;
}
