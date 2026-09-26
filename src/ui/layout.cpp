// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/layout.hpp"

#include <algorithm>
#include <limits>

#include "cvision/core/assert.hpp"
#include "cvision/ui/layout_metrics.hpp"

namespace ckv::ui {

namespace {

// How far an Expanding child may still grow past what it has been assigned:
// up to its max, read as no less than its preferred extent; unbounded when
// it has none.
long long growth_room(const LayoutChild& child, int assigned) {
    if (child.max == kUnboundedExtent) return std::numeric_limits<long long>::max();
    return std::max<long long>(0, static_cast<long long>(std::max(child.max, child.preferred)) - assigned);
}

// Hands `extra` cells to the Expanding children in proportion to weight (an
// even split when every weight is 0 — a legal but degenerate LayoutSpec, and
// "claims a share of any leftover space" is Expanding's whole contract
// regardless of weight). The integer-division remainder goes to the last
// child still sharing, so the container is filled exactly. A child is never
// grown past its max: what it cannot take is shared again among the others,
// and what none can take is left empty after the last child. Each round
// either hands out everything or retires at least one child that reached its
// max, so there are at most as many rounds as children — a bounded pass, not
// an iterative solve.
void share_leftover(const std::vector<LayoutChild>& children, std::vector<int>& assigned, int extra) {
    std::vector<std::size_t> sharing;
    for (std::size_t i = 0; i < children.size(); ++i)
        if (children[i].policy == SizePolicy::Expanding && growth_room(children[i], assigned[i]) > 0)
            sharing.push_back(i);

    while (extra > 0 && !sharing.empty()) {
        long long weight_total = 0;
        for (std::size_t i : sharing) weight_total += children[i].weight;
        const bool split_evenly = weight_total == 0;

        int handed = 0;
        for (std::size_t i : sharing) {
            const long long share = split_evenly ? extra / static_cast<long long>(sharing.size())
                                                 : static_cast<long long>(extra) * children[i].weight / weight_total;
            const int grown = static_cast<int>(std::min(share, growth_room(children[i], assigned[i])));
            assigned[i] += grown;
            handed += grown;
        }
        const std::size_t last = sharing.back();
        const int remainder = static_cast<int>(
            std::min<long long>(extra - handed, growth_room(children[last], assigned[last])));
        assigned[last] += remainder;
        handed += remainder;
        extra -= handed;

        std::erase_if(sharing, [&](std::size_t i) { return growth_room(children[i], assigned[i]) == 0; });
        if (handed == 0) break;  // every share rounded to nothing and the last child is full
    }
}

}  // namespace

std::vector<std::pair<int, int>> distribute_main_axis(const std::vector<LayoutChild>& children,
                                                        int available, int spacing) {
    std::vector<std::pair<int, int>> sizes(children.size(), {0, 0});
    if (children.empty()) return sizes;

    const int spacing_total = spacing * static_cast<int>(children.size() - 1);
    const int usable = std::max(0, available - spacing_total);

    int fixed_total = 0;
    int flexible_preferred_total = 0;
    int expanding_count = 0;
    for (const auto& c : children) {
        if (c.policy == SizePolicy::Fixed) {
            fixed_total += c.preferred;
        } else {
            flexible_preferred_total += c.preferred;
            if (c.policy == SizePolicy::Expanding) ++expanding_count;
        }
    }

    const int remaining_for_flexible = usable - fixed_total;

    std::vector<int> assigned(children.size(), 0);
    for (std::size_t i = 0; i < children.size(); ++i) assigned[i] = children[i].preferred;

    if (remaining_for_flexible >= flexible_preferred_total) {
        // Slack to hand out: only Expanding children claim a share,
        // proportional to weight. Minimum children stay at preferred.
        const int extra = remaining_for_flexible - flexible_preferred_total;
        if (expanding_count > 0 && extra > 0) share_leftover(children, assigned, extra);
    } else if (flexible_preferred_total > 0) {
        // Not enough room: shrink flexible children toward their min,
        // proportional to each one's own shrink capacity.
        const int deficit = flexible_preferred_total - remaining_for_flexible;
        int shrinkable_total = 0;
        for (const auto& c : children)
            if (c.policy != SizePolicy::Fixed) shrinkable_total += (c.preferred - c.min);
        if (shrinkable_total > 0) {
            const int shrink_by = std::min(deficit, shrinkable_total);
            int shrunk = 0;
            for (std::size_t i = 0; i < children.size(); ++i) {
                if (children[i].policy == SizePolicy::Fixed) continue;
                const int capacity = children[i].preferred - children[i].min;
                if (capacity <= 0) continue;
                const int amount = static_cast<int>(
                    static_cast<long long>(shrink_by) * capacity / shrinkable_total);
                assigned[i] -= amount;
                shrunk += amount;
            }
            // Integer division may leave a small remainder unshrunk;
            // apply it to the last shrinkable child, clamped at its min.
            const int leftover = shrink_by - shrunk;
            if (leftover > 0) {
                for (std::size_t i = children.size(); i-- > 0;) {
                    if (children[i].policy == SizePolicy::Fixed) continue;
                    assigned[i] = std::max(children[i].min, assigned[i] - leftover);
                    break;
                }
            }
            // Any remaining deficit beyond every child's min overflows
            // the container; it is left to painting-time clipping
            // rather than an iterative constraint solve (v1 scope).
        }
    }

    int offset = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
        const int size = std::max(0, assigned[i]);
        sizes[i] = {offset, size};
        offset += size + spacing;
    }
    return sizes;
}

std::pair<int, int> align_cross_axis(int available, int preferred, Alignment alignment,
                                      int margin_before, int margin_after) noexcept {
    const int usable = std::max(0, available - margin_before - margin_after);
    if (alignment == Alignment::Fill) return {margin_before, usable};
    const int extent = std::clamp(preferred, 0, usable);
    switch (alignment) {
        case Alignment::Start:
            return {margin_before, extent};
        case Alignment::Center:
            return {margin_before + (usable - extent) / 2, extent};
        case Alignment::End:
            return {margin_before + usable - extent, extent};
        default:
            return {margin_before, usable};
    }
}

namespace {

template <typename SelfMap>
LayoutChild child_layout(View& child, const SelfMap& specs, bool horizontal) {
    const SizeHint hint = horizontal ? child.horizontal_size_hint() : child.vertical_size_hint();
    auto it = specs.find(&child);
    const LayoutSpec spec = (it == specs.end()) ? LayoutSpec{} : it->second;
    return LayoutChild{hint.min, hint.preferred, hint.max, spec.policy, spec.weight};
}

template <typename SelfMap>
LayoutSpec spec_for(View& child, const SelfMap& specs) {
    auto it = specs.find(&child);
    return (it == specs.end()) ? LayoutSpec{} : it->second;
}

// How many columns a Column gives one child when the Column itself has
// `width` — the cross-axis question relayout answers, asked separately so
// that height_for_width measures against the width a child will actually
// be wrapped into rather than against the Column's whole width.
int column_child_width(View& child, const LayoutSpec& spec, int width) {
    const auto [cross_offset, cross_extent] = align_cross_axis(
        width, child.horizontal_size_hint().preferred, spec.alignment, spec.margin_before,
        spec.margin_after);
    (void)cross_offset;
    return cross_extent;
}

}  // namespace

View* Row::add_item(std::unique_ptr<View> child, LayoutSpec spec) {
    CKV_ASSERT(child != nullptr);
    View* observer = add_child(std::move(child));
    // An attachment callback that detached the child, or destroyed this
    // container, leaves nothing to place and no spec to keep.
    if (observer == nullptr) return nullptr;
    specs_[observer] = spec;
    relayout();
    return observer;
}

std::unique_ptr<View> Row::remove_child(View* child) {
    std::unique_ptr<View> owned = View::remove_child(child);
    if (owned == nullptr) return nullptr;
    specs_.erase(child);
    relayout();
    return owned;
}

void Row::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
}

void Row::relayout() {
    const std::vector<View*> laid_out = detail::visible_children(children());
    std::vector<LayoutChild> layout_children;
    layout_children.reserve(laid_out.size());
    for (View* child : laid_out) layout_children.push_back(child_layout(*child, specs_, true));

    const auto sizes = distribute_main_axis(layout_children, bounds().width, spacing_);
    for (std::size_t i = 0; i < laid_out.size(); ++i) {
        View& child = *laid_out[i];
        const LayoutSpec spec = spec_for(child, specs_);
        const int preferred = detail::preferred_height_for_width(child, sizes[i].second);
        const auto [cross_offset, cross_extent] = align_cross_axis(
            bounds().height, preferred, spec.alignment, spec.margin_before, spec.margin_after);
        child.set_bounds(Rect{sizes[i].first, cross_offset, sizes[i].second, cross_extent});
    }
}

int Row::height_for_width(int width) const {
    // A Row is as tall as its tallest child — but how tall a child is can
    // depend on how wide it is, and how wide it is depends on how the row
    // shares `width` out. So the share is computed first, exactly as
    // relayout computes it, and each child is then asked at the width it
    // would actually get.
    const std::vector<View*> laid_out = detail::visible_children(children());
    std::vector<LayoutChild> layout_children;
    layout_children.reserve(laid_out.size());
    for (View* child : laid_out) layout_children.push_back(child_layout(*child, specs_, true));

    const auto sizes = distribute_main_axis(layout_children, std::max(0, width), spacing_);
    int tallest = 0;
    for (std::size_t i = 0; i < laid_out.size(); ++i) {
        const LayoutSpec spec = spec_for(*laid_out[i], specs_);
        tallest = std::max(tallest, spec.margin_before + spec.margin_after +
                                        detail::preferred_height_for_width(*laid_out[i], sizes[i].second));
    }
    return tallest;
}

View* Column::add_item(std::unique_ptr<View> child, LayoutSpec spec) {
    CKV_ASSERT(child != nullptr);
    View* observer = add_child(std::move(child));
    // An attachment callback that detached the child, or destroyed this
    // container, leaves nothing to place and no spec to keep.
    if (observer == nullptr) return nullptr;
    specs_[observer] = spec;
    relayout();
    return observer;
}

std::unique_ptr<View> Column::remove_child(View* child) {
    std::unique_ptr<View> owned = View::remove_child(child);
    if (owned == nullptr) return nullptr;
    specs_.erase(child);
    relayout();
    return owned;
}

void Column::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
}

int Column::height_for_width(int width) const {
    // Wrapped children answer a different height at every width, and in a
    // Column each child's width is settled by the Column's own — so the
    // Column's height is the sum of what its children answer at the widths
    // this width gives them, plus the gaps between them. Without this a
    // Column reported the width-independent default, and a window sized
    // from it opened too short for its own wrapped text and quietly cut it
    // off at the frame.
    const std::vector<View*> laid_out = detail::visible_children(children());
    int total = 0;
    for (View* child : laid_out) {
        const LayoutSpec spec = spec_for(*child, specs_);
        total += detail::preferred_height_for_width(
            *child, column_child_width(*child, spec, std::max(0, width)));
    }
    if (!laid_out.empty()) total += spacing_ * (static_cast<int>(laid_out.size()) - 1);
    return total;
}

void Column::relayout() {
    const std::vector<View*> laid_out = detail::visible_children(children());
    std::vector<LayoutChild> layout_children;
    layout_children.reserve(laid_out.size());
    for (View* child : laid_out) {
        const LayoutSpec spec = spec_for(*child, specs_);
        const int cross_extent = column_child_width(*child, spec, bounds().width);
        const SizeHint vertical = child->vertical_size_hint();
        layout_children.push_back(
            LayoutChild{vertical.min, detail::preferred_height_for_width(*child, cross_extent),
                        vertical.max, spec.policy, spec.weight});
    }

    const auto sizes = distribute_main_axis(layout_children, bounds().height, spacing_);
    for (std::size_t i = 0; i < laid_out.size(); ++i) {
        View& child = *laid_out[i];
        const LayoutSpec spec = spec_for(child, specs_);
        const int preferred = child.horizontal_size_hint().preferred;
        const auto [cross_offset, cross_extent] = align_cross_axis(
            bounds().width, preferred, spec.alignment, spec.margin_before, spec.margin_after);
        child.set_bounds(Rect{cross_offset, sizes[i].first, cross_extent, sizes[i].second});
    }
}

namespace {

// Main axis: children's hints sum (plus spacing between consecutive
// children); cross axis: the largest over children. The aggregate max is
// always unbounded: a container given more room than its children can use
// leaves the rest empty rather than refusing it.
SizeHint sum_hints(const std::vector<std::unique_ptr<View>>& children, bool horizontal, int spacing) {
    SizeHint total{0, 0, 0};
    const std::vector<View*> counted = detail::visible_children(children);
    for (View* child : counted) {
        const SizeHint h = horizontal ? child->horizontal_size_hint() : child->vertical_size_hint();
        total.min += h.min;
        total.preferred += h.preferred;
    }
    const int gaps = counted.empty() ? 0 : spacing * (static_cast<int>(counted.size()) - 1);
    total.min += gaps;
    total.preferred += gaps;
    total.max = kUnboundedExtent;
    return total;
}

// A child's cross-axis margins are part of what it needs across, exactly as
// relayout reserves them, so a container never measures narrower than the
// room it will then give its widest child.
template <typename SelfMap>
SizeHint max_hints(const std::vector<std::unique_ptr<View>>& children, const SelfMap& specs, bool horizontal) {
    SizeHint total{0, 0, kUnboundedExtent};
    for (View* child : detail::visible_children(children)) {
        const SizeHint h = horizontal ? child->horizontal_size_hint() : child->vertical_size_hint();
        const LayoutSpec spec = spec_for(*child, specs);
        const int margins = spec.margin_before + spec.margin_after;
        total.min = std::max(total.min, h.min + margins);
        total.preferred = std::max(total.preferred, h.preferred + margins);
    }
    return total;
}

}  // namespace

// Asked of the children the row lays out: a hidden one puts nothing on the
// row's last line, so it has no say in what is there.
bool Row::trailing_row_is_shadow() const noexcept {
    const std::vector<View*> shown = detail::visible_children(children());
    if (shown.empty()) return false;
    for (const View* child : shown)
        if (!child->trailing_row_is_shadow()) return false;
    return true;
}

SizeHint Row::horizontal_size_hint() const { return sum_hints(children(), true, spacing_); }
SizeHint Row::vertical_size_hint() const { return max_hints(children(), specs_, false); }
bool Column::trailing_row_is_shadow() const noexcept {
    const std::vector<View*> shown = detail::visible_children(children());
    return !shown.empty() && shown.back()->trailing_row_is_shadow();
}

SizeHint Column::horizontal_size_hint() const { return max_hints(children(), specs_, true); }
SizeHint Column::vertical_size_hint() const { return sum_hints(children(), false, spacing_); }

}  // namespace ckv::ui
