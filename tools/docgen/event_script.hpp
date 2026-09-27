// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// An event script is what a reader does to an application, written down:
// named beats, each a run of terminal input. A ScriptPlayer injects every
// event through HeadlessTerminal and runs one Application::step for it, and
// one more once the beat's input is consumed, so the events take the same
// route real input takes -- decoded terminal events, the Application's
// dispatch, its focus and capture rules, its posted work, its frame. No beat
// calls a widget's handler directly.
//
// One script serves two readers. A golden generator plays it through and
// writes the frame of every beat that names a golden file; the test that
// owns it plays to each named beat, asserts what the reader should now see,
// and compares the frame with the pinned file. The sequence is therefore
// written once, and a generated golden cannot drift from the script its
// test runs (tests/check_generated_goldens.py compares the bytes on every
// host).
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/event.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::docgen {

// One beat of a script: its name, the input that makes it, and the golden
// file (a bare file name under tests/golden) that pins the frame it ends on,
// or empty when that frame is not pinned. A beat with no input is the idle
// step alone, which is how a script paints its first frame.
struct ScriptBeat {
    std::string name;
    std::vector<term::TerminalEvent> input;
    std::string golden;
};

// Plays a script's beats in order against `terminal` and `app`, which must
// outlive it.
class ScriptPlayer {
public:
    ScriptPlayer(term::HeadlessTerminal& terminal, ui::Application& app,
                 std::vector<ScriptBeat> beats);

    // Plays every beat up to and including the next one called `name`, and
    // returns it; nullptr, playing nothing, when no beat ahead has that name.
    const ScriptBeat* play_to(std::string_view name);
    // Plays the next beat; nullptr, once the script has ended.
    const ScriptBeat* play_next();

private:
    void play(const ScriptBeat& beat);

    term::HeadlessTerminal& terminal_;
    ui::Application& app_;
    std::vector<ScriptBeat> beats_;
    std::size_t next_ = 0;
};

// The composed frame as a golden dump, cursor included.
std::string capture_frame(const ui::Application& app);
// Writes capture_frame(app) to `directory`/`file_name`.
void write_frame(const std::filesystem::path& directory, std::string_view file_name,
                 const ui::Application& app);
// Plays the whole script and writes the frame of every beat that names a
// golden file. Returns how many were written.
int write_script_goldens(ScriptPlayer& player, const ui::Application& app,
                         const std::filesystem::path& directory);

// Input, spelled the way a script reads.
KeyEvent key(Key key, Modifier modifiers = Modifier::None);
KeyEvent alt(std::string_view letter);
// An unmodified printable key, such as the Space that toggles a checkbox.
KeyEvent character(std::string_view text);
TextEvent typed(std::string text);
MouseEvent press(Point cell, MouseButton button = MouseButton::Left);
MouseEvent release(Point cell, MouseButton button = MouseButton::Left);
// Motion with `button` held (a drag), or with nothing held (a hover).
MouseEvent move(Point cell, MouseButton button = MouseButton::None);
// One wheel notch over `cell`: MouseButton::WheelDown scrolls toward the end,
// WheelUp back toward the start.
MouseEvent wheel(Point cell, MouseButton notch);

}  // namespace ckv::docgen
