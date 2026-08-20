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

struct Anchors {
    bool left = false;
    bool top = false;
    bool right = false;
    bool bottom = false;

    friend bool operator==(const Anchors&, const Anchors&) = default;
};

// Applies `anchors` to `current` given how much the pane's own size
// changed since the last pass (`delta_width`/`delta_height`, either
// sign). Pure function, no View dependency, so it is exhaustively
// testable in isolation — the same shape as distribute_main_axis.
Rect apply_anchors(Rect current, Anchors anchors, int delta_width, int delta_height) noexcept;

class AnchorPane : public View {
public:
    using View::View;

    // `child` keeps whatever bounds it's given (its own, explicit
    // placement — AnchorPane never positions a child itself, only
    // repositions/resizes it on a later pane resize per `anchors`).
    View* add_item(std::unique_ptr<View> child, Anchors anchors = {});
    std::unique_ptr<View> remove_item(View* child);

    void on_resized() override;

private:
    std::unordered_map<View*, Anchors> specs_;
    Size last_size_{bounds().width, bounds().height};
};

}  // namespace ckv::ui
