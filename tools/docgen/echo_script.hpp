// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// The Echo example's scripted session (the roadmap M3 "interactive echo
// demo"). Unlike the event scripts in event_script.hpp, every beat here is
// raw host bytes -- the exact sequences a terminal sends for a key, a mouse
// report, a bracketed paste or a focus report -- fed through
// HeadlessTerminal's input decoder, because what the echo shows is what the
// decoder made of those bytes. Played by tests/test_echo_smoke.cpp and by
// generate_echo_goldens, so the pinned frames and the test cannot drift.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "echo_app.hpp"

namespace ckv::docgen {

// One run of the echo example on an 80x24 headless terminal with the
// baseline capability profile, its first frame already presented.
struct EchoSession {
    EchoSession();

    term::HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    echo::EchoApp echo{app};
};

// One beat: host bytes decoded at the clock's current reading, then an
// optional wait on the clock (which is what resolves a lone Esc and closes a
// bracketed paste), then an optional terminal resize. Each beat ends with
// the frame it leaves.
struct EchoBeat {
    std::string name;
    std::string bytes;
    std::int64_t wait_nanos = 0;
    std::optional<Size> resize;
};

// typed, named_keys, alt_chord, lone_escape, mouse, paste, focus, resize.
std::vector<EchoBeat> echo_script();

// Plays `beat` against `session`.
void play(EchoSession& session, const EchoBeat& beat);

}  // namespace ckv::docgen
