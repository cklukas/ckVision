// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../../tests/completion_app_test_fixture.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/scene/svg_snapshot.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void write(const std::filesystem::path& root, const char* name, ckv::ui::Application& app) {
    std::ofstream stream(root / (std::string(name) + ".svg"), std::ios::binary);
    stream << ckv::scene::render_svg_snapshot(app.composed_surface().view());
    if (!stream) throw std::runtime_error("cannot write completion screenshot");
}
}
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: capture_completion_screenshots <output-directory>\n"; return 2; }
    const std::filesystem::path root(argv[1]);
    ckv::testing::PathDialogFixture file;
    ckv::widgets::FileDialogOptions options; options.filters = {{"Text", {"txt"}}};
    file.open(options); file.type("rep"); write(root, "file-completion", file.app);
    ckv::testing::PathDialogFixture folder;
    folder.attach(ckv::widgets::make_directory_picker(folder.source, "/home/user", folder.roles, folder.app, nullptr, nullptr));
    folder.type("do"); write(root, "folder-completion", folder.app);
    ckv::testing::CompletionAppFixture demo; demo.tick(); write(root, "completion-app", demo.app);
    return 0;
}
