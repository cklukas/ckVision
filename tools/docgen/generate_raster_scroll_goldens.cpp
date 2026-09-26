// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the scrolled-raster goldens (D-081): plays
// raster_scroll_script() on a Sixel stage and a NoGraphics stage in lockstep
// and writes each beat's raster_scroll_*.scene, .dump, .rgba and
// _no_graphics.dump. Regenerating is running this program:
//   build/tools/docgen/generate_raster_scroll_goldens tests/golden
// and reviewing the diff like any other source change. The
// generated_golden_bytes test runs it on every host and compares the bytes.
#include <cstdio>
#include <filesystem>

#include "plane_capture.hpp"
#include "raster_scroll_script.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    ckv::docgen::RasterScrollStage sixel(ckv::docgen::script_sixel_profile());
    ckv::docgen::RasterScrollStage fallback(ckv::docgen::script_no_graphics_profile());
    if (ckv::docgen::write_paired_raster_goldens(sixel, fallback, directory) < 0) {
        std::fprintf(stderr, "scrolled-raster capture failed\n");
        return 1;
    }
    return 0;
}
