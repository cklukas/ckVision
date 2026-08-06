// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// CheckGroup (multi-select) and RadioGroup (exclusive-select): arrow
// navigation, mnemonics (plans/widgets.md M6a baseline). Each group is
// ONE Tab stop — arrows move an internal cursor among the group's own
// items rather than each item being independently focusable, matching
// the classic clustered check/radio control.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

using ui::SizeHint;

class CheckGroup : public ui::View {
public:
    // `labels` may each carry a '&' mnemonic.
    CheckGroup(std::vector<std::string> labels, const ui::Theme& theme, ui::RoleId normal_role,
               ui::RoleId focused_role);

    bool checked(std::size_t index) const;
    void set_checked(std::size_t index, bool value);

    // Fired whenever an item's checked state changes, with its index
    // and new state.
    std::function<void(std::size_t, bool)> on_changed;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;

private:
    void toggle(std::size_t index);
    void move_cursor(int direction);

    std::vector<std::string> labels_;
    std::vector<bool> checked_;
    std::size_t cursor_ = 0;
    bool has_focus_ = false;
    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId focused_role_;
};

class RadioGroup : public ui::View {
public:
    RadioGroup(std::vector<std::string> labels, const ui::Theme& theme, ui::RoleId normal_role,
               ui::RoleId focused_role);

    int selected() const noexcept { return selected_; }  // -1 means none selected
    void set_selected(int index);

    // Fired whenever selection changes, with the newly selected index.
    std::function<void(int)> on_changed;

    void draw(scene::Painter& painter) override;
    SizeHint horizontal_size_hint() const override;
    SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    void on_focus(const FocusEvent& event) override;

private:
    void move_cursor(int direction);

    std::vector<std::string> labels_;
    int selected_ = -1;
    std::size_t cursor_ = 0;
    bool has_focus_ = false;
    const ui::Theme& theme_;
    ui::RoleId normal_role_;
    ui::RoleId focused_role_;
};

}  // namespace ckv::widgets
