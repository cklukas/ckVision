// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// A first-class command system (the architecture §5, the decision log
// D-013): a command's identity is a namespaced string key it declares
// itself under, and the registry assigns the CommandId that stands for
// that key at runtime. Nobody picks a number, so no two parties can
// pick the same one — the library, a widget, an extension library and
// the application all declare into one flat space and cannot collide.
// The library's own standard set (CommandRegistry::standard(), below)
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

// A handle to a declared command, assigned by CommandRegistry::
// declare() from a per-registry monotonic counter. It is a scoped
// enumeration on purpose: `base + index` — the pattern that made every
// party invent its own numeric range and then collide with the next
// one — does not compile, so the class of bug is unrepresentable
// rather than merely discouraged. A CommandId is never written as a
// literal, never persisted, and never compared for magnitude; it is
// meaningful only to the registry that assigned it. Everything durable
// — configuration, menu definitions, cross-process references — keys
// on the string instead (CommandRegistry::key_for/id_for).
enum class CommandId : std::int32_t {};

// "No command": a menu separator, a hand-callback item, a chord bound
// to nothing. Never assigned by declare().
inline constexpr CommandId kInvalidCommand{0};

// Whether a command is one the reader is meant to discover by browsing
// for it — what widgets::CommandPalette lists. This is metadata a
// command states about itself, not a property derived from its id:
// "should the reader see this?" is a question only the declaring party
// can answer.
//
// Hidden covers the two kinds that a browsable list would only clutter:
// framework plumbing an application surfaces (or does not) through its
// own menus — see StandardCommands — and commands that exist purely to
// carry a chord, such as MenuBar's Alt+<mnemonic> menu accelerators,
// which duplicate a menu the reader can already see.
enum class CommandVisibility {
    Palette,
    Hidden,
};

struct CommandInfo {
    CommandId id = kInvalidCommand;
    // The identity this command was declared under, e.g.
    // "ckv.window.close". Stable for the registry's lifetime.
    std::string key;
    std::string title;
    std::string category;
    std::optional<KeyChord> default_chord;
    std::string context;
    CommandVisibility visibility = CommandVisibility::Palette;
};

// One-call declaration (M9/WP-10): metadata, default chord and handler
// in a single descriptor, rather than a register + bind + set_handler
// sequence each caller had to remember to complete.
//
// Scope note: this covers the metadata+handler half of WP-10's own
// illustrative descriptor (the architecture §5 Menus) — the `.menu`/
// `.status` fields that auto-populate a MenuBar/StatusLine entry are
// deliberately NOT here yet. CommandRegistry lives in ui::, below
// widgets:: in this project's dependency direction (the engineering standard); it
// cannot reference MenuBar/StatusLine without inverting that, and
// building menu/status structure from command metadata also needs
// registration-ORDER tracking. That's real, separate design work for a
// widgets::-layer mechanism that reads CommandRegistry, not something
// to bolt on here as an ignored field a caller could set and have
// silently do nothing.
struct CommandDescriptor {
    // The command's identity: a namespaced key whose prefix belongs to
    // the declaring party — "ckv." for this library, an application or
    // extension library using its own. Required; declaring an empty
    // key is a programmer error (CKV_ASSERT). Within one party a
    // repeated key is, by definition, the same command.
    //
    // Owned rather than a view, like every other field here: a
    // descriptor may be built in one place and declared in another, and
    // a computed key must not have to outlive that.
    std::string key;
    std::string title;
    std::string category;
    std::string context;
    // Default chord in KeyChord::parse() spelling, e.g. "Alt+G"; empty
    // means no default chord. A caller holding an already-built
    // KeyChord (rather than a source literal) declares without one and
    // calls bind_key() instead.
    std::string chord;
    CommandVisibility visibility = CommandVisibility::Palette;
    std::function<void()> handler;
};

// The library's own standard commands (D-013 materialized, M9/WP-12),
// declared by every CommandRegistry's constructor with the default
// chords noted below and reachable as registry.standard().quit and so
// on. An application attaches its own handler — via
// CommandRegistry::set_handler() or Application::set_command_handler()
// — to whichever of these its widgets actually need, rather than
// declaring an app-specific command for a concept the framework
// already models (e.g. referencing standard().quit instead of
// declaring a private "quit" command of its own).
//
// Default-chord scheme (this project's own choice, authored for
// WP-12 — no prior source consulted per this repo's provenance rule):
// help = F1, menu = F10, next_window = F6, previous_window =
// Shift+F6, zoom = F5, close = Alt+F3, quit = Alt+X (already the
// convention every example independently used before this landed),
// focus_next = Tab, focus_previous = Shift+Tab (M9/WP-13, D-029).
// tile/tile_horizontally/tile_vertically/tile_grid/cascade/window_list/
// terminal_report get NO default chord — there is no comparably
// strong, widely-recognized single-key convention for them; an
// application binds one itself if it wants one.
//
// The standard set is Hidden: these are the framework's own plumbing,
// which an application surfaces where it wants them through its own
// menu and status entries. An application that does want one of them
// browsable calls set_visibility(standard().quit, Palette).
//
// Titles are not fixed — re-declaring a standard key replaces its
// metadata like any other (an application matching an external
// reference example's vocabulary, as examples/hello does for "Exit",
// can either do that or keep a command of its own; declaring its own
// remains the right move when the concept itself differs, and
// widgets::CommandPresentation covers per-surface wording without
// touching the command at all).
//
// Default HANDLERS (not just metadata/chords) are installed for the
// commands where the framework can supply one without any
// application-specific knowledge (M9/WP-13, D-029):
//   - focus_next/focus_previous call Application::focus_next()/
//     previous() directly — always installed, since Application
//     itself outlives everything that could invoke them.
//   - help's default handler is installed by Application's own
//     constructor (see application.cpp) — D-027.
//   - menu's default handler (calling MenuBar::activate()) is
//     installed by MenuBar::on_attached() the moment one attaches,
//     but ONLY if nothing has claimed it yet
//     (CommandRegistry::has_handler) — an application that wants
//     different F10 behavior and calls set_handler(standard().menu,
//     ...) before attaching its MenuBar is never overridden. MenuBar's
//     destructor clears the handler again if it was the one that
//     installed it, so a destroyed MenuBar can never be called through
//     a stale handler.
//   - quit, close, zoom, next_window, previous_window, tile,
//     tile_horizontally, tile_vertically, tile_grid, cascade,
//     window_list and terminal_report are installed by Desktop::on_attached()
//     under that same has_handler rule — a Desktop is exactly the
//     thing that owns the windows they act on, and knows the one
//     cycling order they all share. An application that claims one
//     before its Desktop attaches keeps it.
// Note that a command with NO handler is still "available":
// is_available() consults the enablement predicate and the focus
// context, neither of which can know whether anyone is listening. A
// menu item bound to an unhandled command therefore draws live,
// accepts its click, and does nothing at all. That is a defect in the
// application rather than a state to design for — bind only commands
// something handles, or give the command an enablement predicate
// returning false, so every surface greys it and says so.
struct StandardCommands {
    CommandId quit = kInvalidCommand;
    CommandId close = kInvalidCommand;
    CommandId zoom = kInvalidCommand;
    CommandId next_window = kInvalidCommand;
    CommandId previous_window = kInvalidCommand;
    CommandId tile = kInvalidCommand;
    // The three explicitly named tilings. The two axis words are used
    // inconsistently across desktops, so each is fixed here by the
    // arrangement it produces, not by its name: tile_horizontally lays
    // full-WIDTH bands stacked top to bottom, tile_vertically lays
    // full-HEIGHT bands side by side, and tile_grid lays a near-square
    // grid. tile_vertically is the arrangement `tile` has always produced;
    // `tile` keeps its own identity because applications already bind it.
    CommandId tile_horizontally = kInvalidCommand;
    CommandId tile_vertically = kInvalidCommand;
    CommandId tile_grid = kInvalidCommand;
    CommandId cascade = kInvalidCommand;
    CommandId window_list = kInvalidCommand;
    CommandId menu = kInvalidCommand;
    CommandId help = kInvalidCommand;
    CommandId terminal_report = kInvalidCommand;
    CommandId focus_next = kInvalidCommand;
    CommandId focus_previous = kInvalidCommand;
    // Putting the active window away (U4-i). A window's own `_` control
    // does this for the window it is drawn on; this is the same verb
    // reached from a menu or a key, which is the route a reader has when
    // the window they mean is the one they are working in.
    CommandId minimize = kInvalidCommand;
};

// The keys the standard set is declared under. Spelled out so a
// frontend that drives the framework by key (a configuration file, a
// scripted test, a cross-process command bridge) can name a standard
// command without holding a registry, and so the one place that
// spells them is shared with docs/standard-commands.md's generator.
namespace std_command_keys {
inline constexpr std::string_view kQuit = "ckv.app.quit";
inline constexpr std::string_view kHelp = "ckv.app.help";
inline constexpr std::string_view kTerminalReport = "ckv.app.terminal_report";
inline constexpr std::string_view kMenu = "ckv.app.menu";
inline constexpr std::string_view kClose = "ckv.window.close";
inline constexpr std::string_view kZoom = "ckv.window.zoom";
inline constexpr std::string_view kNextWindow = "ckv.window.next";
inline constexpr std::string_view kPreviousWindow = "ckv.window.previous";
inline constexpr std::string_view kTile = "ckv.window.tile";
inline constexpr std::string_view kTileHorizontally = "ckv.window.tile_horizontal";
inline constexpr std::string_view kTileVertically = "ckv.window.tile_vertical";
inline constexpr std::string_view kTileGrid = "ckv.window.tile_grid";
inline constexpr std::string_view kCascade = "ckv.window.cascade";
inline constexpr std::string_view kWindowList = "ckv.window.list";
inline constexpr std::string_view kFocusNext = "ckv.focus.next";
inline constexpr std::string_view kFocusPrevious = "ckv.focus.previous";
inline constexpr std::string_view kMinimize = "ckv.window.minimize";
}  // namespace std_command_keys

// Instance-owned (D-008). Enablement is a per-command predicate, and named
// contexts are explicit per-Application state: pushed scopes plus focused-view
// ancestry decide whether a context-bound command is available.
class CommandRegistry {
public:
    using ContextScopeId = std::uint64_t;

    // Declares the standard set (see StandardCommands) so standard()
    // is answerable from the moment a registry exists, and every
    // registry — including one a test builds directly — has the same
    // framework floor under it.
    CommandRegistry();

    // Defines `descriptor.key`'s command and returns the id assigned to
    // that key, binding its default chord if it declared one.
    //
    // Idempotent per key: declaring a key that already has an id
    // returns that same id and re-defines it in place, so a surface
    // that rebuilds its commands (MenuBar's accelerators, an
    // application reloading a command table) keeps every id its menu
    // and status entries already reference. Re-declaring replaces
    // title, category, context, visibility and default chord — and
    // drops the previously declared chord's binding, unless something
    // has since rebound that chord elsewhere — but an EMPTY
    // descriptor.handler leaves any installed handler alone: handlers
    // are routinely attached separately (set_handler, the framework's
    // own default-handler installers), and a re-declaration of the
    // metadata must not silently unhook behavior it says nothing
    // about.
    CommandId declare(CommandDescriptor descriptor);

    const CommandInfo* find(CommandId id) const noexcept;

    // The id assigned to `key`, or nullopt if no command is currently
    // declared under it. This is how anything that names commands as
    // strings — configuration, a menu definition file, a bridge from
    // another command model — resolves them once at startup.
    std::optional<CommandId> id_for(std::string_view key) const;
    // The key `id` was declared under; empty for kInvalidCommand, an
    // id from another registry, or one that has been withdrawn. The
    // view is valid until that command is re-declared or withdrawn.
    std::string_view key_for(CommandId id) const noexcept;

    // Every declared command, in declaration order (M9/WP-12 — a
    // deterministic order for anything that enumerates the registry,
    // e.g. the reference-docs command table generator; commands_
    // itself is an unordered_map and gives no ordering guarantee).
    std::vector<CommandInfo> all() const;

    const StandardCommands& standard() const noexcept { return standard_; }

    // Retracts a command definition and every behavior path owned by that
    // command id: metadata, enablement predicate, handler, and key bindings.
    // Existing menu/status entries that still reference the id become inert
    // until the command is declared again.
    //
    // The key keeps its id reserved: declaring it again returns the
    // same id rather than a fresh one, and no other key can ever be
    // given that id. A surface that withdraws and re-declares as it
    // rebuilds — MenuBar's menu accelerators — therefore cannot hand
    // out an id that later means something else, which is exactly the
    // failure a recycled handle invites.
    void withdraw(CommandId id);

    void set_command_context(CommandId id, std::string context);
    ContextScopeId push_context(std::string context);
    bool pop_context(ContextScopeId id);
    bool context_active(std::string_view context) const noexcept;

    // Changes whether a declared command is browsable — see
    // CommandVisibility. The one common use is opting a standard
    // command into an application's command palette.
    void set_visibility(CommandId id, CommandVisibility visibility);

    void set_enabled_predicate(CommandId id, std::function<bool()> predicate);
    bool is_enabled(CommandId id) const; // true if no predicate registered
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
    // whatever default chord it was declared with (a caller that
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
    // set_command_handler/execute_command now just forward, so
    // declare()'s .handler field has somewhere to land without giving
    // CommandRegistry an Application dependency it doesn't otherwise
    // need). `id` need not be declared — a handler may be attached
    // before or after declare().
    void set_handler(CommandId id, std::function<void()> handler);
    // Whether `id` currently has a real (non-empty) handler installed
    // (M9/WP-13) — the guard a default-handler installer (MenuBar::
    // on_attached(), see StandardCommands' own doc comment) checks
    // before installing itself, so it never clobbers a handler an
    // application deliberately set first.
    bool has_handler(CommandId id) const;
    // Invokes id's handler if one is registered AND is_enabled(id).
    // Returns true if the handler ran.
    bool execute(CommandId id, const std::vector<std::string>& focus_contexts = {});

private:
    // Assigns `key` its permanent id, or returns the one it already
    // has — the whole of the "nobody picks a number" rule, in one
    // place.
    CommandId id_for_key(std::string_view key);

    std::unordered_map<std::string, CommandId> ids_;
    std::unordered_map<CommandId, CommandInfo> commands_;
    std::unordered_map<CommandId, std::function<bool()>> enabled_predicates_;
    std::unordered_map<CommandId, std::function<void()>> handlers_;
    std::vector<std::pair<KeyChord, CommandId>> keymap_;
    std::vector<std::pair<ContextScopeId, std::string>> active_contexts_;
    std::function<std::string(const KeyChord&)> chord_formatter_;
    StandardCommands standard_;
    ContextScopeId next_context_scope_id_ = 1;
    std::int32_t next_command_id_ = 1;
};

}  // namespace ckv::ui
