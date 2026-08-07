// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// One-shot generator for the command table embedded in
// docs/standard-commands.md — NOT run by the test suite or CI; run by
// hand (and only by hand, deliberately, same as any golden-fixture
// regeneration) whenever ckv::ui::std_commands changes, then the new
// table is reviewed and pasted into the doc like any other source
// change. Prints Markdown to stdout.
#include <cstdio>

#include "cvision/core/key.hpp"
#include "cvision/ui/command.hpp"

using ckv::format;
using ckv::ui::CommandRegistry;

int main() {
    CommandRegistry registry;
    registry.register_standard_commands();

    std::printf("| Id | Title | Category | Default chord |\n");
    std::printf("|---|---|---|---|\n");
    for (const auto& info : registry.all()) {
        const std::string chord = info.default_chord ? format(*info.default_chord) : "—";
        std::printf("| %d | `%s` | %s | %s |\n", info.id, info.title.c_str(), info.category.c_str(),
                    chord.c_str());
    }
    return 0;
}
