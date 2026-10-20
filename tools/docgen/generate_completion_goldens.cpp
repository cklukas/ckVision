// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../../tests/completion_test_fixture.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: generate_completion_goldens <output.dump>\n";
        return 2;
    }
    ckv::testing::CompletionFixture fixture;
    fixture.field->set_completions({"report.txt"});
    fixture.field->set_text("rep");
    fixture.app.step(0);
    std::ofstream output(argv[1], std::ios::binary);
    output << ckv::golden::serialize(
        ckv::scene::capture(fixture.app.composed_surface(), fixture.app.current_cursor()));
    return output ? 0 : 1;
}
