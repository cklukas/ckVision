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
// strip between tabs.
//
// Overflow. When the captions do not all fit, the strip shows a run of them
// and scrolls, and the active caption is always one of the run. A "◂" in the
// first column says captions are hidden to the left, a "▸" in the last
// column that captions are hidden to the right; each takes its column only
// while it has something to say. Switching tabs scrolls the strip just far
// enough to show the new active caption. A left-button press on a mark
// scrolls the strip one caption that way, and when that carries the active
// caption off the strip the selection moves with it, to the nearest caption
// still shown. A resize or a new tab keeps the active caption shown and
// scrolls back as far as the room allows without hiding a caption on the
// right. A caption wider than the whole strip is shown on its own, clipped.
//
// The control is a Tab stop. While a key reaches it, Left and Right step to
// the previous and next tab, wrapping around, and Alt plus a caption's
// mnemonic letter selects that tab. Tab and Shift+Tab are left unhandled, so
// focus traversal moves into the page and out of the control as it does
// anywhere else. A left-button press on a caption selects it.
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

    // Shows the page at `index`, hides the previously active one, lays the
    // new one out below the strip and scrolls the strip to show its caption.
    // An index out of range, or the active one, is ignored. No notification
    // fires, and the focus is not moved.
    void set_active_index(std::size_t index);
    // The active tab's index (0 while there are no tabs) and its page, or
    // nullptr when there are no tabs.
    std::size_t active_index() const noexcept { return active_index_; }
    ui::View* active_page() const noexcept;
    // The index of the first caption the strip shows: 0 while every caption
    // fits, and otherwise wherever the strip has scrolled to.
    std::size_t first_visible_index() const noexcept { return first_visible_; }

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
    // Where the captions fall with the strip scrolled to `first`: which of
    // them are shown, at which columns, and which scroll marks are drawn.
    struct StripLayout {
        std::size_t first = 0;
        // One past the last caption shown.
        std::size_t end = 0;
        bool left_mark = false;
        bool right_mark = false;
        // The column each shown caption starts at, the first caption's first.
        std::vector<int> x;
        // The column captions end before: the width, less the right mark's.
        int limit = 0;
    };
    StripLayout layout_from(std::size_t first) const;
    int caption_width(std::size_t index) const;
    int tab_at_x(const StripLayout& strip, int local_x) const;
    void activate_delta(int delta);
    // Scrolls just far enough that the active caption is shown.
    void keep_active_visible();
    // After a resize or a new tab: keeps the active caption shown, then
    // scrolls back while that hides no caption on the right.
    void settle_strip();
    // A click on a scroll mark: one caption toward `delta`'s side.
    void scroll_strip(int delta);

    std::vector<Tab> tabs_;
    // Each tab's label as parsed, for drawing its mnemonic accent.
    std::vector<MnemonicText> captions_;
    std::size_t active_index_ = 0;
    std::size_t first_visible_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId active_role_ = ui::kInvalidRole;
    ui::RoleId page_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId hotkey_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
