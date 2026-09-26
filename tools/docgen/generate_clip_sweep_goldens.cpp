// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Manual fixture generator for the widget width sweeps (clip_sweep.hpp): one
// tests/golden/clip_sweep_<family>.dump per text-drawing widget family, each
// holding that family's frames at every width from 0 up, in order. Run by hand
// into tests/golden, reviewed, and committed; generated_golden_bytes
// regenerates and compares them on every host.
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "clip_sweep.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);
    for (const ckv::docgen::clip_sweep::Sweep& sweep : ckv::docgen::clip_sweep::render_all()) {
        std::ofstream output(directory / ("clip_sweep_" + sweep.family + ".dump"), std::ios::binary);
        output << ckv::golden::serialize(ckv::scene::capture(sweep.surface));
        if (!output) return 1;
    }
    return 0;
}
