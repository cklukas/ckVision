// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Cell-level golden evidence for the private terminal model.  The fixture
// includes primary/alternate-buffer transitions and cursor/style state, so a
// redraw can be checked independently from the outer presenter bytes.
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/terminal_emulator.hpp"

namespace {

std::string read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

std::string capture(const ckv::term::TerminalSnapshot& snapshot) {
    ckv::scene::Surface surface(snapshot.cells, ckv::Cell{});
    for (int row = 0; row < snapshot.cells.height; ++row) {
        for (int column = 0; column < snapshot.cells.width; ++column) {
            surface.set_cell(ckv::Point{column, row},
                             snapshot.cell_buffer[static_cast<std::size_t>(row * snapshot.cells.width + column)]);
        }
    }
    return ckv::golden::serialize(ckv::scene::capture(surface, snapshot.cursor));
}

std::string terminal_cell_manifest() {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{8, 3};
    ckv::term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[?25lprimary\r\n");
    std::string result = "primary-buffer-before-restore\n";
    result += capture(emulator.snapshot());
    emulator.feed_output("\x1b[?1049h\x1b[2J\x1b[1;1H\x1b[32mALT\x1b[?25h");
    result += "\nalternate-buffer\n";
    result += capture(emulator.snapshot());
    emulator.feed_output("\x1b[?1049l");
    result += "\nprimary-buffer-after-restore\n";
    result += capture(emulator.snapshot());
    return result;
}

std::uint64_t raster_hash(const ckv::Image& image) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const ckv::Image::Rgba pixel = image.pixel(x, y);
            for (const std::uint8_t byte : {pixel.r, pixel.g, pixel.b, pixel.a}) {
                hash ^= byte;
                hash *= 1099511628211ULL;
            }
        }
    }
    return hash;
}

std::string terminal_child_sixel_manifest() {
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{4, 3};
    profile.cell_pixels = ckv::Size{4, 6};
    ckv::term::TerminalEmulator emulator(profile);
    emulator.feed_output("\x1b[2;2H\x1bPq#0;2;100;0;0!8~-!8~\x1b\\");
    const ckv::term::TerminalSnapshot snapshot = emulator.snapshot();
    if (snapshot.rasters.size() != 1U || snapshot.rasters[0].image == nullptr) return {};
    const ckv::Image& image = *snapshot.rasters[0].image;
    std::ostringstream result;
    result << "child-sixel pixels " << image.width() << ' ' << image.height() << " anchor "
           << snapshot.rasters[0].anchor.x << ' ' << snapshot.rasters[0].anchor.y << " hash " << std::hex
           << std::setfill('0') << std::setw(16) << raster_hash(image) << '\n';
    return result.str();
}

}  // namespace

CK_TEST(terminal_cell_golden_covers_primary_alternate_and_restored_buffers) {
    CK_CHECK(terminal_cell_manifest() == read_file("golden/terminal_cells.visual"));
}

CK_TEST(terminal_child_sixel_pixel_golden_covers_the_private_decoder_output) {
    CK_CHECK(terminal_child_sixel_manifest() == read_file("golden/terminal_child_sixel.visual"));
}
