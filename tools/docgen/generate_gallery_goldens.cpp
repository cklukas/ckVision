// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the Gallery's scripted goldens: the image
// demo scrolled and occluded by a menu (gallery_picture_*) and the four
// schemes chosen through View > Scheme (gallery_scheme_*). Each script is
// played on a Sixel stage and a NoGraphics stage in lockstep, and every beat
// writes its .scene, .dump, .rgba and _no_graphics.dump. Regenerating is
// running this program:
//   build/tools/docgen/generate_gallery_goldens tests/golden
// and reviewing the diff like any other source change. The
// generated_golden_bytes test runs it on every host and compares the bytes.
#include <cstdio>
#include <filesystem>
#include <vector>

#include "gallery_script.hpp"
#include "plane_capture.hpp"

namespace {

bool write_script(std::vector<ckv::docgen::ScriptBeat> (*script)(), const std::filesystem::path& directory) {
    ckv::docgen::GalleryStage sixel(ckv::docgen::script_sixel_profile(), script());
    ckv::docgen::GalleryStage fallback(ckv::docgen::script_no_graphics_profile(), script());
    return ckv::docgen::write_paired_raster_goldens(sixel, fallback, directory) >= 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    if (!write_script(ckv::docgen::gallery_picture_script, directory) ||
        !write_script(ckv::docgen::gallery_scheme_script, directory)) {
        std::fprintf(stderr, "gallery script capture failed\n");
        return 1;
    }
    return 0;
}
