// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// MenuBar + DropdownMenu (ARCHITECTURE.md §5 "Menus" / plans/
// widgets.md M5 baseline): F10-equivalent activation (activate(),
// installed as ui::std_commands::kMenu's default handler when
// unclaimed — M9/WP-13, D-029 — routing through the command keymap
// like every other accelerator in this framework), mnemonic letters,
// Left/Right/Up/Down navigation with wrapping, Enter/mnemonic to
// activate, disabled items skipped by navigation but still visible,
// Esc closes one level, light-dismiss on an outside click via
// Application's mouse input capture, and right-aligned chord hints
// rendered live from the command registry (M9/WP-11).
//
// show_context_menu() below reuses DropdownMenu directly as a
// positional pop-up ("Context menu: positional pop-up with same
// feature set" — plans/widgets.md M5 baseline) at a caller-chosen
// screen position, e.g. from a right-click handler.
//
// Scope note (documented, not an oversight): nested submenus,
// checkable items, and context-menu KEYBOARD invocation (Menu key /
// Shift-F10 at focus location — mouse invocation via
// show_context_menu is covered) are each a real, separate increment —
// landing them here would roughly double this file without a
// concrete consumer to test any of it against yet.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/command.hpp"
#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/desktop.hpp"

namespace ckv::widgets {

using ui::SizeHint;
using ui::View;

struct MenuItem {
    std::string label;                     // may carry a '&' mnemonic; ignored if separator
    ui::CommandId command = ui::kInvalidCommand;  // if set, activation calls app.execute_command(command)
    std::function<void()> on_activate;     // used when command == kInvalidCommand
    bool separator = false;                // a divider: never highlighted, navigated to, or activated
};

// Resolves its own theme roles from context() once attached (M9
// WP-7, D-028): "ckv.menu.dropdown.normal"/"highlighted"/"disabled".
// Also reads context().app for command enablement — see the file
// comment on why menus are one of the few widgets that need it.
class DropdownMenu : public ui::View {
public:
    explicit DropdownMenu(std::vector<MenuItem> items);
    ~DropdownMenu() override;

    void set_role_override(ui::RoleId normal_role, ui::RoleId highlighted_role,
                            ui::RoleId disabled_role) noexcept {
        normal_role_ = normal_role;
        highlighted_role_ = highlighted_role;
        disabled_role_ = disabled_role;
    }

    // Fires on Esc, on a click outside the dropdown's own bounds (light
    // dismiss), after a successful item activation, AND unconditionally
    // from the destructor — so an owner (MenuBar) always learns the
    // popup is going away regardless of WHO removed it (itself, or a
    // caller bypassing it via Desktop::remove_popup directly), and can
    // reliably clear its own bookkeeping instead of desyncing. May
    // therefore fire more than once for the same dismissal (e.g. once
    // from an explicit dismiss() and again from the destructor it
    // triggers); handlers must be idempotent — MenuBar::close_dropdown()
    // already is.
    std::function<void()> on_dismiss;

    const std::vector<MenuItem>& items() const noexcept { return items_; }
    int highlighted() const noexcept { return highlighted_; }

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
    void dismiss();

    std::vector<MenuItem> items_;
    int highlighted_ = -1;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId highlighted_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
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
// ui::std_commands::kMenu's default handler — but ONLY if nothing has
// claimed kMenu yet (CommandRegistry::has_handler) — so an application
// that calls set_handler(kMenu, ...) itself before attaching a MenuBar
// is never silently overridden. The destructor clears the handler
// again if this instance was the one that installed it, so a
// destroyed MenuBar can never be reached through a stale handler.
class MenuBar : public ui::View {
public:
    explicit MenuBar(std::vector<MenuBarItem> menus);
    ~MenuBar() override;

    void set_role_override(ui::RoleId normal_role, ui::RoleId active_role) noexcept {
        normal_role_ = normal_role;
        active_role_ = active_role;
    }

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

    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;
    void on_attached() override;

private:
    void open_dropdown(std::size_t menu_index);
    void close_dropdown();

    std::vector<MenuBarItem> menus_;
    std::size_t highlighted_ = 0;
    bool active_ = false;
    ui::View* previously_focused_ = nullptr;
    DropdownMenu* open_dropdown_ = nullptr;  // observer into desktop_'s popup list

    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId active_role_ = ui::kInvalidRole;
    ui::Application* app_ = nullptr;
    Desktop* desktop_ = nullptr;
    // Whether THIS instance installed kMenu's default handler (M9/
    // WP-13) — only then does the destructor clear it; a MenuBar that
    // found kMenu already claimed by something else must not touch it
    // on the way out.
    bool installed_default_menu_handler_ = false;
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

}  // namespace ckv::widgets
