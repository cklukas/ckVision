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

// One-call registration (M9/WP-10): the designated-initializer form of
// register_command() + set_handler(), collapsing what used to be two
// separate calls (and, for a chord, a third — hand-building a KeyChord)
// into one descriptor passed to CommandRegistry::add().
//
// Scope note: this covers the metadata+handler half of WP-10's own
// illustrative descriptor (ARCHITECTURE.md §5 Menus) — the `.menu`/
// `.status` fields that auto-populate a MenuBar/StatusLine entry are
// deliberately NOT here yet. CommandRegistry lives in ui::, below
// widgets:: in this project's dependency direction (AGENTS.md); it
// cannot reference MenuBar/StatusLine without inverting that, and
// building menu/status structure from command metadata also needs
// registration-ORDER tracking this registry doesn't keep (commands_
// is an unordered_map). That's real, separate design work for a
// widgets::-layer mechanism that reads CommandRegistry, not something
// to bolt on here as an ignored field a caller could set and have
// silently do nothing.
struct CommandDescriptor {
    CommandId id = kInvalidCommand;
    std::string title;
    std::string category;
    // Canonical spelling (KeyChord::parse), e.g. "Alt+G"; empty means
    // no default chord.
    std::string chord;
    std::function<void()> handler;
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

    // Equivalent to register_command(descriptor.id, descriptor.title,
    // descriptor.category, parsed-chord) followed by
    // set_handler(descriptor.id, descriptor.handler) — see
    // CommandDescriptor's own doc comment for what this does NOT cover
    // yet. CKV_ASSERT if descriptor.chord is non-empty but fails to
    // parse — a malformed chord literal in source is a programmer
    // error to fix, not a condition to silently degrade from.
    void add(CommandDescriptor descriptor);

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

    // Handler dispatch (M9/WP-10 — moved here from Application, whose
    // set_command_handler/execute_command now just forward, so add()'s
    // .handler field has somewhere to land without giving
    // CommandRegistry an Application dependency it doesn't otherwise
    // need). `id` need not be pre-registered — a handler may be
    // attached before or after register_command()/add().
    void set_handler(CommandId id, std::function<void()> handler);
    // Invokes id's handler if one is registered AND is_enabled(id).
    // Returns true if the handler ran.
    bool execute(CommandId id);

private:
    std::unordered_map<CommandId, CommandInfo> commands_;
    std::unordered_map<CommandId, std::function<bool()>> enabled_predicates_;
    std::unordered_map<CommandId, std::function<void()>> handlers_;
    std::vector<std::pair<KeyChord, CommandId>> keymap_;
};

}  // namespace ckv::ui
