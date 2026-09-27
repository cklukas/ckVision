// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// AnchorPane: the modern equivalent of the classic per-view grow flags
// (M10/WP-18) — a container for children placed at their own explicit
// bounds (not flowed like Row/Column), each keeping a fixed distance
// to whichever edges it's anchored to as the pane itself resizes.
// Anchoring OPPOSITE edges (left+right, or top+bottom) makes a child
// stretch to preserve both distances; anchoring only one edge on an
// axis keeps the child's own size on that axis and preserves that
// edge's distance; anchoring neither leaves the child's bounds on
// that axis untouched by the pane's resize entirely (the same
// resulting geometry as anchoring only the near edge — left/top are
// the implicit default every plain Rect placement already behaves
// like, so Anchors{} needs no special case to match it).
#pragma once

#include <unordered_map>

#include "cvision/ui/view.hpp"

namespace ckv::ui {

// Which of the pane's edges an AnchorPane child keeps a fixed distance to when the pane resizes.
// The default (nothing anchored) behaves exactly like anchoring left and top.
struct Anchors {
    // One flag per pane edge. Left with right stretches the child's width by the pane's width
    // change, right alone moves it; top and bottom do the same vertically.
    bool left = false;
    bool top = false;
    bool right = false;
    bool bottom = false;

    // Memberwise equality.
    friend bool operator==(const Anchors&, const Anchors&) = default;
};

// Applies `anchors` to `current` given how much the pane's own size
// changed since the last pass (`delta_width`/`delta_height`, either
// sign). Pure function, no View dependency, so it is exhaustively
// testable in isolation — the same shape as distribute_main_axis.
// The result is not clamped: a stretched child squeezed past zero keeps
// a negative width or height, so growing the pane again restores its
// original geometry exactly.
Rect apply_anchors(Rect current, Anchors anchors, int delta_width, int delta_height) noexcept;

// A container whose children sit at their own explicit bounds and follow the pane's resizes by
// their Anchors. The pane remembers its size from construction and from each resize, and applies
// the difference to every child (children added with add_child() count as unanchored).
class AnchorPane : public View {
public:
    // Same constructor as View; the initial `bounds` size is the baseline the first resize is
    // measured against.
    using View::View;

    // `child` keeps whatever bounds it's given (its own, explicit
    // placement — AnchorPane never positions a child itself, only
    // repositions/resizes it on a later pane resize per `anchors`).
    View* add_item(std::unique_ptr<View> child, Anchors anchors = {});
    // Detaches `child` exactly as View::remove_child() does, then forgets its anchors and
    // returns ownership; nullptr when `child` is not a child of this pane. The other children
    // are not moved. Every way a child leaves comes through here, so a view added back with
    // add_child() is unanchored again.
    std::unique_ptr<View> remove_child(View* child) override;

    void on_resized() override;

private:
    std::unordered_map<View*, Anchors> specs_;
    Size last_size_{bounds().width, bounds().height};
};

}  // namespace ckv::ui
