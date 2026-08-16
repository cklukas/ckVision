// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// A first-class command system (ARCHITECTURE.md §5, DECISIONS.md
// D-013): 32-bit CommandId, ids below 4096 reserved for the library's
// own standard commands, applications and extension libraries own the
// space above. The reserved set itself (ckv::ui::std_commands, below)
// is documented in full — the set, its default chords, and the
// reasoning behind each — in docs/standard-commands.md.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cvision/core/key.hpp"

namespace ckv::ui {

using CommandId = std::int32_t;
inline constexpr CommandId kFirstApplicationCommandId = 4096;
inline constexpr CommandId kInvalidCommand = 0;

// The library's own standard command set (D-013 materialized, M9/
// WP-12): reserved ids below kFirstApplicationCommandId, registered
// by Application's constructor (D-029) with the default chords noted
// below. An application attaches its own handler — via
// CommandRegistry::set_handler() or Application::set_command_handler()
// — to whichever of these its widgets actually need, rather than
// defining an app-specific command for a concept the framework
// already models (e.g. referencing std_commands::kQuit instead of
// registering a private "quit" command of its own).
//
// Default-chord scheme (this project's own choice, authored for
// WP-12 — no prior source consulted per this repo's provenance rule):
// kHelp = F1, kMenu = F10, kNextWindow = F6, kPreviousWindow =
// Shift+F6, kZoom = F5, kClose = Alt+F3, kQuit = Alt+X (already the
// convention every example independently used before this landed),
// kFocusNext = Tab, kFocusPrevious = Shift+Tab (M9/WP-13, D-029).
// kTile/kCascade/kWindowList get NO default chord — there is no
// comparably strong, widely-recognized single-key convention for
// them; an application binds one itself if it wants one.
//
// Titles are fixed, not overridable — the reserved range can only be
// (re-)populated by register_standard_commands() itself, so
// register_command()/add() reject any attempt to touch these ids (see
// their own doc comments). An application that needs different
// wording for the same concept — e.g. matching an external reference
// example's own vocabulary, as examples/hello does for "Exit" — keeps
// its own command in the application range instead of referencing the
// standard one; that is a legitimate reason NOT to reference
// std_commands, not a gap to work around.
//
// Default HANDLERS (not just metadata/chords) are installed for the
// commands where the framework can supply one without any
// application-specific knowledge (M9/WP-13, D-029):
//   - kFocusNext/kFocusPrevious call Application::focus_next()/
//     previous() directly — always installed, since Application
//     itself outlives everything that could invoke them.
//   - kHelp's default handler is installed by Application's own
//     constructor (see application.cpp) — D-027.
//   - kMenu's default handler (calling MenuBar::activate()) is
//     installed by MenuBar::on_attached() the moment one attaches,
//     but ONLY if nothing has claimed kMenu yet
//     (CommandRegistry::has_handler) — an application that wants
//     different F10 behavior and calls set_handler(kMenu, ...) before
//     attaching its MenuBar is never overridden. MenuBar's destructor
//     clears the handler again if it was the one that installed it,
//     so a destroyed MenuBar can never be called through a stale
//     handler.
// Every other standard command (kQuit, kClose, kZoom, kNextWindow,
// kPreviousWindow, kTile, kCascade, kWindowList) needs an
// application- or Desktop-owned window to act on, which the
// CommandRegistry has no way to reach — an application attaches its
// own handler for whichever of these it needs, same as before this
// mechanism existed.
namespace std_commands {
inline constexpr CommandId kQuit = 1;
inline constexpr CommandId kClose = 2;
inline constexpr CommandId kZoom = 3;
inline constexpr CommandId kNextWindow = 4;
inline constexpr CommandId kPreviousWindow = 5;
inline constexpr CommandId kTile = 6;
inline constexpr CommandId kCascade = 7;
inline constexpr CommandId kWindowList = 8;
inline constexpr CommandId kMenu = 9;
inline constexpr CommandId kHelp = 10;
inline constexpr CommandId kFocusNext = 11;
inline constexpr CommandId kFocusPrevious = 12;

// One reserved id per top-level menu, claimed by MenuBar for its
// Alt+<mnemonic> accelerators. They are ordinary commands so the
// accelerator works from anywhere the command keymap reaches, rather than
// only while the bar already holds focus — which is the whole point of a
// menu accelerator.
inline constexpr CommandId kFirstMenuAccelerator = 3840;
inline constexpr CommandId kMenuAcceleratorLimit = 4096;
}  // namespace std_commands

struct CommandInfo {
    CommandId id = kInvalidCommand;
    std::string title;
    std::string category;
    std::optional<KeyChord> default_chord;
    std::string context;
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
    std::string context;
    // Canonical spelling (KeyChord::parse), e.g. "Alt+G"; empty means
    // no default chord.
    std::string chord;
    std::function<void()> handler;
};

// Instance-owned (D-008). Enablement is a per-command predicate, and named
// contexts are explicit per-Application state: pushed scopes plus focused-view
// ancestry decide whether a context-bound command is available.
class CommandRegistry {
public:
    using ContextScopeId = std::uint64_t;

    // CKV_ASSERT if id falls inside the reserved std_commands range
    // (M9/WP-12) — that range is populated exactly once, by
    // Application's constructor calling register_standard_commands()
    // below; application code always registers at
    // kFirstApplicationCommandId or above.
    void register_command(CommandId id, std::string title, std::string category = {},
                           std::optional<KeyChord> default_chord = {},
                           std::string context = {});

    // Equivalent to register_command(descriptor.id, descriptor.title,
    // descriptor.category, parsed-chord) followed by
    // set_handler(descriptor.id, descriptor.handler) — see
    // CommandDescriptor's own doc comment for what this does NOT cover
    // yet. CKV_ASSERT if descriptor.chord is non-empty but fails to
    // parse — a malformed chord literal in source is a programmer
    // error to fix, not a condition to silently degrade from.
    void add(CommandDescriptor descriptor);

    // Populates every std_commands::* id with its metadata and
    // default chord (M9/WP-12, D-013/D-029) — called exactly once, by
    // Application's constructor. Not for application code to call:
    // there is nothing to configure, and calling it twice would just
    // re-bind the same chords to the same ids.
    void register_standard_commands();

    const CommandInfo* find(CommandId id) const noexcept;

    // Every registered command, sorted by id (M9/WP-12 — a
    // deterministic order for anything that enumerates the registry,
    // e.g. the reference-docs command table generator; commands_
    // itself is an unordered_map and gives no ordering guarantee).
    std::vector<CommandInfo> all() const;

    // Retracts a command definition and every behavior path owned by that
    // command id: metadata, enablement predicate, handler, and key bindings.
    // Existing menu/status entries that still reference the id become inert
    // until the application registers it again.
    void retract_command(CommandId id);

    void set_command_context(CommandId id, std::string context);
    ContextScopeId push_context(std::string context);
    bool pop_context(ContextScopeId id);
    bool context_active(std::string_view context) const noexcept;

    void set_enabled_predicate(CommandId id, std::function<bool()> predicate);
    bool is_enabled(CommandId id) const;  // true if no predicate registered
    bool is_available(CommandId id, const std::vector<std::string>& focus_contexts = {}) const;

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

    // How a chord is spelled wherever the framework renders one for the
    // reader: menu chord hints, status-line items, the command palette.
    // The default is ckv::format(). An application whose product has its
    // own established key-label convention — "Alt-X" rather than
    // "Alt+X", or a platform's glyph spelling — installs it once here
    // instead of restating it at every surface that shows a chord, which
    // is the only way those surfaces can agree. Chord *parsing* is
    // unaffected: this is display spelling, not a syntax.
    void set_chord_formatter(std::function<std::string(const KeyChord&)> formatter);
    std::string format_chord(const KeyChord& chord) const;

    // Handler dispatch (M9/WP-10 — moved here from Application, whose
    // set_command_handler/execute_command now just forward, so add()'s
    // .handler field has somewhere to land without giving
    // CommandRegistry an Application dependency it doesn't otherwise
    // need). `id` need not be pre-registered — a handler may be
    // attached before or after register_command()/add().
    void set_handler(CommandId id, std::function<void()> handler);
    // Whether `id` currently has a real (non-empty) handler installed
    // (M9/WP-13) — the guard a default-handler installer (MenuBar::
    // on_attached(), see std_commands' own doc comment) checks before
    // installing itself, so it never clobbers a handler an application
    // deliberately set first.
    bool has_handler(CommandId id) const;
    // Invokes id's handler if one is registered AND is_enabled(id).
    // Returns true if the handler ran.
    bool execute(CommandId id, const std::vector<std::string>& focus_contexts = {});

private:
    // The shared core of register_command()/register_standard_commands()
    // — no reserved-range check, since register_standard_commands()'s
    // whole job is populating that range.
    void register_command_internal(CommandId id, std::string title, std::string category,
                                    std::optional<KeyChord> default_chord,
                                    std::string context);

    std::unordered_map<CommandId, CommandInfo> commands_;
    std::unordered_map<CommandId, std::function<bool()>> enabled_predicates_;
    std::unordered_map<CommandId, std::function<void()>> handlers_;
    std::vector<std::pair<KeyChord, CommandId>> keymap_;
    std::vector<std::pair<ContextScopeId, std::string>> active_contexts_;
    std::function<std::string(const KeyChord&)> chord_formatter_;
    ContextScopeId next_context_scope_id_ = 1;
};

}  // namespace ckv::ui
