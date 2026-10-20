// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../../tests/completion_app_test_fixture.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
#include "cvision/scene/svg_snapshot.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void write(const std::filesystem::path& output, ckv::ui::Application& app) {
    std::ofstream stream(output, std::ios::binary);
    stream << ckv::golden::serialize(ckv::scene::capture(app.composed_surface(), app.current_cursor()));
    if (!stream) throw std::runtime_error("cannot write golden");
    auto svg = output; svg.replace_extension(".svg");
    std::ofstream figure(svg, std::ios::binary);
    figure << ckv::scene::render_svg_snapshot(app.composed_surface().view());
    if (!figure) throw std::runtime_error("cannot write figure");
}
}
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: generate_path_dialog_goldens <output-directory>\n"; return 2; }
    const std::filesystem::path root(argv[1]);
    ckv::testing::PathDialogFixture file;
    ckv::widgets::FileDialogOptions options; options.filters = {{"Text", {"txt"}}};
    file.open(options); file.type("rep");
    write(root / "file_completion.dump", file.app);
    ckv::testing::PathDialogFixture folder;
    folder.attach(ckv::widgets::make_directory_picker(folder.source, "/home/user", folder.roles, folder.app, nullptr, nullptr));
    folder.type("do");
    write(root / "folder_completion.dump", folder.app);
    ckv::testing::CompletionAppFixture demo; demo.tick();
    write(root / "completion_app.dump", demo.app);
    return 0;
}
