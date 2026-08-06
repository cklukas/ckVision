// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/ui/layout.hpp"

#include <algorithm>

#include "cvision/core/assert.hpp"

namespace ckv::ui {

std::vector<std::pair<int, int>> distribute_main_axis(const std::vector<LayoutChild>& children,
                                                        int available, int spacing) {
    std::vector<std::pair<int, int>> sizes(children.size(), {0, 0});
    if (children.empty()) return sizes;

    const int spacing_total = spacing * static_cast<int>(children.size() - 1);
    const int usable = std::max(0, available - spacing_total);

    int fixed_total = 0;
    int flexible_preferred_total = 0;
    int expanding_weight_total = 0;
    for (const auto& c : children) {
        if (c.policy == SizePolicy::Fixed) {
            fixed_total += c.preferred;
        } else {
            flexible_preferred_total += c.preferred;
            if (c.policy == SizePolicy::Expanding) expanding_weight_total += c.weight;
        }
    }

    const int remaining_for_flexible = usable - fixed_total;

    std::vector<int> assigned(children.size(), 0);
    for (std::size_t i = 0; i < children.size(); ++i) assigned[i] = children[i].preferred;

    if (remaining_for_flexible >= flexible_preferred_total) {
        // Slack to hand out: only Expanding children claim a share,
        // proportional to weight. Minimum children stay at preferred.
        const int extra = remaining_for_flexible - flexible_preferred_total;
        if (expanding_weight_total > 0 && extra > 0) {
            int distributed = 0;
            for (std::size_t i = 0; i < children.size(); ++i) {
                if (children[i].policy != SizePolicy::Expanding) continue;
                const int share = static_cast<int>(
                    static_cast<long long>(extra) * children[i].weight / expanding_weight_total);
                assigned[i] += share;
                distributed += share;
            }
            // Integer division leaves a remainder; hand it to the last
            // Expanding child so the container is filled exactly.
            const int leftover = extra - distributed;
            if (leftover > 0) {
                for (std::size_t i = children.size(); i-- > 0;) {
                    if (children[i].policy == SizePolicy::Expanding) {
                        assigned[i] += leftover;
                        break;
                    }
                }
            }
        }
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

namespace {

template <typename SelfMap>
LayoutChild child_layout(View& child, const SelfMap& specs, bool horizontal) {
    const SizeHint hint = horizontal ? child.horizontal_size_hint() : child.vertical_size_hint();
    auto it = specs.find(&child);
    const LayoutSpec spec = (it == specs.end()) ? LayoutSpec{} : it->second;
    return LayoutChild{hint.min, hint.preferred, spec.policy, spec.weight};
}

}  // namespace

View* Row::add_item(std::unique_ptr<View> child, LayoutSpec spec) {
    CKV_ASSERT(child != nullptr);
    View* observer = add_child(std::move(child));
    specs_[observer] = spec;
    relayout();
    return observer;
}

std::unique_ptr<View> Row::remove_item(View* child) {
    std::unique_ptr<View> owned = remove_child(child);
    if (owned) {
        specs_.erase(child);
        relayout();
    }
    return owned;
}

void Row::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
}

void Row::relayout() {
    std::vector<LayoutChild> layout_children;
    layout_children.reserve(children().size());
    for (const auto& child : children()) layout_children.push_back(child_layout(*child, specs_, true));

    const auto sizes = distribute_main_axis(layout_children, bounds().width, spacing_);
    for (std::size_t i = 0; i < children().size(); ++i) {
        View& child = *children()[i];
        child.set_bounds(Rect{sizes[i].first, 0, sizes[i].second, bounds().height});
    }
}

View* Column::add_item(std::unique_ptr<View> child, LayoutSpec spec) {
    CKV_ASSERT(child != nullptr);
    View* observer = add_child(std::move(child));
    specs_[observer] = spec;
    relayout();
    return observer;
}

std::unique_ptr<View> Column::remove_item(View* child) {
    std::unique_ptr<View> owned = remove_child(child);
    if (owned) {
        specs_.erase(child);
        relayout();
    }
    return owned;
}

void Column::set_spacing(int spacing) {
    CKV_ASSERT(spacing >= 0);
    if (spacing == spacing_) return;
    spacing_ = spacing;
    relayout();
}

void Column::relayout() {
    std::vector<LayoutChild> layout_children;
    layout_children.reserve(children().size());
    for (const auto& child : children()) layout_children.push_back(child_layout(*child, specs_, false));

    const auto sizes = distribute_main_axis(layout_children, bounds().height, spacing_);
    for (std::size_t i = 0; i < children().size(); ++i) {
        View& child = *children()[i];
        child.set_bounds(Rect{0, sizes[i].first, bounds().width, sizes[i].second});
    }
}

}  // namespace ckv::ui
