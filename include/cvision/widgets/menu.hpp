// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// MenuBar + DropdownMenu (ARCHITECTURE.md §5 "Menus" / plans/
// widgets.md M5 baseline): F10-equivalent activation (activate(),
// installed as the standard menu command's default handler when
// unclaimed — M9/WP-13, D-029 — routing through the command keymap
// like every other accelerator in this framework), mnemonic letters,
// Left/Right/Up/Down navigation with wrapping, Enter/mnemonic to
// activate, disabled items skipped by navigation but still visible,
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
// feature set" — plans/widgets.md M5 baseline) at a caller-chosen
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

enum class MenuItemCheck {
    None,
    Unchecked,
    Checked,
};

struct MenuItem {
    MenuItem() = default;
    MenuItem(std::string item_label, ui::CommandId command_id = ui::kInvalidCommand,
             std::function<void()> activation = {}, bool is_separator = false,
             MenuItemCheck check_state = MenuItemCheck::None,
             std::vector<MenuItem> child_items = {},
             CommandPresentation command_presentation = {})
        : label(std::move(item_label)),
          command(command_id),
          on_activate(std::move(activation)),
          separator(is_separator),
          check(check_state),
          submenu(std::move(child_items)),
          presentation(std::move(command_presentation)) {}
    explicit MenuItem(CommandPresentation command_presentation,
                      MenuItemCheck check_state = MenuItemCheck::None,
                      std::vector<MenuItem> child_items = {})
        : check(check_state),
          submenu(std::move(child_items)),
          presentation(std::move(command_presentation)) {}

    std::string label;                     // may carry a '&' mnemonic; ignored if separator
    ui::CommandId command = ui::kInvalidCommand;  // if set, activation calls app.execute_command(command)
    std::function<void()> on_activate;     // used when command == kInvalidCommand
    bool separator = false;                // a divider: never highlighted, navigated to, or activated
    MenuItemCheck check = MenuItemCheck::None;
    std::vector<MenuItem> submenu;
    CommandPresentation presentation;
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
    std::function<void(ui::CommandId)> on_highlight_changed;

    const std::vector<MenuItem>& items() const noexcept { return items_; }
    int highlighted() const noexcept { return highlighted_; }
    // The command behind the highlighted item, or kInvalidCommand.
    ui::CommandId highlighted_command() const noexcept;

    // Preferred size: width fits the longest item label plus its chord
    // hint column (+ padding), height is exactly one row per item
    // (including separators).
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool casts_shadow() const noexcept override { return true; }
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_attached() override;

private:
    friend class MenuBar;
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
    ui::Application* app_ = nullptr;
    Desktop* desktop_ = nullptr;
    DropdownMenu* parent_menu_ = nullptr;
    DropdownMenu* child_menu_ = nullptr;
    bool dismissing_ = false;
    bool pointer_pressed_ = false;
    MenuOpenReason open_reason_ = MenuOpenReason::Keyboard;
    std::function<bool(const MouseEvent&)> pointer_navigation_;
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
    std::function<void(ui::CommandId)> on_highlight_changed;

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
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
    ui::View* previously_focused_ = nullptr;
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
