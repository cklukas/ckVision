// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Writes the per-widget figures the widget gallery embeds: one focused
// cut-out per public view, taken out of a full screen that a real
// Application composed through HeadlessTerminal and the Presenter.
//
// The scenes live in widget_shots_*.cpp, one function per widget, and
// the gallery quotes those functions as each widget's usage example, so
// this program is simultaneously the screenshot source and the code
// sample source. Neither can drift from the other: the sample is the
// code that drew the picture.
#include <cstdio>
#include <filesystem>

#include "widget_shots.hpp"
#include "widget_stage.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path out_dir = argv[1];
    std::filesystem::create_directories(out_dir);

    ckv::docgen::capture_control_shots(out_dir);
    ckv::docgen::capture_text_shots(out_dir);
    ckv::docgen::capture_data_shots(out_dir);
    ckv::docgen::capture_chrome_shots(out_dir);
    ckv::docgen::capture_composite_shots(out_dir);

    std::fprintf(stderr, "wrote %d widget figures to %s\n", ckv::docgen::screenshots_written(),
                 out_dir.string().c_str());
    return 0;
}
