// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../../tests/completion_test_fixture.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: generate_completion_goldens <output.dump|output-directory>\n";
        return 2;
    }
    ckv::testing::CompletionFixture fixture;
    fixture.field->set_completions({"report.txt"});
    fixture.field->set_text("rep");
    fixture.app.step(0);
    std::filesystem::path target(argv[1]);
    if (std::filesystem::is_directory(target)) target /= "completion_preview.dump";
    std::ofstream output(target, std::ios::binary);
    output << ckv::golden::serialize(
        ckv::scene::capture(fixture.app.composed_surface(), fixture.app.current_cursor()));
    return output ? 0 : 1;
}
