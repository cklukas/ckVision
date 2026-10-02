// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tab_control.hpp"

#include <algorithm>
#include <string_view>

#include "cvision/core/ascii.hpp"
#include "cvision/core/text.hpp"
#include "cvision/widgets/mnemonic.hpp"
#include "cvision/widgets/mnemonic_internal.hpp"

namespace ckv::widgets {

namespace {
bool mnemonic_matches(const std::optional<std::string>& mnemonic, const std::string& text) {
    if (!mnemonic || mnemonic->empty() || text.empty()) return false;
    return ascii_lower((*mnemonic)[0]) == ascii_lower(text[0]);
}

// The marks that say captions are hidden off either end of the strip.
constexpr std::string_view kHiddenLeftMark = "◂";   // U+25C2
constexpr std::string_view kHiddenRightMark = "▸";  // U+25B8
}  // namespace

TabControl::TabControl() {
    set_focus_policy(ui::FocusPolicy::TabStop);
    set_preferred_size(Size{20, 6});
}

void TabControl::on_attached() {
    normal_role_ = context().roles->find("ckv.tab.normal");
    selected_role_ = context().roles->find("ckv.tab.selected");
    focused_role_ = context().roles->find("ckv.tab.focused");
    page_role_ = context().roles->find("ckv.tab.page");
    disabled_role_ = context().roles->find("ckv.tab.disabled");
    mnemonic_role_ = context().roles->find("ckv.tab.mnemonic");
    separator_role_ = context().roles->find("ckv.tab.separator");
}

void TabControl::set_presentation(TabPresentation presentation) {
    if (presentation_ == presentation) return;
    presentation_ = presentation;
    on_resized();
    invalidate();
    size_hint_changed();
}

ui::View* TabControl::add_tab(std::string label, std::unique_ptr<ui::View> page) {
    MnemonicText parsed = parse_mnemonic(label);
    ui::View* raw = add_child(std::move(page));
    if (raw == nullptr) return nullptr;
    tabs_.push_back(Tab{parsed.display, raw, parsed.mnemonic});
    captions_.push_back(std::move(parsed));
    if (tabs_.size() == 1) {
        active_index_ = 0;
    } else {
        raw->set_visible(false);
    }
    on_resized();
    invalidate();
    size_hint_changed();
    return raw;
}

ui::View* TabControl::active_page() const noexcept {
    if (tabs_.empty()) return nullptr;
    return tabs_[active_index_].page;
}

void TabControl::set_active_index(std::size_t index) {
    if (index >= tabs_.size() || index == active_index_) return;
    if (ui::View* old = active_page()) old->set_visible(false);
    active_index_ = index;
    if (ui::View* page = active_page()) {
        page->set_visible(true);
        page->set_bounds(page_bounds());
    }
    keep_active_visible();
    invalidate();
}

void TabControl::on_resized() {
    if (ui::View* page = active_page()) page->set_bounds(page_bounds());
    settle_strip();
}

Rect TabControl::page_bounds() const {
    const int width = std::max(0, bounds().width);
    const int height = std::max(0, bounds().height);
    switch (presentation_) {
        case TabPresentation::Underlined:
            return Rect{0, std::min(2, height), width, std::max(0, height - 2)};
        case TabPresentation::Framed:
            return Rect{std::min(1, width), std::min(3, height), std::max(0, width - 2),
                        std::max(0, height - 4)};
        case TabPresentation::Compact:
            return Rect{0, std::min(1, height), width, std::max(0, height - 1)};
    }
    return {};
}

int TabControl::caption_width(std::size_t index) const {
    return text::text_width(tabs_[index].label) + (presentation_ == TabPresentation::Framed ? 4 : 2);
}

TabControl::TabLayout TabControl::layout_from(std::size_t first) const {
    TabLayout layout;
    layout.page_bounds = page_bounds();
    const int width = std::max(0, bounds().width);
    if (tabs_.empty() || width == 0) return layout;
    int total = 0;
    for (std::size_t i = 0; i < tabs_.size(); ++i) total += caption_width(i) + (i > 0 ? 1 : 0);
    // Stable gutters whenever the complete strip overflows. For a one-cell
    // control there is no room for gutters: keep a clipped caption instead.
    const bool gutters = total > width && width >= 3;
    const int start = gutters ? 1 : 0;
    const int limit = gutters ? width - 1 : width;
    const bool framed = presentation_ == TabPresentation::Framed;
    const int label_y = framed ? 1 : 0;
    const int inset = framed ? 2 : 1;
    const int header_height = framed ? 3 : (presentation_ == TabPresentation::Underlined ? 2 : 1);
    int x = start;
    std::size_t index = first;
    for (; index < tabs_.size(); ++index) {
        const int desired = caption_width(index);
        if (index > first && x + desired > limit) break;
        const int shown = std::min(desired, limit - x);
        if (shown <= 0) break;
        layout.tabs.push_back(TabGeometry{
            index, Rect{x, 0, shown, std::min(header_height, std::max(0, bounds().height))},
            Rect{x + inset, label_y, std::max(0, shown - 2 * inset), 1}});
        x += desired + 1;
    }
    if (gutters && first > 0) layout.previous_scroll = Rect{0, label_y, 1, 1};
    if (gutters && index < tabs_.size()) layout.next_scroll = Rect{width - 1, label_y, 1, 1};
    return layout;
}

int TabControl::tab_at(const TabLayout& layout, Point local) const {
    for (const TabGeometry& tab : layout.tabs)
        if (tab.hit_bounds.contains(local)) return static_cast<int>(tab.index);
    return -1;
}

std::optional<PointerShape> TabControl::pointer_shape_at(Point local) const {
    if (!enabled_in_tree() || !Rect{0, 0, bounds().width, bounds().height}.contains(local)) return std::nullopt;
    const TabLayout layout = layout_from(first_visible_);
    if ((layout.previous_scroll && layout.previous_scroll->contains(local)) ||
        (layout.next_scroll && layout.next_scroll->contains(local)) || tab_at(layout, local) >= 0)
        return PointerShape::Pointer;
    return std::nullopt;
}

void TabControl::keep_active_visible() {
    if (tabs_.empty()) {
        first_visible_ = 0;
        return;
    }
    first_visible_ = std::min(first_visible_, active_index_);
    while (first_visible_ < active_index_ && (layout_from(first_visible_).tabs.empty() || layout_from(first_visible_).tabs.back().index < active_index_)) ++first_visible_;
}

void TabControl::settle_strip() {
    keep_active_visible();
    // Room gained on the right is given back to the captions hidden on the
    // left, one at a time, for as long as that hides none on the right.
    while (first_visible_ > 0) {
        const TabLayout previous = layout_from(first_visible_ - 1);
        const TabLayout current = layout_from(first_visible_);
        if (previous.tabs.empty() || current.tabs.empty() ||
            previous.tabs.back().index < current.tabs.back().index) break;
        --first_visible_;
    }
}

void TabControl::scroll_strip(int delta) {
    const TabLayout before = layout_from(first_visible_);
    if (delta < 0 && before.previous_scroll) {
        --first_visible_;
    } else if (delta > 0 && before.next_scroll) {
        ++first_visible_;
    } else {
        return;
    }
    // The active caption never leaves the strip: scrolled past, the
    // selection moves to the nearest caption still shown.
    const TabLayout after = layout_from(first_visible_);
    if (!after.tabs.empty()) {
        if (active_index_ < after.tabs.front().index) set_active_index(after.tabs.front().index);
        else if (active_index_ > after.tabs.back().index) set_active_index(after.tabs.back().index);
    }
    invalidate();
}

void TabControl::activate_delta(int delta) {
    if (tabs_.empty()) return;
    const int count = static_cast<int>(tabs_.size());
    const int next = (static_cast<int>(active_index_) + delta + count) % count;
    set_active_index(static_cast<std::size_t>(next));
}

ui::SizeHint TabControl::horizontal_size_hint() const {
    int width = 0;
    for (std::size_t i = 0; i < tabs_.size(); ++i) width += caption_width(i) + (i > 0 ? 1 : 0);
    return ui::SizeHint{4, std::max(20, width), ui::kUnboundedExtent};
}

ui::SizeHint TabControl::vertical_size_hint() const {
    const int chrome = presentation_ == TabPresentation::Framed ? 4 :
                       (presentation_ == TabPresentation::Underlined ? 2 : 1);
    return ui::SizeHint{chrome, 6, ui::kUnboundedExtent};
}

bool TabControl::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || event.action == KeyAction::Release) return false;
    if (event.chord.key == Key::Left) {
        activate_delta(-1);
        return true;
    }
    // Tab and Shift+Tab are not the strip's: they walk the focus through the
    // form, and a strip that took them could never be left from the keyboard.
    if (event.chord.key == Key::Right) {
        activate_delta(1);
        return true;
    }
    if (event.chord.key == Key::Char && has_modifier(event.chord.modifiers, Modifier::Alt) &&
        !has_modifier(event.chord.modifiers, Modifier::Ctrl) &&
        !has_modifier(event.chord.modifiers, Modifier::Super)) {
        for (std::size_t i = 0; i < tabs_.size(); ++i) {
            if (mnemonic_matches(tabs_[i].mnemonic, event.chord.text)) {
                set_active_index(i);
                return true;
            }
        }
    }
    return false;
}

bool TabControl::on_mouse(const MouseEvent& event) {
    if (event.action != MouseAction::Down || event.button != MouseButton::Left) return false;
    const Rect abs = absolute_bounds();
    const Point local{event.cell.x - abs.x, event.cell.y - abs.y};
    if (!enabled_in_tree() || !Rect{0, 0, bounds().width, bounds().height}.contains(local)) return false;
    const TabLayout layout = layout_from(first_visible_);
    if (layout.previous_scroll && layout.previous_scroll->contains(local)) {
        scroll_strip(-1);
        return true;
    }
    if (layout.next_scroll && layout.next_scroll->contains(local)) {
        scroll_strip(1);
        return true;
    }
    const int index = tab_at(layout, local);
    if (index < 0) return false;
    set_active_index(static_cast<std::size_t>(index));
    return true;
}

void TabControl::on_focus(const FocusEvent&) { invalidate(); }

TabControl::TabStyles TabControl::styles() const {
    const auto resolve = [&](ui::RoleId role) { return context().theme->resolve(role); };
    const bool enabled = enabled_in_tree();
    const Style disabled = resolve(disabled_role_);
    const auto shown = [&](Style style) { return enabled ? style : accent_style(style, disabled); };
    const Style selected = shown(resolve(selected_role_));
    return TabStyles{shown(resolve(normal_role_)), selected,
                     enabled ? resolve(focused_role_) : selected,
                     resolve(mnemonic_role_), shown(resolve(separator_role_)), resolve(page_role_)};
}

void TabControl::draw_caption(scene::Painter& painter, const TabGeometry& tab, const TabStyles& styles) {
    const bool selected = tab.index == active_index_;
    const Style face = selected ? styles.selected : styles.normal;
    Style label = selected && has_focus() && enabled_in_tree() ? styles.focused : face;
    if (selected && presentation_ == TabPresentation::Compact) label.attrs |= Attr::Bold;
    const Style accent = enabled_in_tree() ? accent_style(label, styles.mnemonic) : label;
    // Elision retains only an original complete-grapheme prefix. The
    // mnemonic helper clips its accent to that prefix; the marker never
    // acquires an accent belonging to an omitted mnemonic.
    const MnemonicText& original = captions_[tab.index];
    const int width = tab.label_bounds.width;
    if (text::text_width(original.display) <= width) {
        draw_mnemonic(painter, Point{tab.label_bounds.x, tab.label_bounds.y}, original, width, label, accent);
    } else {
        const std::string prefix = text::clip_to_width(original.display, std::max(0, width - 1));
        draw_mnemonic(painter, Point{tab.label_bounds.x, tab.label_bounds.y}, original,
                      text::text_width(prefix), label, accent);
        painter.draw_text(Point{tab.label_bounds.x + text::text_width(prefix), tab.label_bounds.y},
                          text::elide_to_width(original.display, width).substr(prefix.size()), label);
    }
}

void TabControl::draw_underlined(scene::Painter& painter, const TabLayout& layout, const TabStyles& styles) {
    painter.hline(Point{0, 1}, bounds().width, scene::LineStyle::Single, styles.separator);
    for (const TabGeometry& tab : layout.tabs) {
        const Style face = tab.index == active_index_ ? styles.selected : styles.normal;
        painter.fill(Rect{tab.hit_bounds.x, 0, tab.hit_bounds.width, 1}, Cell::from_grapheme(" ", face));
        draw_caption(painter, tab, styles);
        if (tab.index == active_index_)
            painter.hline(Point{tab.hit_bounds.x, 1}, tab.hit_bounds.width, scene::LineStyle::Heavy, styles.selected);
    }
}

void TabControl::draw_framed(scene::Painter& painter, const TabLayout& layout, const TabStyles& styles) {
    const int width = bounds().width;
    const int height = bounds().height;
    // The page sides, bottom, tab caps and split top rail are all lines in
    // this view's Painter scope, so connectors merge from geometry (D-036).
    if (height > 2) {
        painter.vline(Point{0, 2}, height - 2, scene::LineStyle::Single, styles.separator);
        if (width > 1) painter.vline(Point{width - 1, 2}, height - 2, scene::LineStyle::Single, styles.separator);
        if (height > 3) painter.hline(Point{0, height - 1}, width, scene::LineStyle::Single, styles.separator);
    }
    int rail_start = 0;
    for (const TabGeometry& tab : layout.tabs) {
        if (tab.index != active_index_ || tab.hit_bounds.width < 2) continue;
        const int left = tab.hit_bounds.x;
        const int right = left + tab.hit_bounds.width - 1;
        if (left > rail_start) painter.hline(Point{rail_start, 2}, left - rail_start + 1,
                                           scene::LineStyle::Single, styles.separator);
        rail_start = right;
    }
    // At the right edge the selected tab's side continues straight into
    // the page side. A one-cell hline would contribute false left/right
    // connectors (Painter's documented isolated-stub rule).
    if (rail_start == 0 || rail_start < width - 1)
        painter.hline(Point{rail_start, 2}, width - rail_start,
                                        scene::LineStyle::Single, styles.separator);
    for (const TabGeometry& tab : layout.tabs) {
        const Rect cap{tab.hit_bounds.x, 0, tab.hit_bounds.width, 3};
        const Style face = tab.index == active_index_ ? styles.selected : styles.normal;
        painter.fill(Rect{cap.x + 1, 1, std::max(0, cap.width - 2), 1}, Cell::from_grapheme(" ", face));
        if (cap.width >= 2) {
            if (tab.index == active_index_) {
                painter.hline(Point{cap.x, 0}, cap.width, scene::LineStyle::Single, styles.separator);
                painter.vline(Point{cap.x, 0}, 3, scene::LineStyle::Single, styles.separator);
                painter.vline(Point{cap.right() - 1, 0}, 3, scene::LineStyle::Single, styles.separator);
            } else {
                painter.draw_box(cap, scene::LineStyle::Single, styles.separator);
            }
        }
        draw_caption(painter, tab, styles);
    }
}

void TabControl::draw_compact(scene::Painter& painter, const TabLayout& layout, const TabStyles& styles) {
    for (const TabGeometry& tab : layout.tabs) {
        const bool selected = tab.index == active_index_;
        const Style face = selected ? styles.selected : styles.normal;
        painter.fill(tab.hit_bounds, Cell::from_grapheme(" ", face));
        draw_caption(painter, tab, styles);
    }
}

void TabControl::draw(scene::Painter& painter) {
    if (bounds().height <= 0 || bounds().width <= 0) return;
    auto clipped = painter.clipped(Rect{0, 0, bounds().width, bounds().height});
    const TabLayout layout = layout_from(first_visible_);
    const TabStyles resolved = styles();
    clipped.fill(Rect{0, 0, bounds().width, bounds().height}, Cell::from_grapheme(" ", resolved.normal));
    clipped.fill(layout.page_bounds, Cell::from_grapheme(" ", resolved.page));
    // The selected framed tab opens onto the page's surface.
    if (presentation_ == TabPresentation::Framed && bounds().height > 2)
        clipped.fill(Rect{0, 2, bounds().width, bounds().height - 2}, Cell::from_grapheme(" ", resolved.page));
    switch (presentation_) {
        case TabPresentation::Underlined: draw_underlined(clipped, layout, resolved); break;
        case TabPresentation::Framed: draw_framed(clipped, layout, resolved); break;
        case TabPresentation::Compact: draw_compact(clipped, layout, resolved); break;
    }
    if (layout.previous_scroll) clipped.draw_text(Point{layout.previous_scroll->x, layout.previous_scroll->y},
                                                kHiddenLeftMark, resolved.normal);
    if (layout.next_scroll) clipped.draw_text(Point{layout.next_scroll->x, layout.next_scroll->y},
                                            kHiddenRightMark, resolved.normal);
}

}  // namespace ckv::widgets
