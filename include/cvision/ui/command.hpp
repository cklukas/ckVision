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

#include <array>
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

// Where a command can be used (M9/WP-13, WP-59). A context is a named
// scope: an application pushes one on the registry, or a view names one
// for its focus ancestry (View::set_command_context). A command bound to
// contexts is available while any of them is active — an editor's Save
// belongs to the document and to the outline beside it — and a command
// bound to none is available everywhere.
//
// `outside_contexts` adds the one place a list of names cannot state:
// where no context is active at all — nothing pushed, and nothing on the
// focus path naming one, which is the bare desktop with no window focused
// or a window that declares no context of its own. An application whose
// Open belongs to the desktop and to its document windows, but not to the
// field where the reader is typing a value, says exactly that with
// {.contexts = {"document"}, .outside_contexts = true}. A scope naming no
// contexts and outside_contexts is available only where no context is
// active.
struct CommandScope {
    // The context names the command is available in (matched exactly), and whether it is also
    // available where no context is active at all. Both empty/false is the unrestricted default.
    std::vector<std::string> contexts{};
    bool outside_contexts = false;

    // Available everywhere: no context named and no restriction to the
    // context-free places.
    bool unrestricted() const noexcept { return contexts.empty() && !outside_contexts; }

    // Memberwise equality; the order of `contexts` matters.
    friend bool operator==(const CommandScope&, const CommandScope&) = default;
};

// A declared command's metadata as the registry holds it (CommandRegistry::find(), all()). It
// carries no handler or enablement predicate; those live in the registry beside it.
struct CommandInfo {
    // The id the registry assigned to `key`.
    CommandId id = kInvalidCommand;
    // The identity this command was declared under, e.g.
    // "ckv.window.close". Stable for the registry's lifetime.
    std::string key;
    // The rest is the declared CommandDescriptor's metadata: display title (may carry an '&'
    // mnemonic), category, the parsed default chord (nullopt when none was declared; the chord
    // actually bound now is CommandRegistry::chord_for_command()), and the scope and visibility,
    // which set_command_scope() and set_visibility() may since have changed.
    std::string title;
    std::string category;
    std::optional<KeyChord> default_chord;
    CommandScope scope;
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
    //
    // Every member carries an explicit initializer, empty though most are:
    // descriptors are declared with designated initializers naming only what
    // a command needs, and a member without one trips
    // -Wmissing-field-initializers at every declaration that leaves it out.
    std::string key{};
    // The display title menus and status items fall back to when they state no label of their
    // own (an '&' marks a mnemonic), a free-form grouping label the library only reports (as in
    // the standard-commands table), and where the command is available (see CommandScope;
    // unrestricted by default).
    std::string title{};
    std::string category{};
    CommandScope scope{};
    // Default chord in KeyChord::parse() spelling, e.g. "Alt+G"; empty
    // means no default chord. A caller holding an already-built
    // KeyChord (rather than a source literal) declares without one and
    // calls bind_key() instead.
    std::string chord{};
    // Whether the palette lists it, and the handler to install. An empty handler installs
    // nothing and, on a re-declaration, keeps whatever handler is already installed.
    CommandVisibility visibility = CommandVisibility::Palette;
    std::function<void()> handler{};
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
// focus_next = Tab, focus_previous = Shift+Tab (M9/WP-13, D-029),
// select_window[0..8] = Alt+1..Alt+9 (the observed classic-desktop
// convention for naming a window by its number), size_move = Ctrl+F5 (the
// observed classic-desktop Size/Move key, the Ctrl sibling of the zoom key;
// no other standard binding or decoder path gives Ctrl+F5 a meaning),
// command_palette = Ctrl+Shift+P (the convention observed across
// contemporary editors and terminals; only a terminal speaking the kitty
// keyboard protocol tells it apart from Ctrl+P, and on any other the palette
// is still reached through whatever surface lists the command), tooltip =
// Ctrl+F1 (F1 asks about the focused view; with Ctrl it asks for that view's
// short explanation rather than its help topic, and no other standard or
// example binding uses the chord).
// tile/tile_horizontally/tile_vertically/tile_grid/cascade/window_list/
// terminal_report/minimize get NO default chord — there is no comparably
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
//   - tooltip's handler is installed by widgets::TooltipController when
//     one is constructed, under the same has_handler rule, and withdrawn
//     by its destructor if it was the one that installed it.
//   - quit, close, zoom, minimize, size_move, next_window,
//     previous_window, tile, tile_horizontally, tile_vertically,
//     tile_grid, cascade, window_list, terminal_report, command_palette and
//     every select_window entry are installed by Desktop::on_attached()
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
    // Application-wide quit, then the active window's close, zoom (maximize/restore), next and
    // previous window in the Desktop's cycling order, and the classic tiling. Every field holds
    // the id the owning registry assigned; kInvalidCommand only in a default-constructed value.
    CommandId quit = kInvalidCommand;
    CommandId close = kInvalidCommand;
    CommandId zoom = kInvalidCommand;
    CommandId next_window = kInvalidCommand;
    CommandId previous_window = kInvalidCommand;
    CommandId tile = kInvalidCommand;
    // The three explicitly named tilings. The two axis words are used
    // inconsistently across desktops, so each is fixed here by the
    // arrangement it produces, not by its name — the axis names what the
    // windows are laid out ALONG: tile_horizontally lays full-HEIGHT bands
    // side by side in a row across the desktop, tile_vertically lays
    // full-WIDTH bands stacked down it, and tile_grid lays a near-square
    // grid. tile_horizontally is the arrangement `tile` has always produced;
    // `tile` keeps its own identity because applications already bind it.
    CommandId tile_horizontally = kInvalidCommand;
    CommandId tile_vertically = kInvalidCommand;
    CommandId tile_grid = kInvalidCommand;
    // Cascade the windows, open the window list, activate the menu bar, show context help, show
    // the terminal capability report, and move keyboard focus forward and back (Tab/Shift+Tab).
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
    // Activating a window by its number (Desktop::select_by_number):
    // select_window[0] names window 1 and select_window[8] window 9, in the
    // Desktop's insertion order — the numbers its window list shows.
    std::array<CommandId, 9> select_window{};
    // The keyboard move/size mode for the active window
    // (Window::enter_move_size_mode): arrows move it, Shift+arrows resize
    // it, Enter keeps the result and Esc undoes it. Also available while a
    // modal window is up, and then acts on that window: a dialog covering
    // what the reader needs to see has to be movable without a pointer.
    CommandId size_move = kInvalidCommand;
    // Opening the command palette (widgets::show_command_palette): the
    // searchable list of every command the application has declared
    // palette-visible, over the place the reader was working.
    CommandId command_palette = kInvalidCommand;
    // Showing the focused view's tooltip from the keyboard. Its
    // handler is installed by widgets::TooltipController.
    CommandId tooltip = kInvalidCommand;
};

// The keys the standard set is declared under. Spelled out so a
// frontend that drives the framework by key (a configuration file, a
// scripted test, a cross-process command bridge) can name a standard
// command without holding a registry, and so the one place that
// spells them is shared with docs/standard-commands.md's generator.
namespace std_command_keys {
// One key per StandardCommands field, named after it (kQuit is quit, kTileHorizontally is
// tile_horizontally, and so on): exactly the strings CommandRegistry's constructor declares the
// standard set under, so CommandRegistry::id_for() resolves each of them in any registry.
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
inline constexpr std::string_view kCommandPalette = "ckv.app.command_palette";
inline constexpr std::string_view kTooltip = "ckv.app.tooltip";
// StandardCommands::select_window's keys, entry for entry: kSelectWindow[0] names window 1.
inline constexpr std::array<std::string_view, 9> kSelectWindow{
    "ckv.window.select.1", "ckv.window.select.2", "ckv.window.select.3",
    "ckv.window.select.4", "ckv.window.select.5", "ckv.window.select.6",
    "ckv.window.select.7", "ckv.window.select.8", "ckv.window.select.9"};
inline constexpr std::string_view kSizeMove = "ckv.window.size_move";
}  // namespace std_command_keys

// Instance-owned (D-008). Enablement is a per-command predicate, and named
// contexts are explicit per-Application state: pushed scopes plus focused-view
// ancestry decide whether a context-bound command is available.
class CommandRegistry {
public:
    // Identifies one push_context() entry so its owner can pop exactly that entry. Assigned from
    // 1 upward and never reused within a registry.
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

    // The metadata of a declared command, or nullptr for kInvalidCommand, a withdrawn command,
    // or an id this registry never assigned. The pointer stays valid until that command is
    // re-declared or withdrawn.
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

    // The ids the constructor assigned to the standard set. Fixed for the registry's lifetime:
    // withdrawing or re-declaring a standard command keeps its id.
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

    // Replaces a declared command's scope (asserted: `id` must be declared). A later declare() of
    // the same key replaces it again with the descriptor's scope.
    void set_command_scope(CommandId id, CommandScope scope);
    // Activates the named context until the matching pop_context(); `context` must be non-empty
    // (asserted). Pushes nest, and the same name may be pushed more than once.
    ContextScopeId push_context(std::string context);
    // Pops `id` only when it is the most recent push still active. Returns false, changing
    // nothing, for any other id (an outer or already popped scope).
    bool pop_context(ContextScopeId id);
    // Whether `context` is pushed. The empty name is always active.
    bool context_active(std::string_view context) const noexcept;
    // Whether one of the contexts `id`'s scope names is active — pushed, or
    // among `focus_contexts` — ignoring enablement and outside_contexts.
    // This is what a modal scope admits a command by: a context the modal's
    // own controls name is a deliberate invitation, while being usable
    // "where no context is active" says nothing about the modal at all.
    bool in_named_context(CommandId id, const std::vector<std::string>& focus_contexts) const;

    // Changes whether a declared command is browsable — see
    // CommandVisibility. The one common use is opting a standard
    // command into an application's command palette.
    void set_visibility(CommandId id, CommandVisibility visibility);

    // A declared command's enablement (asserted: `id` must be declared). The predicate is called
    // on every query — by is_enabled(), is_available() and execute(), which menus and status
    // lines ask as they draw — and must be cheap. An empty predicate means always enabled;
    // withdraw() removes it.
    void set_enabled_predicate(CommandId id, std::function<bool()> predicate);
    // The predicate's answer, or true when `id` has none (including undeclared ids).
    bool is_enabled(CommandId id) const; // true if no predicate registered

    // Makes a declared command a toggle (asserted: `id` must be declared):
    // `predicate` answers whether it is on right now -- word wrap, a panel
    // shown. The state belongs to the command rather than to any surface, so
    // a menu row draws its check mark and a tool-bar button its checked face
    // from this one answer and neither can disagree with the other. Called
    // whenever such a surface draws, and must be cheap. An empty predicate
    // makes the command an ordinary one again; withdraw() removes it.
    void set_checked_predicate(CommandId id, std::function<bool()> predicate);
    // The toggle's state now, or std::nullopt for a command that is not a
    // toggle (no checked predicate; also undeclared ids).
    std::optional<bool> checked(CommandId id) const;
    // In scope: its scope is unrestricted, one of the contexts it names is
    // active (pushed, or among `focus_contexts`), or it is usable outside
    // contexts and none is active at all. Enablement is not consulted: this
    // is whether the command applies there, which a surface that shows a
    // disabled command rather than hiding it needs to know on its own.
    bool in_scope(CommandId id, const std::vector<std::string>& focus_contexts = {}) const;
    // Enabled, and in scope.
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
    // The chord bound to `id` right now (chord_for_command) in that display
    // spelling, or empty when nothing is bound. This is the text a menu hint,
    // a status item or an application's own help sentence shows for the
    // command, so a runtime rebind reaches every one of them alike.
    std::string chord_text(CommandId id) const;

    // Counts every change to what a surface shows for a command: a
    // declaration, re-declaration or withdrawal, a chord bound or unbound,
    // and a new chord formatter. Application compares it once per frame and
    // repaints when it has moved, which is how a runtime rebind reaches the
    // menus and status lines already on screen; anything else that keeps
    // chord text of its own compares it the same way. The value itself
    // means nothing beyond "different from before".
    std::uint64_t revision() const noexcept { return revision_; }

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
    // Invokes id's handler if one is registered AND is_available(id,
    // focus_contexts) — enabled and in scope. Returns true if the handler ran.
    bool execute(CommandId id, const std::vector<std::string>& focus_contexts = {});

private:
    // Assigns `key` its permanent id, or returns the one it already
    // has — the whole of the "nobody picks a number" rule, in one
    // place.
    CommandId id_for_key(std::string_view key);

    std::unordered_map<std::string, CommandId> ids_;
    std::unordered_map<CommandId, CommandInfo> commands_;
    std::unordered_map<CommandId, std::function<bool()>> enabled_predicates_;
    std::unordered_map<CommandId, std::function<bool()>> checked_predicates_;
    std::unordered_map<CommandId, std::function<void()>> handlers_;
    std::vector<std::pair<KeyChord, CommandId>> keymap_;
    std::vector<std::pair<ContextScopeId, std::string>> active_contexts_;
    std::function<std::string(const KeyChord&)> chord_formatter_;
    StandardCommands standard_;
    std::uint64_t revision_ = 0;
    ContextScopeId next_context_scope_id_ = 1;
    std::int32_t next_command_id_ = 1;
};

}  // namespace ckv::ui
