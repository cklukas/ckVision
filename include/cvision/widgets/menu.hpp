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
// supplies the keyboard path: a view that owns a context menu answers the Menu
// key or Shift+F10 (is_keyboard_context_menu_request) by calling it, and the
// menu opens at the focused view's cell location without any global menu
// registry.
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
// flag because there is nothing else that could know. The check mark
// follows the same rule: a Command item whose command is a toggle
// (CommandRegistry::set_checked_predicate) shows the registry's state
// without a with_mark of its own, as a tool bar presenting the same command
// does.
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
    // Read current application state when drawing a menu that remains open
    // across command changes (for example, a selected theme or view mode).
    [[nodiscard]] MenuItem with_mark_provider(std::function<MenuMark()> provider) const;
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
    MenuMark mark() const { return mark_provider_ ? mark_provider_() : mark_; }
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
    std::function<MenuMark()> mark_provider_;
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
    // The command the row runs; kInvalidCommand for an Action row, a submenu
    // parent, a separator, or when no row is highlighted.
    ui::CommandId command = ui::kInvalidCommand;
    // The row's help topic (MenuItem::with_help), or empty.
    std::string help_context;
    // The row's own explanation of why it is unavailable
    // (MenuItem::with_disabled_reason), or empty. Reported whether or not the
    // row is currently enabled; a listener decides whether to show it.
    std::string disabled_reason;
    // Whether the row would act if chosen: a Command row's availability in the
    // registry (for the focus the menu was opened from), an Action or Submenu
    // row's own flag. Stays true when `none` is set.
    bool enabled = true;
    // No row is highlighted (the menu closed, or the pointer left it).
    bool none = false;
};

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
// done with the menu. Cancelling (Esc) leaves the menu system to decide how
// far to unwind, one level at a time. The distinction matters beyond
// appearances — whatever the command then does (open a dialog, say) sees the
// focus the menu left behind, so a bar that stays focused hands the command a
// focus target the reader never chose, and it comes back highlighted once the
// dialog closes.
//
// Outside is the light dismiss (the architecture §5 "Windows, popups"): a
// pointer press outside every menu of the chain, or the release of a gesture
// that ended outside all of them. The reader has turned to something else, so
// it too ends the whole menu interaction: a menu bar deactivates and hands the
// focus back to where it found it, and nothing runs. The press itself is
// consumed by the dismissal and never reaches what lies beneath the menu.
enum class MenuDismissReason {
    Cancelled,
    ItemChosen,
    Outside,
};

// One open menu popup: a framed column of MenuItem rows, with a left mark
// column when any row carries a mark and a right-aligned column for chord
// hints and the submenu marker. It is what a MenuBar drops down, what
// show_context_menu() puts up, and what a submenu row opens beside its parent.
// A DropdownMenu is meant to live in a Desktop's popup list; it finds that
// Desktop through its parents when attached.
//
// Resolves its own theme roles from context() once attached (M9 WP-7, D-028):
// "ckv.menu.dropdown.normal"/"highlighted"/"disabled", and "ckv.hotkey" for
// the mnemonic accent. Also reads context().app to ask whether each Command
// row's command is available.
class DropdownMenu : public ui::View {
public:
    // `items` are the rows, in order. `parent_menu` is the menu whose submenu
    // row this one hangs from, or nullptr for a menu that starts a chain (the
    // one a MenuBar opens, or a context menu); it is not owned and must
    // outlive this menu. The highlight is placed on the first reachable row
    // when the menu is attached, unless it was opened by a pointer press.
    explicit DropdownMenu(std::vector<MenuItem> items, DropdownMenu* parent_menu = nullptr);
    // Dismisses the menu (and any submenu it has open), so on_dismiss fires
    // even when the popup was removed by someone else.
    ~DropdownMenu() override;

    // Replace the roles this menu draws with: plain rows, the highlighted row,
    // and unavailable rows. A role left as kInvalidRole is resolved from the
    // standard names at attach; an override set after attach repaints the
    // menu. Submenus this menu opens resolve their own roles.
    void set_role_override(ui::RoleId normal_role, ui::RoleId highlighted_role,
                            ui::RoleId disabled_role) noexcept {
        if (normal_role_ == normal_role && highlighted_role_ == highlighted_role &&
            disabled_role_ == disabled_role)
            return;
        normal_role_ = normal_role;
        highlighted_role_ = highlighted_role;
        disabled_role_ = disabled_role;
        invalidate();
    }
    // The role whose colours accent each row's mnemonic letter, in place of
    // "ckv.hotkey".
    void set_hotkey_role_override(ui::RoleId role) noexcept {
        if (hotkey_role_ == role) return;
        hotkey_role_ = role;
        invalidate();
    }

    // Fires on Esc (MenuDismissReason::Cancelled), on a press outside every
    // menu of its chain (light dismiss, MenuDismissReason::Outside), after a
    // successful item activation (carrying
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

    // The rows as constructed; a menu's rows do not change while it is open.
    const std::vector<MenuItem>& items() const noexcept { return items_; }
    // Index into items() of this menu's own highlighted row, or -1 when none
    // is: a menu opened by a pointer press that has not yet settled on a row,
    // or a menu with no reachable row. It does not follow an open submenu —
    // see highlight() for that.
    int highlighted() const noexcept { return highlighted_; }
    // The command behind the highlighted item, or kInvalidCommand.
    ui::CommandId highlighted_command() const noexcept;
    // Everything about the row the reader is standing on, following any
    // open submenu chain to its innermost menu: that is where the reader
    // is, and that is the row a status line should explain and F1 should
    // answer about.
    MenuHighlight highlight() const;

    // Fixed size (min, preferred and max agree): width fits the longest item
    // label plus its mark column, its chord hint or submenu marker column, the
    // padding and the one-cell frame; height is one row per item (including
    // separators) plus the two frame rows.
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool casts_shadow() const noexcept override { return true; }
    // Up/Down move the highlight over reachable rows, wrapping at the ends,
    // and Home/End jump to the first/last of them. Enter or an enabled row's
    // mnemonic letter chooses a row (Enter on an unavailable row does
    // nothing), Right
    // opens the highlighted row's submenu, Left closes this menu when it is a
    // submenu, and Esc dismisses it. Right on a row without a submenu, Left on
    // a top-level menu, and every other key are left unhandled so a MenuBar
    // above can act on them. While a submenu is open every key goes to the
    // innermost open menu instead: that is how a context menu, which keeps
    // the keyboard focus on its root, walks and closes its submenus one level
    // at a time.
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every row in the drop-down invokes or opens something.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_attached() override;

private:
    // MenuBar, show_context_menu() and show_anchored_menu() set up a menu
    // before it is shown (open reason, invocation contexts, pointer
    // navigation) and drive a bar's dropdown from the bar's own key handling,
    // through the private members below.
    friend class MenuBar;
    friend DropdownMenu* show_context_menu(std::vector<MenuItem> items, Point screen_position,
                                           ui::Application& app, Desktop& desktop);
    friend DropdownMenu* show_anchored_menu(std::vector<MenuItem> items, Rect anchor, ui::Application& app,
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
    bool has_check_column() const;
    // The mark a row shows: its own (with_mark, with_mark_provider) when it
    // has one, otherwise, for a Command row, its command's checked state from
    // the registry (CommandRegistry::checked), otherwise none.
    MenuMark item_mark(const MenuItem& item) const;
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

// One title on a MenuBar and the menu it drops down.
struct MenuBarItem {
    // The title drawn on the bar. A '&' marks its mnemonic letter, which the
    // bar also binds as an Alt+<letter> accelerator once it is attached.
    std::string label;  // may carry a '&' mnemonic
    // The rows of the dropdown this title opens.
    std::vector<MenuItem> items;
};

// A trailing view that behaves as a title on the bar rather than as
// decoration beside it: the keyboard walks onto it, it highlights while it
// holds the walk, and Enter or Space acts on it.
//
// An interface rather than a concrete type because what drops out of such a
// title is the caller's business -- a calendar, a palette, anything. The bar
// keeps what a bar owns: the highlight, the walk, and the acting.
//
// A MenuBar recognises a trailing view as an accessory by a dynamic_cast, so
// the view implements this interface alongside ui::View.
class MenuBarAccessory {
public:
    // Destroyed through the view that implements it; the bar never owns an
    // accessory through this interface.
    virtual ~MenuBarAccessory() = default;
    // Whether the bar's keyboard walk is standing on this title. The bar calls
    // it whenever that changes, including with false when the walk ends or the
    // bar loses focus; the accessory draws itself highlighted accordingly.
    virtual void set_menu_highlighted(bool highlighted) = 0;
    // Enter or Space while the walk stands on this title. The bar has already
    // ended the walk and restored the focus it saved when it was activated, so
    // anything the accessory opens saves and later restores that focus.
    virtual void activate_from_menu_bar() = 0;
};

// The one-row strip of menu titles across the top of a Desktop. Titles are
// drawn from two cells in, two cells apart; the keyboard walks them with
// Left/Right, opens a title's DropdownMenu with Enter, Down or its mnemonic,
// and a pointer press on a title opens it too. The bar holds keyboard focus
// for as long as it is being walked and hands it back when the walk ends.
// Esc closes one level: out of a submenu to the row that opened it, out of a
// dropdown to its title with the walk still on the bar, and off the bar,
// restoring the focus the walk began from.
//
// Overflow: a bar too narrow for all its titles draws the ones that fit in
// full, never a clipped one, and ends them with the overflow title "»". Its
// dropdown lists the hidden titles, each a submenu holding that menu's items.
// The overflow title is a stop on the keyboard walk like any title, a press
// on it opens its list, and a hidden menu's mnemonic opens the list with that
// menu already entered.
//
// Resolves its own theme roles from context() once attached (M9 WP-7, D-028):
// "ckv.menu.bar.normal"/"ckv.menu.bar.active", and "ckv.hotkey" for mnemonic
// letters. Its dropdowns resolve their own "ckv.menu.dropdown.*" roles the same
// way. Also reads context().app for focus save/restore, and finds its owning
// Desktop with a parent-chain walk at attach, so the bar must be a descendant
// of the Desktop its dropdowns open on (usually docked with Desktop::dock_top).
//
// F10 activation (M9/WP-13, D-029): on_attached() installs activate() as the
// standard menu command's default handler — but ONLY if nothing has claimed it
// yet (CommandRegistry::has_handler) — so an application that calls
// set_handler(commands().standard().menu, ...) itself before attaching a
// MenuBar is never silently overridden. The destructor clears the handler
// again if this instance was the one that installed it, so a destroyed
// MenuBar can never be reached through a stale handler.
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
    // A pointer going down on the trailing view while the bar is being walked
    // ends the walk before the view reacts: the press is a hand-off.
    void on_descendant_mouse_down(ui::View& target) override;

    // `menus` are the titles in left-to-right order. The bar is a tab stop;
    // accelerators and the F10 handler are installed when it is attached.
    explicit MenuBar(std::vector<MenuBarItem> menus);
    // Closes any open dropdown and withdraws the F10 default handler if this
    // bar installed it.
    ~MenuBar() override;

    // Replace the roles the bar draws with: the strip and its idle titles, and
    // the title the walk stands on while the bar has focus. A role left as
    // kInvalidRole is resolved from the standard names at attach; an override
    // set after attach repaints the bar.
    void set_role_override(ui::RoleId normal_role, ui::RoleId active_role) noexcept {
        if (normal_role_ == normal_role && active_role_ == active_role) return;
        normal_role_ = normal_role;
        active_role_ = active_role;
        invalidate();
    }
    // The role whose colours accent each title's mnemonic letter, in place of
    // "ckv.hotkey".
    void set_hotkey_role_override(ui::RoleId role) noexcept {
        if (hotkey_role_ == role) return;
        hotkey_role_ = role;
        invalidate();
    }

    // Gives the bar focus (saving whatever was previously focused, for
    // deactivate()/Esc to restore) and highlights the first menu. Also
    // callable directly by an application that wants its own trigger
    // for opening the menu, in addition to (or instead of) the F10
    // default this class installs itself — see the class comment. On a bar
    // that already has focus it closes any open dropdown and goes back to
    // the first menu. The bar must be attached under an Application.
    void activate();
    // Closes any open dropdown and restores focus to whatever was
    // focused before activate() was called.
    void deactivate();
    bool active() const noexcept { return has_focus(); }

    // Replaces the titles. Any open dropdown is closed first, the old
    // Alt+<mnemonic> accelerators are withdrawn and new ones installed (when
    // attached) unless the titles are the same ones in the same order, in
    // which case the accelerators already installed are kept, and a walk that
    // was past the new last title moves back onto it. The bar asks its parent
    // to re-lay it out.
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
    // Handles keys only while the bar has focus, i.e. while it is being
    // walked; an unfocused bar is reached through F10 and the Alt+<mnemonic>
    // accelerators instead. While a dropdown is open, navigation keys go to
    // its innermost open menu first, and Esc closes exactly one level: the
    // innermost submenu, else the dropdown (the walk stays on its title), else
    // the walk itself.
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // Every title on the bar opens something.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    // Losing focus ends the walk however it happened: the open dropdown
    // closes and the saved focus is forgotten, so a later deactivate() does
    // not return focus to a view the reader has since left.
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

    // How many titles, from the first, the bar draws in its own width. The
    // rest are hidden behind the overflow title, which stands at the end of
    // the drawn ones and drops a menu listing them. All of them when every
    // title fits; none on a bar that has no room even for one.
    std::size_t visible_menu_count() const;
    // Whether some titles are hidden behind the overflow title.
    bool overflowing() const { return visible_menu_count() < menus_.size(); }

private:
    // Drops the dropdown of the title that holds `menu_index`: the menu's own
    // title when it is drawn, the overflow title when it is hidden. The
    // overflow list opens on the hidden menu's row, without entering it.
    void open_dropdown(std::size_t menu_index,
                        MenuOpenReason reason = MenuOpenReason::Keyboard);
    // Opens menu `menu_index` itself, wherever its title is: a hidden menu
    // opens as the overflow list with that menu's submenu open inside it. What
    // a mnemonic asks for.
    void open_menu(std::size_t menu_index);
    void close_dropdown();
    bool navigate_pointer(const MouseEvent& event);
    // The rows of the overflow list: each hidden title as a submenu of its
    // own items, in bar order.
    std::vector<MenuItem> overflow_items() const;
    // Whether `index` (a menu index, or menus_.size() for the trailing title)
    // is a hidden menu, and so is shown by the overflow title.
    bool is_hidden_menu(std::size_t index) const {
        return index >= visible_menu_count() && index < menus_.size();
    }
    // The walk's position among the bar's stops -- the drawn titles, then the
    // overflow title, then the trailing title -- for a highlight, and back.
    // Every hidden menu is one stop: the overflow title.
    std::size_t walk_position(std::size_t index) const;
    std::size_t index_at_walk_position(std::size_t position) const;
    // The column the overflow title starts in, just past the drawn titles.
    int overflow_title_x() const;
    // The first column after the drawn titles (the overflow title included).
    int titles_end() const;
    // The menu index whose title covers bar column `local_x` -- the first
    // hidden one for the overflow title -- or nullopt between titles.
    std::optional<std::size_t> menu_at_column(int local_x) const;

    ui::View* trailing_view_ = nullptr;
    ui::View* set_trailing_view_impl(std::unique_ptr<ui::View> view);
    // The trailing view as a bar title, or nullptr when it is only
    // decoration. Decides whether the keyboard walk has one more stop.
    MenuBarAccessory* trailing_accessory() const noexcept;
    // Stops the walk can occupy: one per drawn title, the overflow title when
    // titles are hidden, and the trailing title.
    std::size_t navigable_slots() const;
    bool trailing_slot_highlighted() const noexcept;
    // Puts the walk on `index` (a menu, or menus_.size() for the trailing
    // title) and brings what is open into agreement with it.
    void set_bar_highlight(std::size_t index);
    void sync_trailing_highlight();
    // Ends the walk, then activates the trailing title -- in that order.
    void activate_trailing_accessory();
    void layout_trailing_view();
    std::vector<MenuBarItem> menus_;
    // The menu the walk stands on, or menus_.size() for the trailing title. A
    // hidden menu's index means the walk stands on the overflow title.
    std::size_t highlighted_ = 0;
    // Whether the open dropdown is the overflow list, so a resize that shows
    // or hides the highlighted menu's title can replace it with the right one.
    bool open_dropdown_is_overflow_ = false;
    // Whether walking the bar carries an open menu with it, as it does from
    // the moment one is opened until the reader closes it or leaves the bar.
    bool menus_follow_walk_ = false;
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
// click) — do not retain it past that point. A listener the caller sets on
// its on_highlight_changed hears a `none` highlight as the menu closes; the
// menu has already settled on its first row by the time it is returned, so
// the caller reads that opening position from highlight().
DropdownMenu* show_context_menu(std::vector<MenuItem> items, Point screen_position,
                                 ui::Application& app, Desktop& desktop);

// The same menu hanging from a control rather than opened at a point:
// `anchor` is that control's screen rect (Desktop-absolute, as
// View::absolute_bounds() gives it). The menu's left edge is the anchor's,
// and it opens below the anchor when its rows fit beneath it, above it when
// they fit there instead — so a bar docked at the bottom of the desktop drops
// its menus upward — and below it, clamped onto the desktop, when they fit on
// neither side. Everything else is show_context_menu's: the keyboard and
// input capture, the commands' availability for the focus it opened from,
// the dismissal and the focus handed back.
DropdownMenu* show_anchored_menu(std::vector<MenuItem> items, Rect anchor, ui::Application& app,
                                 Desktop& desktop);

// Whether `event` asks for the focused view's context menu from the keyboard:
// a press of the Menu key on its own, or of Shift+F10, the portable chord
// every terminal can send. A view answers it by opening its menu at the
// focus -- show_context_menu_for_focus, or its own caret position -- exactly
// as a right click opens it where the pointer is. Releases and repeats never
// ask.
bool is_keyboard_context_menu_request(const KeyEvent& event) noexcept;

// Opens a context menu at the focused view's top-left cell when focus is
// inside `desktop`; otherwise uses the Desktop origin. This is the keyboard
// equivalent of a right-click positional context menu and deliberately accepts
// caller-provided items instead of installing a process-global context source.
DropdownMenu* show_context_menu_for_focus(std::vector<MenuItem> items, ui::Application& app,
                                          Desktop& desktop);

}  // namespace ckv::widgets
