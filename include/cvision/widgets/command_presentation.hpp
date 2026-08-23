// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <utility>

#include "cvision/ui/command.hpp"

namespace ckv::widgets {

// Widgets-layer command presentation (WP-35): menu and status surfaces can
// present one CommandId with surface-specific wording/mnemonic without
// creating another handler, chord, enablement predicate, or command identity.
// An empty label means "use the registered command title".
struct CommandPresentation {
    CommandPresentation() = default;
    explicit CommandPresentation(ui::CommandId command_id, std::string display_label = {},
                                  std::string display_chord = {})
        : command(command_id), label(std::move(display_label)), chord(std::move(display_chord)) {}

    ui::CommandId command = ui::kInvalidCommand;
    std::string label;
    // How THIS surface says the command is reached. Empty — the ordinary
    // case — means "ask the registry", which is right whenever a single
    // KeyChord is the whole truth.
    //
    // It is not always the whole truth. An application may reach a command
    // by something the keymap cannot hold (a multi-key sequence, say), or
    // the registry's chord may be unreachable in the context the reader is
    // actually in — a full-screen child program that consumes function keys
    // leaves an "F6" hint describing a key that will never arrive. Showing a
    // chord that does not work where the reader is standing is worse than
    // showing none, so the surface lets the application state the spelling
    // it means. Purely presentational: the keymap, and what the registry's
    // own chord still does elsewhere, are untouched.
    std::string chord;
};

}  // namespace ckv::widgets
