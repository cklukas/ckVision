// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Editor performance smoke: timing is diagnostic, while the exercised edit
// and incremental-highlighting invariants make this a deterministic CTest
// budget gate rather than a wall-clock threshold.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "ckbench.hpp"
#include "cvision/widgets/editor_document.hpp"
#include "cvision/widgets/syntax_cache.hpp"
#include "cvision/widgets/syntax_profile.hpp"
#include "cvision/widgets/text_editor.hpp"

bool run_editor_benchmarks() {
    std::string source;
    for (int line = 0; line < 4000; ++line)
        source += "item" + std::to_string(line) + ": value # deterministic editor benchmark\n";
    auto document = std::make_shared<ckv::widgets::EditorDocument>(source);
    ckv::widgets::SyntaxProfileRegistry profiles;
    ckv::widgets::register_standard_syntax_profiles(profiles);
    ckv::widgets::TextEditor editor(document, &profiles);
    editor.set_file_name("benchmark.yaml");
    editor.set_bounds(ckv::Rect{0, 0, 120, 40});

    std::vector<std::string> syntax_lines;
    syntax_lines.reserve(4000);
    for (int line = 0; line < 4000; ++line)
        syntax_lines.push_back("item" + std::to_string(line) + ": value # deterministic editor benchmark");
    ckv::widgets::SyntaxCache syntax_cache;
    const auto* yaml = profiles.find("yaml");
    if (yaml == nullptr || !syntax_cache.update(*yaml, syntax_lines).reached_fixed_point) return false;

    bool holds = true;
    ckbench::run("editor_local_replace_4000_lines", 200, [&] {
        const auto position = document->position_at_byte(0);
        if (!position) { holds = false; return; }
        const auto result = document->replace(ckv::widgets::DocumentRange{*position, *position}, "x");
        if (!result || !document->undo()) holds = false;
    });
    constexpr std::size_t kLocalRelexLineBudget = 2;
    bool alternate = false;
    ckbench::run("editor_incremental_highlight_4000_lines", 200, [&] {
        syntax_lines.front() = alternate ? "item0: one # deterministic editor benchmark"
                                         : "item0: two # deterministic editor benchmark";
        alternate = !alternate;
        const ckv::widgets::SyntaxRelexReport report = syntax_cache.update(*yaml, syntax_lines);
        if (!report.reached_fixed_point || report.first_line != 0U || report.line_count > kLocalRelexLineBudget ||
            editor.profile_id() != "yaml")
            holds = false;
    });
    std::printf("  (editor document bytes %zu, profile %s; local relex cap %zu lines)\n", document->byte_size(),
                editor.profile_id().c_str(), kLocalRelexLineBudget);
    return holds;
}
