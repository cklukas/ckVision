// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "event_script.hpp"

#include <fstream>
#include <utility>

#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"

namespace ckv::docgen {

ScriptPlayer::ScriptPlayer(term::HeadlessTerminal& terminal, ui::Application& app,
                           std::vector<ScriptBeat> beats)
    : terminal_(terminal), app_(app), beats_(std::move(beats)) {}

const ScriptBeat* ScriptPlayer::play_to(std::string_view name) {
    std::size_t target = next_;
    while (target < beats_.size() && beats_[target].name != name) ++target;
    if (target == beats_.size()) return nullptr;
    while (next_ <= target) play(beats_[next_++]);
    return &beats_[target];
}

const ScriptBeat* ScriptPlayer::play_next() {
    if (next_ == beats_.size()) return nullptr;
    play(beats_[next_]);
    return &beats_[next_++];
}

void ScriptPlayer::play(const ScriptBeat& beat) {
    for (const term::TerminalEvent& event : beat.input) {
        terminal_.inject_event(event);
        app_.step(0);
    }
    // The loop keeps turning after the last event: this step runs whatever
    // that input posted (a closed window's detach, for one), so every beat
    // ends on the frame a reader is left looking at.
    app_.step(0);
}

std::string capture_frame(const ui::Application& app) {
    return golden::serialize(scene::capture(app.composed_surface(), app.current_cursor()));
}

void write_frame(const std::filesystem::path& directory, std::string_view file_name,
                 const ui::Application& app) {
    std::ofstream output(directory / std::filesystem::path(std::string(file_name)),
                         std::ios::binary);
    output << capture_frame(app);
}

int write_script_goldens(ScriptPlayer& player, const ui::Application& app,
                         const std::filesystem::path& directory) {
    int written = 0;
    while (const ScriptBeat* beat = player.play_next()) {
        if (beat->golden.empty()) continue;
        write_frame(directory, beat->golden, app);
        ++written;
    }
    return written;
}

KeyEvent key(Key key, Modifier modifiers) { return KeyEvent{KeyChord{key, modifiers, ""}}; }

KeyEvent alt(std::string_view letter) {
    return KeyEvent{KeyChord{Key::Char, Modifier::Alt, std::string(letter)}};
}

KeyEvent character(std::string_view text) {
    return KeyEvent{KeyChord{Key::Char, Modifier::None, std::string(text)}};
}

TextEvent typed(std::string text) { return TextEvent{std::move(text), false}; }

namespace {

MouseEvent pointer(MouseAction action, Point cell, MouseButton button) {
    return MouseEvent{.action = action,
                      .button = button,
                      .cell = cell,
                      .pixel = std::nullopt,
                      .modifiers = Modifier::None};
}

}  // namespace

MouseEvent press(Point cell, MouseButton button) {
    return pointer(MouseAction::Down, cell, button);
}

MouseEvent release(Point cell, MouseButton button) {
    return pointer(MouseAction::Up, cell, button);
}

MouseEvent move(Point cell, MouseButton button) {
    return pointer(MouseAction::Move, cell, button);
}

MouseEvent wheel(Point cell, MouseButton notch) {
    return pointer(MouseAction::Wheel, cell, notch);
}

}  // namespace ckv::docgen
