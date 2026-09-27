// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Fixture generator for the Echo example (the roadmap M3 "interactive echo
// demo"): the frame as the example opens, and the frame after the whole
// scripted input session of echo_script.hpp -- keys, an Alt chord, a lone
// Esc, mouse reports, a bracketed paste, host focus reports and a resize.
// tests/test_echo_smoke.cpp plays the same script; generated_golden_bytes
// regenerates these on every host.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "echo_script.hpp"

namespace {

void write_dump(const std::filesystem::path& directory, const std::string& name, const ckv::ui::Application& app) {
    const std::filesystem::path path = directory / (name + ".dump");
    std::ofstream output(path, std::ios::binary);
    output << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    std::fprintf(stderr, "wrote %s\n", path.string().c_str());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <tests/golden output directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    ckv::docgen::EchoSession session;
    write_dump(directory, "echo_initial", session.app);
    for (const ckv::docgen::EchoBeat& beat : ckv::docgen::echo_script()) ckv::docgen::play(session, beat);
    write_dump(directory, "echo_scripted", session.app);
    return 0;
}
