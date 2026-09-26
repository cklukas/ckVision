// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "plane_capture.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <utility>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/surface.hpp"

namespace ckv::docgen {

term::Capabilities script_sixel_profile() {
    term::Capabilities caps = term::headless_sixel_profile();
    caps.cell_pixels = kScriptCellPixels;
    return caps;
}

term::Capabilities script_no_graphics_profile() {
    term::Capabilities caps = term::headless_no_graphics_profile();
    caps.cell_pixels = kScriptCellPixels;
    return caps;
}

std::string capture_scene(const ui::Application& app) {
    return golden::serialize(scene::capture_frame(app.compositor(), app.current_cursor()));
}

std::string frame_cells_dump(FrameView frame, CursorState cursor) {
    scene::Surface surface(frame.size(), Cell{});
    for (int y = 0; y < frame.size().height; ++y)
        for (int x = 0; x < frame.size().width; ++x)
            surface.set_cell(Point{x, y}, frame.at(Point{x, y}), frame.link_target(Point{x, y}));
    return golden::serialize(scene::capture(surface, cursor));
}

std::string display_cells_dump(const term::VirtualDisplay& display) {
    return frame_cells_dump(display.frame(), display.cursor());
}

std::string rgba_plane_bytes(const Image& plane) {
    std::string bytes = "ckvision-rgba 1\n" + std::to_string(plane.width()) + ' ' +
                        std::to_string(plane.height()) + '\n';
    bytes.reserve(bytes.size() + static_cast<std::size_t>(plane.width()) *
                                     static_cast<std::size_t>(plane.height()) * 4U);
    for (int y = 0; y < plane.height(); ++y) {
        for (int x = 0; x < plane.width(); ++x) {
            const Image::Rgba pixel = plane.pixel(x, y);
            for (const std::uint8_t channel : {pixel.r, pixel.g, pixel.b, pixel.a})
                bytes.push_back(static_cast<char>(channel));
        }
    }
    return bytes;
}

bool write_golden(const std::filesystem::path& directory, std::string_view file_name,
                  std::string_view bytes) {
    std::ofstream output(directory / std::filesystem::path(std::string(file_name)), std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

std::string read_golden(const std::filesystem::path& directory, std::string_view file_name) {
    std::ifstream input(directory / std::filesystem::path(std::string(file_name)), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

PairedRasterBeat capture_paired_raster_beat(std::string stem, const ui::Application& sixel_app,
                                            const term::HeadlessTerminal& sixel_terminal,
                                            const ui::Application& fallback_app,
                                            const term::HeadlessTerminal& fallback_terminal) {
    PairedRasterBeat beat;
    beat.stem = std::move(stem);
    beat.scene = capture_scene(sixel_app);
    beat.fallback_scene = capture_scene(fallback_app);
    beat.sixel_cells = display_cells_dump(sixel_terminal.display());
    beat.sixel_pixels = rgba_plane_bytes(sixel_terminal.display().raster_plane());
    beat.fallback_cells = display_cells_dump(fallback_terminal.display());
    beat.fallback_has_pixels = fallback_terminal.display().has_raster_pixels();
    return beat;
}

bool write_paired_raster_beat(const std::filesystem::path& directory, const PairedRasterBeat& beat) {
    if (beat.scene != beat.fallback_scene || beat.fallback_has_pixels) return false;
    return write_golden(directory, beat.stem + ".scene", beat.scene) &&
           write_golden(directory, beat.stem + ".dump", beat.sixel_cells) &&
           write_golden(directory, beat.stem + ".rgba", beat.sixel_pixels) &&
           write_golden(directory, beat.stem + "_no_graphics.dump", beat.fallback_cells);
}

bool matches_pinned(const PairedRasterBeat& beat, const std::filesystem::path& directory) {
    const std::string scene = read_golden(directory, beat.stem + ".scene");
    return !scene.empty() && beat.scene == scene && beat.fallback_scene == scene &&
           beat.sixel_cells == read_golden(directory, beat.stem + ".dump") &&
           beat.sixel_pixels == read_golden(directory, beat.stem + ".rgba") &&
           beat.fallback_cells == read_golden(directory, beat.stem + "_no_graphics.dump") &&
           !beat.fallback_has_pixels;
}

}  // namespace ckv::docgen
