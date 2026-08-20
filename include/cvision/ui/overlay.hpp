// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Overlay: the layered-composition container from D-006's remaining
// set (M10/WP-19) — a stack of children sharing the same footprint
// (a background layer, content above it, a toast/tooltip layer above
// that). Z-order is NOT a separate concept Overlay invents: it is
// exactly View::children()' own list order, the same order every
// paint_children()/topmost_view_at() walk already honors (later
// children on top) — a later layer is added later, and an existing
// one is restacked with the already-public View::raise_to_front(),
// with no Overlay-specific wrapper needed for either.
//
// Each child is either Fill (resized to the overlay's own full extent
// on every insertion and every later resize — the common case: a
// layer that covers the whole stack) or Manual (keeps whatever bounds
// it was given, completely untouched by the overlay's own bounds
// changing — a small positioned badge over otherwise-Fill layers
// below it). Deliberately NOT overriding on_child_size_hint_changed
// (unlike Row/Column/Grid/Dock): a Fill child's placement is always
// exactly the overlay's own bounds regardless of the child's own
// hint, and a Manual child's bounds are entirely caller-controlled —
// neither depends on a child's size hint, so there is nothing here to
// react to when one changes.
#pragma once

#include <unordered_map>

#include "cvision/ui/view.hpp"

namespace ckv::ui {

enum class OverlayMode { Fill, Manual };

class Overlay : public View {
public:
    using View::View;

    View* add_item(std::unique_ptr<View> child, OverlayMode mode = OverlayMode::Fill);
    std::unique_ptr<View> remove_item(View* child);

    void on_resized() override { relayout(); }

private:
    void relayout();

    std::unordered_map<View*, OverlayMode> specs_;
};

}  // namespace ckv::ui
