// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/tool_bar.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/core/text.hpp"
#include "cvision/ui/application.hpp"
#include "widgets/common_internal.hpp"
#include "widgets/mnemonic_internal.hpp"
#include <algorithm>
#include <cstddef>
#include <utility>

namespace ckv::widgets {
using detail::is_press;
using detail::enclosing_desktop;
using detail::without_last_grapheme;

void BreadcrumbBar::set_segments(std::vector<std::string> segments) {
    ellipsis_pressed_ = false;
    cached_layout_.reset();
    segments_ = std::move(segments);
    focused_ = 0;
    ellipsis_focused_ = false;
    invalidate();
}
void BreadcrumbBar::set_separator(std::string separator) {
    ellipsis_pressed_ = false;
    cached_layout_.reset();
    separator_ = std::move(separator);
    invalidate();
}
void BreadcrumbBar::set_presentation(BreadcrumbPresentation presentation) {
    if (presentation_ == presentation) return;
    ellipsis_pressed_ = false;
    presentation_ = presentation;
    cached_layout_.reset();
    invalidate();
}
std::string_view BreadcrumbBar::separator_text() const noexcept {
    return presentation_ == BreadcrumbPresentation::Connected ? std::string_view("›") : std::string_view(separator_);
}
const BreadcrumbBar::Layout& BreadcrumbBar::layout() const {
    if (!cached_layout_) cached_layout_ = build_layout();
    return *cached_layout_;
}
std::optional<PointerShape> BreadcrumbBar::pointer_shape_at(Point local) const {
    if (local.y != 0 || local.x < 0 || local.x >= bounds().width || bounds().height <= 0) return std::nullopt;
    if (stop_at_x(layout(), local.x) < 0) return std::nullopt;
    return enabled_in_tree() ? PointerShape::Pointer : PointerShape::NotAllowed;
}

namespace {
// The stop that stands for the segments elision hides.
constexpr std::string_view kBreadcrumbEllipsis = "…";

// `text` in at most `columns` cells: whole when it fits, elided when a cell
// is left for the mark, and nothing at all when there is no room.
std::string fitted(const std::string& text, int columns) {
    if (columns <= 0) return {};
    return text::elide_to_width(text, columns);
}
}  // namespace

BreadcrumbBar::Layout BreadcrumbBar::build_layout() const {
    Layout bar;
    const std::size_t count = segments_.size();
    if (count == 0) return bar;
    const int width = std::max(0, bounds().width);
    const int separator = text::text_width(separator_text());
    const int padding = presentation_ == BreadcrumbPresentation::Plain ? 0 : 2;
    const auto segment_width = [this, padding](std::size_t index) { return text::text_width(segments_[index]) + padding; };
    const auto place = [&](std::vector<std::pair<std::optional<std::size_t>, std::string>> parts) {
        int x = 0;
        for (auto& [segment, text] : parts) {
            if (padding) text = " " + text + " ";
            bar.stops.push_back(Stop{segment, x, text});
            x += text::text_width(text) + separator;
        }
    };

    int whole = separator * static_cast<int>(count - 1);
    for (std::size_t i = 0; i < count; ++i) whole += segment_width(i);
    if (whole <= width) {
        std::vector<std::pair<std::optional<std::size_t>, std::string>> parts;
        for (std::size_t i = 0; i < count; ++i) parts.emplace_back(i, segments_[i]);
        place(std::move(parts));
        return bar;
    }

    const std::size_t last = count - 1;
    const int ellipsis = count > 2 ? text::text_width(kBreadcrumbEllipsis) + padding + separator : 0;
    // Keep the longest run of segments before the last that still fits
    // beside the first segment and the ellipsis: those nearest the last are
    // the ones the reader is most likely to go back to.
    if (count > 2) {
        int tail = segment_width(last);
        std::size_t kept_from = last;
        while (kept_from > 1) {
            const int wider = tail + segment_width(kept_from - 1) + separator;
            if (segment_width(0) + separator + ellipsis + wider > width) break;
            tail = wider;
            --kept_from;
        }
        if (segment_width(0) + separator + ellipsis + tail <= width) {
            std::vector<std::pair<std::optional<std::size_t>, std::string>> parts;
            parts.emplace_back(std::size_t{0}, segments_[0]);
            parts.emplace_back(std::nullopt, std::string(kBreadcrumbEllipsis));
            for (std::size_t i = kept_from; i < count; ++i) parts.emplace_back(i, segments_[i]);
            for (std::size_t i = 1; i < kept_from; ++i) bar.hidden.push_back(i);
            place(std::move(parts));
            return bar;
        }
    }

    // Not even the first segment, the ellipsis and the last fit: the two
    // segments share what room there is, the last -- where the reader is --
    // keeping the larger part of it.
    const int room = std::max(0, width - separator - ellipsis);
    const int last_room = count == 1 ? width
                                     : std::min(segment_width(last), std::max(room - segment_width(0), (room + 1) / 2));
    const int first_room = count == 1 ? 0 : std::min(segment_width(0), room - last_room);
    std::vector<std::pair<std::optional<std::size_t>, std::string>> parts;
    if (count > 1) parts.emplace_back(std::size_t{0}, fitted(segments_[0], std::max(0, first_room - padding)));
    if (count > 2) {
        parts.emplace_back(std::nullopt, std::string(kBreadcrumbEllipsis));
        for (std::size_t i = 1; i < last; ++i) bar.hidden.push_back(i);
    }
    parts.emplace_back(last, fitted(segments_[last], std::max(0, last_room - padding)));
    place(std::move(parts));
    return bar;
}

std::vector<std::size_t> BreadcrumbBar::hidden_segments() const { return layout().hidden; }

std::size_t BreadcrumbBar::focused_stop(const Layout& bar) const {
    std::optional<std::size_t> ellipsis;
    for (std::size_t i = 0; i < bar.stops.size(); ++i) {
        if (!bar.stops[i].segment) ellipsis = i;
        else if (!ellipsis_focused_ && *bar.stops[i].segment == focused_) return i;
    }
    // On the ellipsis, or on a segment elided into it.
    if (ellipsis) return *ellipsis;
    // On an ellipsis a wider bar no longer draws: the first segment it stood
    // for, which is the second stop now.
    return std::min<std::size_t>(ellipsis_focused_ ? 1 : 0, bar.stops.empty() ? 0 : bar.stops.size() - 1);
}

std::optional<std::size_t> BreadcrumbBar::focused_segment() const {
    const Layout& bar = layout();
    if (bar.stops.empty()) return std::nullopt;
    return bar.stops[focused_stop(bar)].segment;
}

void BreadcrumbBar::focus_stop(const Layout& bar, std::size_t stop) {
    if (const std::optional<std::size_t> segment = bar.stops[stop].segment) {
        focused_ = *segment;
        ellipsis_focused_ = false;
    } else {
        ellipsis_focused_ = true;
    }
    invalidate();
}

int BreadcrumbBar::stop_at_x(const Layout& bar, int x) const {
    for (std::size_t i = 0; i < bar.stops.size(); ++i) {
        const int start = bar.stops[i].x;
        if (x >= start && x < start + text::text_width(bar.stops[i].text)) return static_cast<int>(i);
    }
    return -1;
}

void BreadcrumbBar::activate_segment(std::size_t index) {
    focused_ = index;
    ellipsis_focused_ = false;
    invalidate();
    if (on_activate) on_activate(index);
}

bool BreadcrumbBar::activate_stop(const Layout& bar, std::size_t stop) {
    if (!on_activate) return false;
    if (const std::optional<std::size_t> segment = bar.stops[stop].segment) {
        activate_segment(*segment);
        return true;
    }
    // The ellipsis: the segments it stands for, as a menu hanging from it.
    Desktop* const desktop = enclosing_desktop(*this);
    if (desktop == nullptr || context().app == nullptr) return false;
    ellipsis_focused_ = true;
    invalidate();
    std::vector<MenuItem> items;
    const std::weak_ptr<void> liveness = lifetime_token();
    for (const std::size_t index : bar.hidden) {
        // A segment is plain text, so an '&' in it is a literal one.
        std::string label;
        for (const char c : segments_[index]) label += c == '&' ? std::string("&&") : std::string(1, c);
        items.push_back(MenuItem::action(std::move(label), [this, liveness, index] {
            if (!liveness.expired()) activate_segment(index);
        }));
    }
    const Rect abs = absolute_bounds();
    show_anchored_menu(std::move(items),
                       Rect{abs.x + bar.stops[stop].x, abs.y, text::text_width(bar.stops[stop].text), 1},
                       *context().app, *desktop);
    return true;
}

void BreadcrumbBar::draw(scene::Painter& target) {
    if (bounds().width <= 0 || bounds().height <= 0) return;
    auto painter = target.clipped(Rect{0, 0, bounds().width, 1});
    const bool enabled = enabled_in_tree();
    const Style normal = enabled ? context().theme->resolve(role_)
                                 : accent_style(context().theme->resolve(role_), context().theme->resolve(disabled_role_));
    const Style focused = context().theme->resolve(focused_role_);
    painter.fill(Rect{0, 0, bounds().width, 1}, Cell::from_grapheme(" ", normal));
    const Layout& bar = layout();
    if (bar.stops.empty()) return;
    const std::size_t focus = focused_stop(bar);
    for (std::size_t i = 0; i < bar.stops.size(); ++i) {
        const Stop& stop = bar.stops[i];
        if (stop.x >= bounds().width) break;
        Style style = enabled && has_focus() && i == focus ? focused : normal;
        if (enabled && stop.segment && *stop.segment + 1 == segments_.size()) style.attrs |= Attr::Bold;
        if (enabled && has_focus() && i == focus) style.attrs |= Attr::Underline;
        painter.draw_text(Point{stop.x, 0}, text::clip_to_width_view(stop.text, bounds().width - stop.x), style);
        if (i + 1 < bar.stops.size())
            painter.draw_text(Point{stop.x + text::text_width(stop.text), 0}, separator_text(), normal);
    }
}
bool BreadcrumbBar::on_key(const KeyEvent& event) {
    if (!enabled_in_tree() || !is_press(event) || segments_.empty()) return false;
    const Layout& bar = layout();
    const std::size_t focus = focused_stop(bar);
    if (event.chord.key == Key::Left && focus > 0) { focus_stop(bar, focus - 1); return true; }
    if (event.chord.key == Key::Right && focus + 1 < bar.stops.size()) { focus_stop(bar, focus + 1); return true; }
    if (event.chord.key == Key::Enter) return activate_stop(bar, focus);
    return false;
}
bool BreadcrumbBar::on_mouse(const MouseEvent& event) {
    if (!enabled_in_tree()) { ellipsis_pressed_ = false; return false; }
    const Rect abs = absolute_bounds();
    if (event.cell.y != abs.y || event.cell.x < abs.x || event.cell.x >= abs.x + abs.width || abs.height <= 0) {
        if (event.action == MouseAction::Up) ellipsis_pressed_ = false;
        return false;
    }
    if (event.button != MouseButton::Left && event.button != MouseButton::None) return false;
    const Layout& bar = layout();
    const int stop = stop_at_x(bar, event.cell.x - absolute_bounds().x);
    if (event.action == MouseAction::Down) {
        if (event.button != MouseButton::Left || stop < 0) return false;
        const auto index = static_cast<std::size_t>(stop);
        focus_stop(bar, index);
        // A segment acts at once. The ellipsis opens its menu when the click
        // completes: a menu put up on the way down would take the release
        // that follows as a click outside it and close again.
        if (bar.stops[index].segment) activate_stop(bar, index);
        else ellipsis_pressed_ = true;
        return true;
    }
    if (event.action == MouseAction::Up && ellipsis_pressed_) {
        ellipsis_pressed_ = false;
        if (stop >= 0 && !bar.stops[static_cast<std::size_t>(stop)].segment)
            activate_stop(bar, static_cast<std::size_t>(stop));
        return true;
    }
    return false;
}
void BreadcrumbBar::on_focus(const FocusEvent& event) { if (!event.gained) ellipsis_pressed_ = false; invalidate(); }
BreadcrumbBar::BreadcrumbBar() { set_focus_policy(ui::FocusPolicy::TabStop); }

void BreadcrumbBar::on_attached() {
    if (role_ == ui::kInvalidRole) role_ = context().roles->find("ckv.label.text");
    if (focused_role_ == ui::kInvalidRole) focused_role_ = context().roles->find("ckv.list.selected");
    if (disabled_role_ == ui::kInvalidRole) disabled_role_ = context().roles->find("ckv.label.disabled");
}

ui::SizeHint BreadcrumbBar::horizontal_size_hint() const {
    if (preferred_size().width > 0) return ui::View::horizontal_size_hint();
    if (segments_.empty()) return ui::SizeHint{0, 0, ui::kUnboundedExtent};
    int width = 0;
    const int sep_w = text::text_width(separator_text());
    const int pad = presentation_ == BreadcrumbPresentation::Padded ? 2 : 0;
    for (std::size_t i = 0; i < segments_.size(); ++i) {
        if (i > 0) width += sep_w;
        width += text::text_width(segments_[i]) + pad;
    }
    return ui::SizeHint{std::min(4, width), width, ui::kUnboundedExtent};
}

ui::SizeHint BreadcrumbBar::vertical_size_hint() const {
    if (preferred_size().height > 0) return ui::View::vertical_size_hint();
    return ui::SizeHint{1, 1, 1};
}



}  // namespace ckv::widgets
