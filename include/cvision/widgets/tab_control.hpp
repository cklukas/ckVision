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

// Geometry of a TabControl; colours and attributes come from ckv.tab.* roles.
enum class TabPresentation {
    Underlined,  // Two rows: captions and a baseline with a heavy selected segment.
    Framed,      // Three header rows and an inset, single-line framed page.
    Compact,     // One row, with color and bold weight marking selection.
};

// Owned, mutually exclusive pages behind a configurable tab strip (D-117).
// Underlined is the default. Left/Right wrap, Alt+mnemonic selects a page,
// and Tab/Shift+Tab remain focus traversal. Clicking a caption selects it.
// Focus underlines the selected label; selection survives focus leaving.
//
// Overflow reserves both end columns, drawing ◂/▸ only where captions are
// hidden. Selection stays visible. Clicking an arrow scrolls one caption;
// if selection would leave the strip it moves to the nearest visible tab
// (D-100). Oversized captions use a complete-grapheme ellipsis.
//
// Dedicated roles: ckv.tab.normal/selected/focused/disabled/mnemonic/
// separator/page. Disabled controls retain selection geometry and background,
// with the disabled foreground and no focus or mnemonic accent.
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

    // Changes geometry immediately, relays out the active page, keeps its
    // caption visible, and notifies the parent of changed size hints.
    void set_presentation(TabPresentation presentation);
    TabPresentation presentation() const noexcept { return presentation_; }

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
    // new one out in the presentation's page rectangle and scrolls the strip to show its caption.
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
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    void on_focus(const FocusEvent& event) override;

private:
    struct TabGeometry {
        std::size_t index = 0;
        Rect hit_bounds;
        Rect label_bounds;
    };
    struct TabLayout {
        Rect page_bounds;
        std::vector<TabGeometry> tabs;
        std::optional<Rect> previous_scroll;
        std::optional<Rect> next_scroll;
    };
    struct TabStyles {
        Style normal;
        Style selected;
        Style focused;
        Style mnemonic;
        Style separator;
        Style page;
    };
    TabLayout layout_from(std::size_t first) const;
    Rect page_bounds() const;
    int caption_width(std::size_t index) const;
    int tab_at(const TabLayout& layout, Point local) const;
    TabStyles styles() const;
    void draw_underlined(scene::Painter& painter, const TabLayout& layout, const TabStyles& styles);
    void draw_framed(scene::Painter& painter, const TabLayout& layout, const TabStyles& styles);
    void draw_compact(scene::Painter& painter, const TabLayout& layout, const TabStyles& styles);
    void draw_caption(scene::Painter& painter, const TabGeometry& tab, const TabStyles& styles);
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
    TabPresentation presentation_ = TabPresentation::Underlined;
    std::size_t active_index_ = 0;
    std::size_t first_visible_ = 0;
    ui::RoleId normal_role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
    ui::RoleId focused_role_ = ui::kInvalidRole;
    ui::RoleId separator_role_ = ui::kInvalidRole;
    ui::RoleId page_role_ = ui::kInvalidRole;
    ui::RoleId disabled_role_ = ui::kInvalidRole;
    ui::RoleId mnemonic_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
