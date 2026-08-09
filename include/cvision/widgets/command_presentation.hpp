// Copyright (c) 2026 C. Klukas. All rights reserved.
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
    explicit CommandPresentation(ui::CommandId command_id, std::string display_label = {})
        : command(command_id), label(std::move(display_label)) {}

    ui::CommandId command = ui::kInvalidCommand;
    std::string label;
};

}  // namespace ckv::widgets
