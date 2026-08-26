// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// MenuBar + DropdownMenu (the architecture §5 "Menus" / the internal plans
// widgets.md M5 baseline): F10-equivalent activation (activate(),
// installed as the standard menu command's default handler when
// unclaimed — M9/WP-13, D-029 — routing through the command keymap
// like every other accelerator in this framework), mnemonic letters,
// Left/Right/Up/Down navigation with wrapping, Home/End to the ends,
// Enter/mnemonic to activate, unavailable items reachable but inert,
// checkable items, nested submenus the keyboard enters and leaves the way the
// pointer does — Right or Enter on an item that has one opens it and the keys
// go to it, Left or Esc steps back out to that item, and Right on an item
// without one carries the walk on to the next top-level menu —
// Esc closes one level, light-dismiss
// on an outside click via Application's mouse input capture, and
// right-aligned chord hints rendered live from the command registry
// (M9/WP-11).
//
// show_context_menu() below reuses DropdownMenu directly as a
// positional pop-up ("Context menu: positional pop-up with same
// feature set" — the widget catalog M5 baseline) at a caller-chosen
// screen position, e.g. from a right-click handler. show_context_menu_for_focus()
// supplies the keyboard path: applications that own the current context menu
// model call it from their Shift+F10/Menu-command handler, and the menu opens
// at the focused view's cell location without any global menu registry.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/command_presentation.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

// The mark in a menu's left column. Checked/Unchecked is an independent
// switch — a setting that is on or off. RadioOn/RadioOff is one choice
// out of a set, and reads differently on purpose: a reader scanning a
// column of round marks knows that turning one on turns another off,
// which a column of boxes would not tell them. None leaves the column
// out entirely for menus that need no marks at all.
enum class MenuMark {
    None,
    Unchecked,
    Checked,
    RadioOff,
    RadioOn,
};

// What a menu row IS.
//
// A row is exactly one of these, and which one is decided by the named
// constructor that made it — see MenuItem. The kind is explicit because
// the alternative, inferring it from which of several optional fields
// happen to be filled in, makes states like "a separator that also has a
// submenu and a command" representable, unreviewable, and eventually
// real.
enum class MenuItemKind {
    Command,   // runs a registry command; wording and chord come from it
    Action,    // runs a callback this item carries
    Submenu,   // opens child items
    Separator, // a divider: never highlighted, navigated to, or activated
};

// One row of a menu.
//
// Built through the named constructors below rather than by filling in
// fields, so an item's kind is a fact about how it was made rather than
// a rule a reader has to reconstruct. The refinements every kind can
// carry — a mark, a help topic, a reason it is unavailable — chain onto
// that:
//
//     MenuItem::command(save_id),
//     MenuItem::separator(),
//     MenuItem::submenu("&Recent", std::move(recent)),
//     MenuItem::action("&Word wrap", [this] { toggle_wrap(); })
//         .with_mark(wrapping ? MenuMark::Checked : MenuMark::Unchecked)
//         .with_help("editor.wrap"),
//
// Enablement has one source per kind, deliberately: a Command item is
// available exactly when its command is (the registry's predicate and
// context decide, and no menu may disagree with the palette or the
// status line about it), while an Action or Submenu item carries its own
// flag because there is nothing else that could know.
class MenuItem {
public:
    // --- the four kinds ------------------------------------------------
    static MenuItem command(ui::CommandId id);
    static MenuItem command(CommandPresentation presentation);
    static MenuItem action(std::string label, std::function<void()> run);
    static MenuItem submenu(std::string label, std::vector<MenuItem> children);
    static MenuItem separator();

    // --- refinements, chainable ----------------------------------------
    [[nodiscard]] MenuItem with_mark(MenuMark mark) const;
    // The help topic F1 resolves while this row is highlighted. Menus are
    // where a reader looks for a verb they do not know yet, so this is
    // where explaining one belongs.
    [[nodiscard]] MenuItem with_help(std::string help_context) const;
    // Why this row cannot be used right now, in the application's own
    // words ("no document is open"). A surface that greys a verb without
    // saying why leaves the reader to guess; the menu reports the reason
    // and the status line says it.
    [[nodiscard]] MenuItem with_disabled_reason(std::string reason) const;
    // Action and Submenu rows only — a Command row's availability is its
    // command's, and overriding it here would let a menu lie about it.
    [[nodiscard]] MenuItem with_enabled(bool enabled) const;

    // --- what a menu asks ----------------------------------------------
    MenuItemKind kind() const noexcept { return kind_; }
    bool is_separator() const noexcept { return kind_ == MenuItemKind::Separator; }
    bool has_children() const noexcept { return kind_ == MenuItemKind::Submenu; }

    // The command this row runs, or kInvalidCommand for the other kinds.
    ui::CommandId command() const noexcept { return presentation_.command; }
    const CommandPresentation& presentation() const noexcept { return presentation_; }
    // The row's own text, for Action and Submenu rows. A Command row's
    // text comes from its presentation or its registration, never from
    // here — see item_source_text().
    const std::string& label() const noexcept { return label_; }
    const std::function<void()>& action() const noexcept { return action_; }
    const std::vector<MenuItem>& children() const noexcept { return children_; }
    MenuMark mark() const noexcept { return mark_; }
    const std::string& help_context() const noexcept { return help_context_; }
    const std::string& disabled_reason() const noexcept { return disabled_reason_; }
    // Only meaningful for Action/Submenu; a Command row asks the registry.
    bool enabled_flag() const noexcept { return enabled_; }

private:
    MenuItem() = default;

    MenuItemKind kind_ = MenuItemKind::Separator;
    std::string label_;
    CommandPresentation presentation_;
    std::function<void()> action_;
    std::vector<MenuItem> children_;
    MenuMark mark_ = MenuMark::None;
    std::string help_context_;
    std::string disabled_reason_;
    bool enabled_ = true;
};

// What a menu reports about the row under the highlight, for the
// surfaces that explain it: a status line showing the command's hint or
// the reason it is grey, and F1 resolving the row's help topic.
//
// A struct rather than a bare CommandId because "which row is the reader
// looking at" and "what can be said about it" are one question, and a
// listener that had to look the rest up again could look it up wrong.
struct MenuHighlight {
    ui::CommandId command = ui::kInvalidCommand;
    std::string help_context;
    std::string disabled_reason;
    bool enabled = true;
    // No row is highlighted (the menu closed, or the pointer left it).
    bool none = false;
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.menu.dropdown.normal"/"highlighted"/"disabled".
// Also reads context().app for command enablement — see the file
// comment on why menus are one of the few widgets that need it.
// How a dropdown came to be open, which decides whether it already has a
// selection. A menu opened from the keyboard must land on an item at once —
// there is no pointer to indicate one, and the next arrow key has to move
// from somewhere. A menu opened by a pointer press has an indicator: the
// pointer itself. It therefore opens with nothing selected and follows the
// pointer, settling on its first item only when the press ends without
// having chosen anything. Highlighting an item the reader has not pointed
// at would claim a choice they have not made.
enum class MenuOpenReason {
    Keyboard,
    PointerPress,
};

// Why a menu is going away. Choosing an item ends the whole menu
// interaction, not merely the popup: the reader asked for a command and is
// done with the menu. Cancelling leaves the menu system to decide how far
// to unwind. The distinction matters beyond appearances — whatever the
// command then does (open a dialog, say) sees the focus the menu left
// behind, so a bar that stays focused hands the command a focus target the
// reader never chose, and it comes back highlighted once the dialog closes.
enum class MenuDismissReason {
    Cancelled,
    ItemChosen,
};

class DropdownMenu : public ui::View {
public:
    explicit DropdownMenu(std::vector<MenuItem> items, DropdownMenu* parent_menu = nullptr);
    ~DropdownMenu() override;

    void set_role_override(ui::RoleId normal_role, ui::RoleId highlighted_role,
                            ui::RoleId disabled_role) noexcept {
        normal_role_ = normal_role;
        highlighted_role_ = highlighted_role;
        disabled_role_ = disabled_role;
    }
    void set_hotkey_role_override(ui::RoleId role) noexcept { hotkey_role_ = role; }

    // Fires on Esc, on a click outside the dropdown's own bounds (light
    // dismiss), after a successful item activation (carrying
    // MenuDismissReason::ItemChosen — and BEFORE the command runs, so the
    // handler can settle focus first), AND unconditionally
    // from the destructor — so an owner (MenuBar) always learns the
    // popup is going away regardless of WHO removed it (itself, or a
    // caller bypassing it via Desktop::remove_popup directly), and can
    // reliably clear its own bookkeeping instead of desyncing. May
    // therefore fire more than once for the same dismissal (e.g. once
    // from an explicit dismiss() and again from the destructor it
    // triggers); handlers must be idempotent — MenuBar::close_dropdown()
    // already is.
    std::function<void(MenuDismissReason)> on_dismiss;

    // Fires whenever the highlighted item changes, carrying that item's
    // command (kInvalidCommand for a separator, a submenu parent, or an
    // item with no command). Browsing a menu is how a reader asks what a
    // command does before committing to it, so a status line that
    // explains the highlighted entry needs to hear about the move; the
    // menu reports it rather than assuming what any particular surface
    // wants to do with it.
    //
    // A submenu inherits this handler when it opens, so one wiring hears the
    // whole chain: the highlight a chain of menus shows is the innermost
    // menu's, and it is reported both on opening a submenu and again for the
    // parent item once that submenu closes.
    std::function<void(const MenuHighlight&)> on_highlight_changed;

    const std::vector<MenuItem>& items() const noexcept { return items_; }
    int highlighted() const noexcept { return highlighted_; }
    // The command behind the highlighted item, or kInvalidCommand.
    ui::CommandId highlighted_command() const noexcept;
    // Everything about the row the reader is standing on, following any
    // open submenu chain to its innermost menu: that is where the reader
    // is, and that is the row a status line should explain and F1 should
    // answer about.
    MenuHighlight highlight() const;

    // Preferred size: width fits the longest item label plus its chord
    // hint column (+ padding), height is exactly one row per item
    // (including separators).
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool casts_shadow() const noexcept override { return true; }
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every title on the bar opens something.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_attached() override;

private:
    friend class MenuBar;
    friend DropdownMenu* show_context_menu(std::vector<MenuItem> items,
                                            Point screen_position,
                                            ui::Application& app,
                                            Desktop& desktop);
    // Single assignment point for the highlight, so every route that
    // moves it — construction, arrows, the pointer — reports the move
    // exactly once and none can forget to.
    void set_highlighted(int index);
    // Source text for an item's mnemonic/label parsing (M9/WP-11): the
    // registered command's title when `item.command` is set — so a
    // menu item referencing a command stops carrying its own label
    // text, matching the command's registration exactly rather than
    // risking the two drifting apart — falling back to `item.label`
    // for hand-callback items with no command.
    std::string item_source_text(const MenuItem& item) const;
    // The right-aligned chord hint text ("Alt+G"), or nullopt if the
    // item has no command or that command has no chord bound right
    // now (CommandRegistry::chord_for_command, live — a runtime rebind
    // changes what renders here without touching the item itself).
    std::optional<std::string> item_chord_hint(const MenuItem& item) const;
    bool item_enabled(std::size_t index) const;
    void set_invocation_contexts(std::vector<std::string> contexts) {
        invocation_contexts_ = std::move(contexts);
        has_invocation_contexts_ = true;
    }
    // Whether the highlight may rest here at all — separators alone
    // cannot be stood on. A row that is merely unavailable can be.
    bool item_reachable(std::size_t index) const;
    // Brings the open submenu into agreement with the highlighted row,
    // however the highlight got there. Idempotent on purpose.
    void follow_highlight_with_submenu();
    int step_selection(int from, int direction) const;  // -1 if no selectable item exists at all
    void activate(int index);
    void dismiss(MenuDismissReason reason = MenuDismissReason::Cancelled);
    void open_submenu(int index);
    void close_submenu(bool restore_capture);
    // The deepest menu currently open below this one, or this one when no
    // submenu is up. The keyboard belongs to it: a chain of menus shows one
    // highlight, and it is the innermost menu's. MenuBar routes with this
    // because focus never leaves the bar — see MenuBar::on_key.
    DropdownMenu* innermost_menu() noexcept;
    // The menu this chain hangs from: the one a MenuBar opened, or a context
    // menu itself. It holds what belongs to the chain rather than to any one
    // popup in it, and it is where a pointer event over none of them goes.
    DropdownMenu* root_menu() noexcept;
    // Which menu of this chain the pointer is over, innermost first — a
    // submenu overlaps its parent's border, and the submenu wins there — or
    // nullptr when it is over none of them.
    DropdownMenu* menu_under_pointer(Point cell) noexcept;
    // One press, one chain. The button goes down on whichever menu is under
    // the pointer and may come back up over a different one — most often a
    // submenu that opened under the pointer in between — so whether a press
    // is outstanding is the chain's state, not any one popup's.
    bool& chain_pointer_pressed() noexcept;
    void dismiss_chain(MenuDismissReason reason = MenuDismissReason::Cancelled);
    void begin_pointer_press() noexcept { pointer_pressed_ = true; }
    // The press that opened this menu has ended. If it ended without the
    // pointer ever settling on an item, the menu now takes a selection so
    // the keyboard can carry on from a definite place.
    void end_pointer_press();
    // Set before attaching. PointerPress defers the initial selection; see
    // MenuOpenReason.
    void set_open_reason(MenuOpenReason reason) noexcept { open_reason_ = reason; }
    void set_pointer_navigation(std::function<bool(const MouseEvent&)> navigation) {
        pointer_navigation_ = std::move(navigation);
    }
    bool has_check_column() const noexcept;
    ui::CommandId item_command(const MenuItem& item) const noexcept;
    std::string item_presentation_label(const MenuItem& item) const;

    std::vector<MenuItem> items_;
    int highlighted_ = -1;
    // Cached at attach for the things a menu does TO an application —
    // capturing input, opening a popup. Never for asking what a command's
    // state is: this pointer outlives the application it names (nothing
    // clears it on detach), while context().app follows attachment. A menu
    // is interrogated during teardown too, and the difference between the
    // two is a use-after-free.
    ui::Application* app_ = nullptr;
    Desktop* desktop_ = nullptr;
    DropdownMenu* parent_menu_ = nullptr;
    DropdownMenu* child_menu_ = nullptr;
    // Which row child_menu_ belongs to, so asking for the submenu that is
    // already showing costs nothing and asking for a different one replaces
    // it. Without the row, "a submenu is open" and "the RIGHT submenu is
    // open" are the same question, and the second one is the one that
    // matters when the highlight moves between two rows that both have
    // children.
    int child_index_ = -1;
    bool dismissing_ = false;
    bool pointer_pressed_ = false;
    MenuOpenReason open_reason_ = MenuOpenReason::Keyboard;
    std::function<bool(const MouseEvent&)> pointer_navigation_;
    // A popup replaces the view that invoked it in the focus chain. Command
    // availability must still describe that invoker, not the popup itself.
    std::vector<std::string> invocation_contexts_;
    bool has_invocation_contexts_ = false;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId highlighted_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
};

struct MenuBarItem {
    std::string label;  // may carry a '&' mnemonic
    std::vector<MenuItem> items;
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.menu.bar.normal"/"ckv.menu.bar.active" — its
// dropdowns resolve their own "ckv.menu.dropdown.*" roles the same
// way, so MenuBar no longer needs to hold or thread them through.
// Also reads context().app for focus save/restore, and finds its
// owning Desktop with a parent-chain walk at attach (real usage
// always docks a MenuBar directly onto the Desktop it controls, via
// Desktop::dock_top — see gallery_app.cpp).
//
// F10 activation (M9/WP-13, D-029): on_attached() installs itself as
// the standard menu command's default handler — but ONLY if nothing has
// claimed it yet (CommandRegistry::has_handler) — so an application
// that calls set_handler(commands().standard().menu, ...) itself before
// attaching a MenuBar is never silently overridden. The destructor clears the handler
// again if this instance was the one that installed it, so a
// destroyed MenuBar can never be reached through a stale handler.
// A trailing view that behaves as a title on the bar rather than as
// decoration beside it: the keyboard walks onto it, it highlights while it
// holds the walk, and Enter or Space acts on it.
//
// An interface rather than a concrete type because what drops out of such a
// title is the caller's business -- a calendar, a palette, anything. The bar
// keeps what a bar owns: the highlight, the walk, and the acting.
class MenuBarAccessory {
public:
    virtual ~MenuBarAccessory() = default;
    virtual void set_menu_highlighted(bool highlighted) = 0;
    virtual void activate_from_menu_bar() = 0;
};

class MenuBar : public ui::View {
public:
    // A view pinned to the right end of the bar -- a clock, an indicator,
    // anything an application wants permanently in view. It is a child, so
    // it draws and receives input normally; the bar only decides where it
    // sits, and re-decides on every resize so it stays at the right end
    // rather than where the right end used to be.
    // Typed insertion, as add_window/add_popup do: the bar hands back what
    // was put in, so a caller keeps its own type without a cast.
    template <class T>
    T* set_trailing_view(std::unique_ptr<T> view) {
        return static_cast<T*>(set_trailing_view_impl(std::move(view)));
    }
    ui::View* trailing_view() const noexcept { return trailing_view_; }
    void on_resized() override;
    // A trailing view whose own width changes is placed again, since where it
    // sits is the bar's decision -- a clock switched to seconds is two cells
    // wider than it was, and keeping the old width clips it.
    void on_child_size_hint_changed(ui::View& child) override;

    explicit MenuBar(std::vector<MenuBarItem> menus);
    ~MenuBar() override;

    void set_role_override(ui::RoleId normal_role, ui::RoleId active_role) noexcept {
        normal_role_ = normal_role;
        active_role_ = active_role;
    }
    void set_hotkey_role_override(ui::RoleId role) noexcept { hotkey_role_ = role; }

    // Gives the bar focus (saving whatever was previously focused, for
    // deactivate()/Esc to restore) and highlights the first menu. Also
    // callable directly by an application that wants its own trigger
    // for opening the menu, in addition to (or instead of) the F10
    // default this class installs itself — see the class comment.
    void activate();
    // Closes any open dropdown and restores focus to whatever was
    // focused before activate() was called.
    void deactivate();
    bool active() const noexcept { return active_; }

    void set_menus(std::vector<MenuBarItem> menus);
    const std::vector<MenuBarItem>& menus() const noexcept { return menus_; }

    // The command under the highlight in whatever dropdown this bar has
    // open, and kInvalidCommand once it closes. Wired once, it reports
    // every move for as long as the bar lives — see
    // DropdownMenu::on_highlight_changed for what a listener does with it.
    std::function<void(const MenuHighlight&)> on_highlight_changed;

    // The help topic of the row the reader is standing on, or empty. An
    // application's F1 handler consults this while a menu is open, so
    // that asking about a verb answers about that verb rather than about
    // whatever held focus before the menu opened.
    std::string highlighted_help_context() const;

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every row in the drop-down invokes or opens something.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void open_dropdown(std::size_t menu_index,
                        MenuOpenReason reason = MenuOpenReason::Keyboard);
    void close_dropdown();
    bool navigate_pointer(const MouseEvent& event);

    ui::View* trailing_view_ = nullptr;
    ui::View* set_trailing_view_impl(std::unique_ptr<ui::View> view);
    // The trailing view as a bar title, or nullptr when it is only
    // decoration. Decides whether the keyboard walk has one more stop.
    MenuBarAccessory* trailing_accessory() const noexcept;
    // Slots the walk can occupy: one per menu, plus the trailing title.
    std::size_t navigable_slots() const noexcept;
    bool trailing_slot_highlighted() const noexcept;
    void set_bar_highlight(std::size_t slot);
    void sync_trailing_highlight();
    void layout_trailing_view();
    std::vector<MenuBarItem> menus_;
    std::size_t highlighted_ = 0;
    // Whether walking the bar carries an open menu with it, as it does from
    // the moment one is opened until the reader closes it or leaves the bar.
    bool menus_follow_walk_ = false;
    bool active_ = false;
    std::optional<ui::Application::FocusBookmark> previously_focused_;
    std::vector<std::string> invocation_contexts_;
    DropdownMenu* open_dropdown_ = nullptr;  // observer into desktop_'s popup list

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId active_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
    ui::Application* app_ = nullptr;
    Desktop* desktop_ = nullptr;
    // Whether THIS instance installed kMenu's default handler (M9/
    // WP-13) — only then does the destructor clear it; a MenuBar that
    // found kMenu already claimed by something else must not touch it
    // on the way out.
    bool installed_default_menu_handler_ = false;
    // Alt+<mnemonic> accelerators this bar currently owns, so they can be
    // withdrawn when the menus change or the bar goes away — a stale
    // accelerator would open a menu that no longer exists.
    std::vector<ui::CommandId> menu_accelerators_;
    void install_menu_accelerators();
    void remove_menu_accelerators();
};

// Opens `items` as a positional pop-up context menu at `screen_position`
// (Desktop-absolute coordinates — e.g. the mouse cell from a right-
// click MouseEvent) with the same navigation/mnemonic/dismiss feature
// set as MenuBar's dropdown: input capture for light-dismiss on an
// outside click, Esc to close, Up/Down/Enter to navigate and activate.
// The returned DropdownMenu resolves its own roles from `desktop`'s
// context, same as one opened by a MenuBar; call set_role_override on
// it before returning control to the caller's event loop if it needs
// to look different from the standard menu-dropdown roles.
// Self-removing: the menu takes care of leaving `desktop`'s popup list
// and clearing input capture when it dismisses, so the caller does not
// need to track or explicitly close it. The returned pointer is only
// valid until the menu dismisses (any activation, Esc, or an outside
// click) — do not retain it past that point.
DropdownMenu* show_context_menu(std::vector<MenuItem> items, Point screen_position,
                                 ui::Application& app, Desktop& desktop);

// The standard portable keyboard context-menu chord. Terminals can report a
// physical Menu key through future backends, but Shift+F10 is the baseline
// chord already representable by ckVision's key model.
bool is_keyboard_context_menu_request(const KeyEvent& event) noexcept;

// Opens a context menu at the focused view's top-left cell when focus is
// inside `desktop`; otherwise uses the Desktop origin. This is the keyboard
// equivalent of a right-click positional context menu and deliberately accepts
// caller-provided items instead of installing a process-global context source.
DropdownMenu* show_context_menu_for_focus(std::vector<MenuItem> items, ui::Application& app,
                                          Desktop& desktop);

}  // namespace ckv::widgets
