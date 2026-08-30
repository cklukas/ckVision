// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation tooling, NOT part of the cvision library: the per-widget
// figures the widget gallery embeds. Each group lives in its own
// translation unit, and each widget in its own scene function, because
// the gallery cites the scene's own source as that widget's usage
// example (`// ckvision-doc: <region>` markers, extracted by
// tools/docgen/extract_snippets.py). Code a reader copies out of the
// documentation is therefore the code that produced the picture beside
// it, compiled, run, and screenshotted on every documentation build.
#pragma once

#include <filesystem>

namespace ckv::docgen {

void capture_control_shots(const std::filesystem::path& dir);
void capture_text_shots(const std::filesystem::path& dir);
void capture_data_shots(const std::filesystem::path& dir);
void capture_chrome_shots(const std::filesystem::path& dir);
void capture_composite_shots(const std::filesystem::path& dir);

}  // namespace ckv::docgen
