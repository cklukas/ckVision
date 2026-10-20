// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "path_dialog_test_fixture.hpp"
#include "completion_app.hpp"

namespace ckv::testing {
struct CompletionAppFixture {
    ScriptedDirectoryService source;
    term::HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    std::unique_ptr<completion_demo::CompletionApp> demo;
    CompletionAppFixture() {
        source.fs.add_directory("/home/user/docs");
        source.fs.add_file("/home/user/report.txt");
        source.fs.add_file("/home/user/report.csv");
        demo = std::make_unique<completion_demo::CompletionApp>(app, source, "/home/user");
    }
    void tick() { clock.advance(20'000'000); app.step(clock.now_nanos()); }
};
} // namespace ckv::testing
