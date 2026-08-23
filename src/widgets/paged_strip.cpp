// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/paged_strip.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "cvision/core/text.hpp"

namespace ckv::widgets {

namespace {

// The steering glyphs, all four checked against ckv::text — this library's
// own width authority — before being used: U+25BC, U+25B2, U+25C1 and U+25B7
// are East Asian Ambiguous, which D-019 resolves to ONE column, and none of
// them is Extended_Pictographic, so no variation selector can turn one into a
// two-cell emoji. A double-width triangle would shear the row it is steering,
// so the check is the reason these four are the glyphs and the BLACK
// left/right triangles U+25C0/U+25B6 are not: those two ARE
// Extended_Pictographic and terminals do widen them.
constexpr std::string_view kExpandedGlyph = "▼";   // U+25BC, the host's chrome is showing
constexpr std::string_view kCollapsedGlyph = "▲";  // U+25B2, the host's chrome is away
constexpr std::string_view kPreviousGlyph = "◁";   // U+25C1
constexpr std::string_view kNextGlyph = "▷";       // U+25B7

// Cells the previous-page control and the page index need, and the blank that
// separates the left chrome band from the items.
constexpr int kControlWidth = 1;
constexpr int kChromeGap = 1;
// The right edge: one blank, then the next-page control.
constexpr int kRightChromeWidth = kChromeGap + kControlWidth;
// The collapse toggle and the blank after it.
constexpr int kToggleBandWidth = kControlWidth + kChromeGap;
// The narrowest item area worth reserving. One cell still carries an elision
// marker, which still says an item is here — and keeping the page controls
// alive matters more than a legible name, because a name the reader cannot
// page to is not legible either.
constexpr int kMinItemArea = 1;

int digits(std::size_t value) noexcept {
    int count = 1;
    while (value >= 10) {
        value /= 10;
        ++count;
    }
    return count;
}

}  // namespace

PagedStrip::PagedStrip() = default;

void PagedStrip::set_item_source(std::function<std::vector<Item>()> source) {
    item_source_ = std::move(source);
    refresh_items();
}

void PagedStrip::refresh_items() {
    std::vector<Item> rebuilt;
    if (item_source_) rebuilt = item_source_();
    // Repaint only on a real change. A host that re-reads its model on a timer
    // would otherwise leave this row permanently dirty; and since the page
    // layout is a function of the widths and the count alone, an unchanged
    // item set cannot have changed the pages either.
    if (rebuilt == items_) return;
    items_ = std::move(rebuilt);
    // A press whose item has gone is a press on nothing.
    if (pressed_item_.has_value() && *pressed_item_ >= items_.size()) {
        pressed_item_.reset();
        pressed_visible_ = true;
    }
    relayout();
    invalidate();
}

// --- paging ---------------------------------------------------------------

bool PagedStrip::set_page(std::size_t page) {
    if (page >= page_starts_.size() || page == page_) return false;
    page_ = page;
    invalidate();
    if (on_page_changed) on_page_changed(page_);
    return true;
}

bool PagedStrip::next_page() { return set_page(page_ + 1); }

bool PagedStrip::previous_page() { return page_ > 0 && set_page(page_ - 1); }

std::string PagedStrip::page_index_text() const {
    if (page_starts_.size() <= 1) return {};
    return std::to_string(page_ + 1) + "/" + std::to_string(page_starts_.size());
}

bool PagedStrip::shows_previous_control() const noexcept {
    return chrome_.previous_x >= 0 && page_ > 0;
}

bool PagedStrip::shows_next_control() const noexcept {
    return chrome_.next_x >= 0 && page_ + 1 < page_starts_.size();
}

// --- the collapse toggle --------------------------------------------------

void PagedStrip::set_collapsible(bool collapsible) {
    if (collapsible_ == collapsible) return;
    collapsible_ = collapsible;
    // The toggle costs a column, so the items re-page around it.
    relayout();
    invalidate();
}

void PagedStrip::set_collapsed(bool collapsed) {
    if (collapsed_ == collapsed) return;
    collapsed_ = collapsed;
    // The glyph changes and nothing else does: what collapsing MEANS is the
    // host's, and it hears about it here.
    invalidate();
    if (on_collapse_changed) on_collapse_changed(collapsed_);
}

// --- layout ---------------------------------------------------------------

std::vector<std::size_t> PagedStrip::pack(int area_width) const {
    std::vector<std::size_t> starts;
    if (items_.empty() || area_width < kMinItemArea) return starts;
    const auto box_of = [this](std::size_t index) {
        return std::max(0, items_[index].width) + 2 * kItemPadding;
    };
    std::size_t index = 0;
    while (index < items_.size()) {
        starts.push_back(index);
        // The first item of a page is always taken, even where its box is
        // wider than the whole area: a page that held nothing would be a page
        // the reader can reach and learn nothing from, and the item would then
        // have no page at all.
        int used = box_of(index);
        ++index;
        while (index < items_.size() && used + kItemGap + box_of(index) <= area_width) {
            used += kItemGap + box_of(index);
            ++index;
        }
    }
    return starts;
}

bool PagedStrip::try_paged_layout(bool with_toggle, bool with_index) {
    const int width = bounds().width;
    const int toggle_band = with_toggle ? kToggleBandWidth : 0;
    // The index is as wide as "N/N" for the page COUNT's digits, on every
    // page, so the band does not breathe as the reader walks from page 9 to
    // page 10. Its width feeds back into the page count, so this settles at a
    // fixed point: reserving more can only produce more pages, more pages can
    // only produce more digits, and the digit count is bounded by the item
    // count — a handful of turns at the very most.
    int index_digits = 1;
    for (int guard = 0; guard < 8; ++guard) {
        const int index_width = with_index ? 2 * index_digits + 1 : 0;
        const int items_x = toggle_band + kControlWidth + index_width + kChromeGap;
        const int items_width = width - items_x - kRightChromeWidth;
        if (items_width < kMinItemArea) return false;
        std::vector<std::size_t> starts = pack(items_width);
        const int settled = digits(starts.size());
        if (with_index && settled != index_digits) {
            index_digits = settled;
            continue;
        }
        Chrome chrome;
        chrome.collapse_x = with_toggle ? 0 : -1;
        chrome.previous_x = toggle_band;
        chrome.index_x = with_index ? toggle_band + kControlWidth : -1;
        chrome.index_width = index_width;
        chrome.next_x = width - 1;
        chrome.items_x = items_x;
        chrome.items_width = items_width;
        chrome_ = chrome;
        page_starts_ = std::move(starts);
        return true;
    }
    return false;
}

void PagedStrip::relayout() {
    const std::size_t was = page_;
    const int width = bounds().width;

    chrome_ = Chrome{};
    page_starts_.clear();
    const int toggle_band = collapsible_ ? std::min(kToggleBandWidth, std::max(0, width)) : 0;
    if (collapsible_ && width > 0) chrome_.collapse_x = 0;
    chrome_.items_x = toggle_band;
    chrome_.items_width = std::max(0, width - toggle_band);

    if (width > 0 && !items_.empty()) {
        std::vector<std::size_t> unpaged = pack(chrome_.items_width);
        if (unpaged.size() <= 1) {
            // Everything fits: no controls, no index, and the row is laid out
            // exactly as it was before this widget knew how to page.
            page_starts_ = std::move(unpaged);
        } else if (!try_paged_layout(collapsible_, true) &&
                   !try_paged_layout(collapsible_, false) && !try_paged_layout(false, false)) {
            // The chrome is given up in order of what it costs the reader:
            // the page index first, because it is information and the controls
            // are function; then the collapse toggle, which steers the host's
            // furniture rather than the reader's items; and only then the page
            // controls. Past that the strip is narrower than one control and
            // one item cell, and it keeps its pages — page_count() and
            // next_page() are still honest — but cannot be steered by pointer.
            page_starts_ = std::move(unpaged);
        }
    }

    // Revalidation. The item set moves under the strip — a window opens, a
    // session ends — so the pages are recomputed above on every change, and a
    // current page that no longer exists falls back to the last one that does
    // rather than leaving a blank row under a 3/2 index. A page that survives
    // is kept: removing an item from an EARLIER page must not carry the reader
    // somewhere they did not ask to go.
    if (page_ >= page_starts_.size()) page_ = page_starts_.empty() ? 0 : page_starts_.size() - 1;
    if (page_ != was && on_page_changed) on_page_changed(page_);
}

int PagedStrip::text_columns(int box) noexcept {
    return box >= 2 * kItemPadding + 1 ? box - 2 * kItemPadding : box;
}

int PagedStrip::text_column(int x, int box) noexcept {
    return box >= 2 * kItemPadding + 1 ? x + kItemPadding : x;
}

std::size_t PagedStrip::page_end(std::size_t page) const noexcept {
    if (page + 1 < page_starts_.size()) return page_starts_[page + 1];
    return items_.size();
}

std::vector<PagedStrip::Placement> PagedStrip::placed_items() const {
    if (page_ >= page_starts_.size()) return {};
    const std::size_t first = page_starts_[page_];
    const std::size_t last = page_end(page_);
    const int limit = chrome_.items_x + chrome_.items_width;

    std::vector<Placement> laid_out;
    laid_out.reserve(last - first);
    int x = chrome_.items_x;
    for (std::size_t index = first; index < last; ++index) {
        int box = std::max(0, items_[index].width) + 2 * kItemPadding;
        // Only ever the lone item on a page of its own, by construction of
        // pack(): everything else was chosen because it fits whole. This is
        // the one place elision survives paging.
        if (x + box > limit) box = limit - x;
        if (box <= 0) break;
        laid_out.push_back(Placement{index, x, box,
                                     text::elide_to_width(items_[index].text, text_columns(box))});
        x += box + kItemGap;
    }
    return laid_out;
}

// --- hit testing ----------------------------------------------------------

PagedStrip::Hit PagedStrip::hit_test(Point cell) const {
    const Rect abs = absolute_bounds();
    if (cell.y != abs.y) return {};
    const int x = cell.x - abs.x;
    if (x < 0 || x >= bounds().width) return {};
    if (chrome_.collapse_x >= 0 && x == chrome_.collapse_x) return Hit{Region::CollapseToggle, 0};
    if (shows_previous_control() && x == chrome_.previous_x) return Hit{Region::PreviousPage, 0};
    if (shows_next_control() && x == chrome_.next_x) return Hit{Region::NextPage, 0};
    for (const Placement& placed : placed_items())
        if (x >= placed.x && x < placed.x + placed.width) return Hit{Region::Item, placed.index};
    return {};
}

std::optional<std::size_t> PagedStrip::item_at(Point cell) const {
    const Hit hit = hit_test(cell);
    if (hit.region != Region::Item) return std::nullopt;
    return hit.item;
}

bool PagedStrip::item_is_selected(std::size_t index) const noexcept {
    if (index >= items_.size()) return false;
    if (items_[index].selected) return true;
    return pressed_item_.has_value() && *pressed_item_ == index && pressed_visible_;
}

// --- view ------------------------------------------------------------------

ui::SizeHint PagedStrip::horizontal_size_hint() const {
    return ui::SizeHint{0, 0, ui::kUnboundedExtent};
}

// Exactly one row, never more and never fewer: a strip is a strip.
ui::SizeHint PagedStrip::vertical_size_hint() const { return ui::SizeHint{1, 1, 1}; }

void PagedStrip::draw(scene::Painter& painter) {
    const Style normal = context().theme->resolve(role_);
    const Style selected = context().theme->resolve(selected_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));

    if (chrome_.collapse_x >= 0)
        painter.draw_text(Point{chrome_.collapse_x, 0},
                          collapsed_ ? kCollapsedGlyph : kExpandedGlyph, normal);
    // A control whose press would do nothing leaves its reserved cell blank.
    // The cell stays reserved so the items do not shuffle sideways as the
    // reader pages.
    if (shows_previous_control())
        painter.draw_text(Point{chrome_.previous_x, 0}, kPreviousGlyph, normal);
    if (shows_next_control()) painter.draw_text(Point{chrome_.next_x, 0}, kNextGlyph, normal);
    if (chrome_.index_x >= 0) {
        const std::string index = page_index_text();
        // Right-aligned in its band, so the total stays in one column while
        // the current page's own digits grow beneath it.
        const int pad = std::max(0, chrome_.index_width - text::text_width(index));
        painter.draw_text(Point{chrome_.index_x + pad, 0}, index, normal);
    }

    for (const Placement& placed : placed_items()) {
        const Style style = item_is_selected(placed.index) ? selected : normal;
        painter.fill(Rect{placed.x, 0, placed.width, 1}, Cell::from_grapheme(" ", style));
        if (placed.text.empty()) continue;
        const int column = text_column(placed.x, placed.width);
        // An item's leading mark, where it asked for one, is the only part of
        // a row drawn in a style that is not the row's — see Item::icon_role.
        // It is taken off the front of the text AS IT WILL BE DRAWN rather
        // than off the source text, so a lone over-wide item that lost its
        // tail to elision keeps its mark, and one clipped so hard that the
        // mark itself is gone has nothing left to recolour and says so by
        // handing back an empty prefix.
        const Item& item = items_[placed.index];
        const std::string mark = item.icon_width > 0 && item.icon_role != ui::kInvalidRole
                                     ? text::clip_to_width(placed.text, item.icon_width)
                                     : std::string();
        if (!mark.empty()) {
            const Style mark_role_style = context().theme->resolve(item.icon_role);
            // A borrowed foreground can land on a background of its own
            // colour: the classic theme gives the window control the green
            // the status line selects with, and the mono theme gives it the
            // foreground the status line inverts to. A mark drawn in the
            // colour it stands on is not a mark, it is a gap in the name —
            // so where the two agree the row's own style wins. Nothing is
            // lost that the reader needs: the highlight under that row is
            // already saying which one it is, and a legible mark in the
            // row's colour beats an invisible one in the right colour.
            const Style mark_style =
                mark_role_style.fg == style.bg
                    ? style
                    : Style{mark_role_style.fg, style.bg, style.attrs | mark_role_style.attrs};
            painter.draw_text(Point{column, 0}, mark, mark_style);
            painter.draw_text(Point{column + text::text_width(mark), 0},
                              placed.text.substr(mark.size()), style);
            continue;
        }
        painter.draw_text(Point{column, 0}, placed.text, style);
    }
}

bool PagedStrip::on_mouse(const MouseEvent& event) {
    const Hit hit = hit_test(event.cell);

    if (event.button == MouseButton::Right) {
        if (event.action != MouseAction::Down || hit.region != Region::Item) return false;
        if (!on_item_context_press) return false;
        return on_item_context_press(hit.item, event.cell);
    }

    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left) return false;
        switch (hit.region) {
        case Region::CollapseToggle:
            // The controls act on the press, the way a scrollbar's arrows do:
            // they steer, and steering is cheap to undo. An item acts on the
            // release, because what it does is the host's and may not be.
            set_collapsed(!collapsed_);
            return true;
        case Region::PreviousPage:
            previous_page();
            return true;
        case Region::NextPage:
            next_page();
            return true;
        case Region::Item:
            // Show the press before acting on it: a row that acts with no
            // visible acknowledgement leaves the reader unsure it was hit.
            pressed_item_ = hit.item;
            pressed_visible_ = true;
            invalidate();
            return true;
        case Region::None:
            return false;
        }
        return false;
    }

    if (event.action == MouseAction::Move) {
        if (!pressed_item_.has_value()) return false;
        const bool over = hit.region == Region::Item && hit.item == *pressed_item_;
        if (over != pressed_visible_) {
            pressed_visible_ = over;
            invalidate();
        }
        return true;
    }

    if (event.action == MouseAction::Up) {
        const std::optional<std::size_t> pressed = pressed_item_;
        pressed_item_.reset();
        pressed_visible_ = true;
        invalidate();
        // Releasing away from the item it started on takes the press back.
        if (!pressed.has_value()) return false;
        const bool over = hit.region == Region::Item && hit.item == *pressed;
        if (over && on_item_activated) on_item_activated(*pressed);
        return true;
    }
    return false;
}

std::optional<PointerShape> PagedStrip::pointer_shape_at(Point local) const {
    const Rect abs = absolute_bounds();
    // Only over something a press would act on: the blank run past the last
    // item, the page index and a dead control are background, and a pointer
    // that promised otherwise would promise what pressing does not do.
    if (hit_test(Point{abs.x + local.x, abs.y + local.y}).region == Region::None)
        return std::nullopt;
    return PointerShape::Pointer;
}

void PagedStrip::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.statusline.normal");
    if (selected_role_ == ui::kInvalidRole)
        selected_role_ = context().roles->find("ckv.statusline.selected");
    refresh_items();
}

void PagedStrip::on_resized() {
    // A narrower strip is a differently paged strip. set_bounds has already
    // damaged the row, so nothing here invalidates a second time.
    relayout();
}

}  // namespace ckv::widgets
