// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// PagedStrip (U4-l): one row of variable-width items that PAGES when they do
// not all fit, plus the chrome that steers it — a collapse toggle at the far
// left, a previous/next page control, and a page index.
//
// It exists because the paging is not a property of any one bar. A window
// switcher pages its windows; a status line pages its command items; a
// session manager pages its sessions. Each of those differs only in what an
// item IS, so that is the single thing the strip does not know: an item
// source hands it a width and the text to draw, and everything else — where
// the items fall, which of them are on this page, which controls are worth
// drawing, and what a click on any cell means — belongs here. The same
// reasoning that put WindowSwitcherBar in this library rather than in its
// first application puts the paging here rather than in the bar.
//
// Left to right, at a width that needs paging:
//
//     ▼ ◁ 2/3  Item  Another item  Third   ▷
//     │ │  │   └── the items, as many as fit WHOLE
//     │ │  └────── the page index, only while there is more than one page
//     │ └───────── the previous-page control, live only off the first page
//     └─────────── the collapse toggle, only when the host asked for one
//
// **The strip collapses nothing.** The toggle reports itself through
// `on_collapse_changed` and answers `collapsed()`; what collapsing MEANS is
// the host's other chrome — a footer, a second docked row — and a widget
// that reached out to hide a sibling would be deciding something that is not
// its own. The strip's own geometry never changes with the flag.
//
// Elision versus paging. An item takes its natural width and overflows onto
// the next page; nothing is elided to make room, because a name shortened to
// "Te…" in a row that had another page available is a loss with no reason.
// Elision survives in exactly ONE case: an item whose box is wider than the
// entire item area is alone on its page and is elided to that area. It has
// nowhere else to go, and the alternative is a blank page or an item that
// cannot be reached at all.
//
// Chrome is reserved for every page alike once there is more than one page,
// including the previous-page cell on the first page, where it is drawn
// blank. Items therefore keep their columns as the reader pages, and — more
// importantly — which items land on which page is a pure function of the
// width and the item widths, so paging forward and back returns the reader
// to the set they came from.
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "cvision/ui/theme.hpp"
#include "cvision/ui/view.hpp"

namespace ckv::widgets {

// Resolves its own theme roles from context() once attached (D-028):
// "ckv.statusline.normal" for the strip, its chrome and its ordinary items,
// "ckv.statusline.selected" for a selected or held one. The status line's own
// roles rather than a family of its own, for the reason WindowSwitcherBar
// gives: this is a row of clickable words docked against a screen edge, and a
// second family would let two halves of one docked stack disagree about what
// a bar looks like in a theme that retinted only one of them.
//
// Keyboard: none. A strip is chrome, and its items are reachable by whatever
// commands the host already binds to them; a focus stop here would put the
// reader's Tab in the furniture rather than in their work.
class PagedStrip : public ui::View {
public:
    // One blank cell either side of an item's content. It belongs to the
    // item — a selected item colours its padding too, which is what makes the
    // highlight read as one row rather than as recoloured words — and it
    // accepts the click, exactly as its appearance promises.
    static constexpr int kItemPadding = 1;
    // The blank cell between two items. Nothing is drawn there: the gap IS the
    // separation, the same answer StatusLine gives to the same question.
    static constexpr int kItemGap = 1;

    // One item, as the strip understands it.
    struct Item {
        // The cells this item's own content needs — its text, and any icon it
        // means to draw before it. The provider's answer, never a measurement
        // the strip takes of `text`: an item that carries a leading glyph says
        // so here instead of leaving the layout to guess. The strip adds its
        // own kItemPadding either side.
        int width = 0;
        // What the item draws inside its content cells.
        std::string text;
        // Drawn in the selected role rather than the normal one.
        bool selected = false;

        friend bool operator==(const Item&, const Item&) = default;
    };

    // One item as it is actually laid out at the current width — exposed so a
    // caller can read the columns an item occupies, and the text it will show,
    // without scraping rendered cells.
    struct Placement {
        std::size_t index = 0;  // into items()
        int x = 0;              // local column of the item's first cell
        int width = 0;          // cells the item occupies, padding included
        std::string text;       // the text as it will be drawn, elided only in the lone-item case
    };

    // Where the strip's own furniture falls at the current width. A column of
    // -1 is furniture the strip is not drawing at all. `previous_x` and
    // `next_x` are RESERVED for every page once there is more than one, so
    // they are >= 0 even where the control itself is blank — ask
    // shows_previous_control()/shows_next_control() for whether a glyph is
    // there to click.
    struct Chrome {
        int collapse_x = -1;
        int previous_x = -1;
        int index_x = -1;
        int index_width = 0;
        int next_x = -1;
        int items_x = 0;
        int items_width = 0;
    };

    // What a cell belongs to. Only the controls that can do something are
    // reported: the previous-page cell on the first page, the page index, and
    // the blank run past the last item are all None, because a press there
    // would do nothing and a hit that promised otherwise would be a lie the
    // pointer shape then repeats.
    enum class Region { None, CollapseToggle, PreviousPage, NextPage, Item };

    struct Hit {
        Region region = Region::None;
        std::size_t item = 0;  // meaningful only for Region::Item
    };

    PagedStrip();

    // --- what the strip is made of ------------------------------------
    // Lists nothing until an item source is installed. One provider rather
    // than a count and a width provider, because layout needs both together
    // and two answers can disagree between the calls.
    void set_item_source(std::function<std::vector<Item>()> source);

    // Re-reads the item source, re-pages, and revalidates the current page.
    // Called by set_item_source and on attach; a host whose item model moves
    // calls it then. Repaints only if the answer actually changed.
    void refresh_items();

    const std::vector<Item>& items() const noexcept { return items_; }

    // --- paging -------------------------------------------------------
    std::size_t page_count() const noexcept { return page_starts_.size(); }
    std::size_t page() const noexcept { return page_; }
    // Moves to `page` if it exists. Returns whether the current page changed.
    bool set_page(std::size_t page);
    bool next_page();
    bool previous_page();
    // "2/3", or empty while there is at most one page: an index that always
    // reads 1/1 is a column spent saying nothing.
    std::string page_index_text() const;
    // Whether a control is drawn AND live. The previous control is live off
    // the first page, the next control while items remain after this one.
    bool shows_previous_control() const noexcept;
    bool shows_next_control() const noexcept;
    // Fires whenever the current page changes for any reason — a control, a
    // set_page call, or a revalidation that had to fall back.
    std::function<void(std::size_t)> on_page_changed;

    // --- the collapse toggle ------------------------------------------
    // Off by default: a strip whose host has nothing to collapse must not
    // spend a column saying so.
    void set_collapsible(bool collapsible);
    bool collapsible() const noexcept { return collapsible_; }
    // The state the toggle reports. The strip draws ▼ while expanded and ▲
    // while collapsed and does nothing else with it whatsoever.
    void set_collapsed(bool collapsed);
    bool collapsed() const noexcept { return collapsed_; }
    // Fires whenever the flag changes for any reason, the toggle and a direct
    // set_collapsed call alike — one place for a host to hide its footer.
    std::function<void(bool)> on_collapse_changed;

    // --- what a click on an item does ---------------------------------
    // A completed left click: pressed and released on the same item. The
    // press alone only shows itself, and a press dragged off its item and
    // released takes the click back.
    std::function<void(std::size_t)> on_item_activated;
    // A right press on an item, at that absolute cell. Returns whether the
    // host consumed it — a host with nothing to offer leaves the press
    // unclaimed rather than swallowing it, so an application's own
    // desktop-wide context menu still gets a chance at it.
    std::function<bool(std::size_t, Point)> on_item_context_press;

    // --- geometry -----------------------------------------------------
    const Chrome& chrome() const noexcept { return chrome_; }
    // How the current page's items fall at the current width.
    std::vector<Placement> placed_items() const;
    // The item under an ABSOLUTE cell (a MouseEvent's own coordinates).
    std::optional<std::size_t> item_at(Point cell) const;
    Hit hit_test(Point cell) const;

    void set_role_override(ui::RoleId normal_role, ui::RoleId selected_role) noexcept {
        role_ = normal_role;
        selected_role_ = selected_role;
    }

    ui::SizeHint horizontal_size_hint() const override;
    ui::SizeHint vertical_size_hint() const override;

    void draw(scene::Painter& painter) override;
    bool on_mouse(const MouseEvent& event) override;
    std::optional<PointerShape> pointer_shape_at(Point local) const override;
    void on_attached() override;
    void on_resized() override;

protected:
    // The style one item is drawn in, so a consumer can read the same answer
    // the strip draws with. `selected` covers both the item's own flag and a
    // press currently held on it: a taskbar row held down and a taskbar row
    // already in front are the same promise.
    bool item_is_selected(std::size_t index) const noexcept;

    // Whether the pointer is currently holding an item down — true from the
    // press until the release, whether or not the pointer has since been
    // dragged off the item it claimed.
    //
    // For a subclass that changes its own item widths on a TIMER rather than
    // in answer to an event. This strip resolves a click by INDEX: the press
    // remembers `pressed_item_`, and the release fires for whatever item now
    // sits at that index. Relaying out under a held pointer therefore either
    // moves the item out from under it, so the release misses and the click
    // silently does nothing, or shifts the columns so the release names an
    // item the reader never pointed at. A reader mid-click is exactly when
    // the target must not move, so a subclass that would resize an item while
    // this is true should defer it and re-run on the release.
    bool press_in_flight() const noexcept { return pressed_item_.has_value(); }

private:
    // Splits the items into pages, greedily, given the cells items may use.
    // The first item of a page is always taken, even where it does not fit —
    // that is the lone-oversized-item case, and the alternative is a page
    // that holds nothing.
    std::vector<std::size_t> pack(int area_width) const;
    // Recomputes the chrome band and the page starts, then revalidates the
    // current page. See the .cpp for the order the chrome is given up in when
    // the strip is too narrow to carry all of it.
    void relayout();
    bool try_paged_layout(bool with_toggle, bool with_index);
    // Where an item's text starts and how many cells it gets, given its box.
    // An item too narrow to carry its padding spends every cell on text: at
    // that width a blank cell says nothing at all, and an elision marker at
    // least says an item is here.
    static int text_column(int x, int box) noexcept;
    static int text_columns(int box) noexcept;
    std::size_t page_end(std::size_t page) const noexcept;

    std::function<std::vector<Item>()> item_source_;
    std::vector<Item> items_;

    // The first item index of each page. Empty means nothing to show.
    std::vector<std::size_t> page_starts_;
    std::size_t page_ = 0;
    Chrome chrome_;

    bool collapsible_ = false;
    bool collapsed_ = false;

    // The item currently held down by the pointer, and whether the pointer is
    // still on it — a press dragged away un-highlights but stays claimed, so
    // returning to the item re-arms it.
    std::optional<std::size_t> pressed_item_;
    bool pressed_visible_ = true;

    ui::RoleId role_ = ui::kInvalidRole;
    ui::RoleId selected_role_ = ui::kInvalidRole;
};

}  // namespace ckv::widgets
