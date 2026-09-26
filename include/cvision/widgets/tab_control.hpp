// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"
#include "cvision/widgets/mnemonic.hpp"

namespace ckv::widgets {

// Pages stacked behind a one-row strip of tab captions. Row 0 is the strip;
// the active page fills the rows below it at the control's full width, and
// every other page stays a hidden child. Each caption takes its label's width
// plus three cells: a space either side in the tab's colour and one cell of
// strip between tabs. Captions that run past the right edge are clipped.
//
// The control is a Tab stop. While a key reaches it, Left and Right step to
// the previous and next tab, wrapping around, and Alt plus a caption's
// mnemonic letter selects that tab. Tab (with any modifiers, Shift included)
// also steps to the next tab and is consumed, so Tab traversal does not move
// the focus out of a focused TabControl. A left-button press on a caption
// selects it.
//
// Resolves its theme roles from context() once attached, borrowing the menu
// family's: "ckv.menu.bar.normal" for the strip and inactive captions,
// "ckv.menu.bar.active" for the active caption, "ckv.dialog.background" for
// the page area, "ckv.hotkey" for the mnemonic accent, and
// "ckv.menu.dropdown.disabled"'s foreground for a disabled control (D-076),
// which also shows no mnemonic accent and no focus mark.
class TabControl : public ui::View {
public:
    // One tab as added: its caption with the '&' markers stripped, its page
    // (owned by the control as a child; non-owning here), and the marked
    // grapheme. add_tab always engages `mnemonic`; it holds an empty string
    // when the label marks none.
    struct Tab {
        // The caption drawn in the strip.
        std::string label;
        // The page shown while this tab is active.
        ui::View* page = nullptr;
        // The grapheme Alt selects this tab with; only its first byte is
        // compared, ASCII case-insensitively.
        std::optional<std::string> mnemonic;
    };

    // An empty control: a Tab stop preferring 20 by 6 cells.
    TabControl();

    // Appends a tab captioned `label` ('&' marks the mnemonic) and adopts
    // `page`, which must not be null, as a child. The first tab added becomes
    // active; later pages start hidden. Returns the page, or nullptr (adding
    // no tab) when attaching it detached or destroyed it.
    ui::View* add_tab(std::string label, std::unique_ptr<ui::View> page);
    // The number of tabs, and one of them by index; `index` must be below
    // tab_count().
    std::size_t tab_count() const noexcept { return tabs_.size(); }
    const Tab& tab(std::size_t index) const { return tabs_[index]; }

    // Shows the page at `index`, hides the previously active one and lays the
    // new one out below the strip. An index out of range, or the active one,
    // is ignored. No notification fires, and the focus is not moved.
    void set_active_index(std::size_t index);
    // The active tab's index (0 while there are no tabs) and its page, or
    // nullptr when there are no tabs.
    std::size_t active_index() const noexcept { return active_index_; }
    ui::View* active_page() const noexcept;

    void on_attached() override;
    void on_resized() override;
    void draw(scene::Painter& painter) override;
    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;
    bool on_key(const KeyEvent& event) override;
    bool on_mouse(const MouseEvent& event) override;
    // The strip of tabs switches pages when clicked.
    std::optional<PointerShape> pointer_shape_at(Point) const override {
        return PointerShape::Pointer;
    }
    void on_focus(const FocusEvent& event) override;

private:
    int tab_start_x(std::size_t index) const;
    int tab_at_x(int local_x) const;
    void activate_delta(int delta);

    std::vector<Tab> tabs_;
    // Each tab's label as parsed, for drawing its mnemonic accent.
    std::vector<MnemonicText> captions_;
    std::size_t active_index_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId active_role_ = ui::kInvalidRole;
    ui::RoleId page_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
