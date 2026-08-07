// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// A first-class command system (ARCHITECTURE.md §5, DECISIONS.md
// D-013): 32-bit CommandId, ids below 4096 reserved for the library's
// own standard commands, applications and extension libraries own the
// space above.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cvision/core/key.hpp"

namespace ckv::ui {

using CommandId = std::int32_t;
inline constexpr CommandId kFirstApplicationCommandId = 4096;
inline constexpr CommandId kInvalidCommand = 0;

struct CommandInfo {
    CommandId id = kInvalidCommand;
    std::string title;
    std::string category;
    std::optional<KeyChord> default_chord;
};

// Instance-owned (D-008). Enablement is a per-command predicate
// (checked on demand, e.g. before dispatch or when rendering a menu
// item in M5) rather than a full named-context stack — a context stack
// is a natural future extension of the same predicate hook once real
// menus exist to observe it; M4 has no menu system yet to need one.
class CommandRegistry {
public:
    void register_command(CommandId id, std::string title, std::string category = {},
                           std::optional<KeyChord> default_chord = {});

    const CommandInfo* find(CommandId id) const noexcept;

    void set_enabled_predicate(CommandId id, std::function<bool()> predicate);
    bool is_enabled(CommandId id) const;  // true if no predicate registered

    // The active keymap: chord -> command. Rebindable at runtime;
    // binding the same chord again replaces the previous command.
    void bind_key(KeyChord chord, CommandId id);
    void unbind_key(const KeyChord& chord);
    std::optional<CommandId> command_for_key(const KeyChord& chord) const;

    // The reverse of command_for_key: the first chord currently bound
    // to `id` (M9/WP-11 — what a menu/status item renders as its chord
    // hint). A command may have more than one chord bound; this is
    // display's "one representative chord" pick, not an enumeration.
    // nullopt if nothing is bound to `id` right now, regardless of
    // whatever default_chord it was registered with (a caller that
    // unbinds a command's only chord sees that reflected here).
    std::optional<KeyChord> chord_for_command(CommandId id) const;

private:
    std::unordered_map<CommandId, CommandInfo> commands_;
    std::unordered_map<CommandId, std::function<bool()>> enabled_predicates_;
    std::vector<std::pair<KeyChord, CommandId>> keymap_;
};

}  // namespace ckv::ui
