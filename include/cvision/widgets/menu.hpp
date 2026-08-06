// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// MenuBar + DropdownMenu (ARCHITECTURE.md §5 "Menus" / plans/
// widgets.md M5 baseline): F10-equivalent activation (activate(), a
// caller binds F10 to it via CommandRegistry — MenuBar does not
// intercept F10 itself, matching how every other accelerator in this
// framework routes through the command keymap), mnemonic letters,
// Left/Right/Up/Down navigation with wrapping, Enter/mnemonic to
// activate, disabled items skipped by navigation but still visible,
// Esc closes one level, and light-dismiss on an outside click via
// Application's mouse input capture.
//
// Scope note (documented, not an oversight): nested submenus, right-
// aligned accelerator-chord hints, checkable items, context-menu
// keyboard invocation, and the status line are each a real, separate
// increment — landing them here would roughly double this file without
// a concrete consumer to test any of it against yet. This delivers one
// complete, working menu bar + single-level dropdown, which is already
// the whole "commands-driven menu" story end to end.
#pragma once

#include <functional>
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

class DropdownMenu : public ui::View {
public:
    DropdownMenu(std::vector<MenuItem> items, const ui::Theme& theme, ui::RoleId normal_role,
                 ui::RoleId highlighted_role, ui::RoleId disabled_role, ui::Application& app);

    // Fires exactly once: on Esc, on a click outside the dropdown's own
    // bounds (light dismiss), or after a successful item activation.
    std::function<void()> on_dismiss;

    const std::vector<MenuItem>& items() const noexcept { return items_; }
    int highlighted() const noexcept { return highlighted_; }

    // Preferred size: width fits the longest item label (+ padding),
    // height is exactly one row per item (including separators).
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;

private:
    bool item_enabled(std::size_t index) const;
    int step_selection(int from, int direction) const;  // -1 if no selectable item exists at all
    void activate(int index);
    void dismiss();

    std::vector<MenuItem> items_;
    int highlighted_ = -1;
    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId highlighted_role_;
    ui::RoleId disabled_role_;
    ui::Application& app_;
};

struct MenuBarItem {
    std::string label;  // may carry a '&' mnemonic
    std::vector<MenuItem> items;
};

class MenuBar : public ui::View {
public:
    MenuBar(std::vector<MenuBarItem> menus, const ui::Theme& theme, ui::RoleId normal_role,
            ui::RoleId active_role, ui::RoleId dropdown_normal_role, ui::RoleId dropdown_highlighted_role,
            ui::RoleId dropdown_disabled_role, ui::Application& app, Desktop& desktop);
    ~MenuBar() override;

    // Gives the bar focus (saving whatever was previously focused, for
    // deactivate()/Esc to restore) and highlights the first menu. A
    // caller wires this to F10 via CommandRegistry — see the file
    // comment for why MenuBar doesn't intercept F10 itself.
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

private:
    void open_dropdown(std::size_t menu_index);
    void close_dropdown();

    std::vector<MenuBarItem> menus_;
    std::size_t highlighted_ = 0;
    bool active_ = false;
    ui::View* previously_focused_ = nullptr;
    DropdownMenu* open_dropdown_ = nullptr;  // observer into desktop_'s popup list

    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId active_role_;
    ui::RoleId dropdown_normal_role_;
    ui::RoleId dropdown_highlighted_role_;
    ui::RoleId dropdown_disabled_role_;
    ui::Application& app_;
    Desktop& desktop_;
};

}  // namespace ckv::widgets
