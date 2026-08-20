// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// One-shot generator for the command table embedded in
// docs/standard-commands.md — NOT run by the test suite or CI; run by
// hand (and only by hand, deliberately, same as any golden-fixture
// regeneration) whenever the standard set changes, then the new table
// is reviewed and pasted into the doc like any other source change.
// Prints Markdown to stdout.
//
// A freshly constructed registry holds exactly the standard set —
// nothing else has had the chance to declare anything into it — so
// enumerating it IS the set, in declaration order. Ids are deliberately
// absent from the table: they are assigned at runtime and mean nothing
// outside the registry that assigned them, so the key is the name worth
// printing.
#include <cstdio>

#include "cvision/core/key.hpp"
#include "cvision/ui/command.hpp"

using ckv::format;
using ckv::ui::CommandRegistry;

int main() {
    const CommandRegistry registry;

    std::printf("| Key | Title | Category | Default chord |\n");
    std::printf("|---|---|---|---|\n");
    for (const auto& info : registry.all()) {
        const std::string chord = info.default_chord ? format(*info.default_chord) : "—";
        std::printf("| `%s` | `%s` | %s | %s |\n", info.key.c_str(), info.title.c_str(),
                    info.category.c_str(), chord.c_str());
    }
    return 0;
}
