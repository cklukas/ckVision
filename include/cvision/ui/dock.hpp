// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Dock: the edges-plus-center container from D-006's remaining set
// (M10/WP-19) — the classic border layout (Top/Bottom/Left/Right
// strips around a filling Center), and the general-purpose primitive
// behind "a sidebar next to a main content area." A STANDALONE ui::
// container: it does not touch, wrap, or restructure Desktop's own
// existing top_dock_/bottom_dock_ (widgets::Desktop's docked status/
// menu-bar mechanism serves a narrower, already-shipped purpose and is
// left exactly as it is).
//
// Top/Bottom edge heights use the edge child's height_for_width(dock width),
// so wrapped content can reserve its natural strip height after the dock width
// is known. At most one child per edge (including Center) — a caller wanting
// several views on the same edge nests a Row/Column there, the same
// "compose smaller primitives" answer Row/Column give for anything
// beyond their own scope. Reservation order is fixed and canonical
// (Top, then Bottom, then Left, then Right, Center last): Top/Bottom
// each claim a full-width strip first, so Left/Right only ever span
// the vertical space left between them, never the whole height — the
// same order every classic border-layout implementation uses.
#pragma once

#include <optional>
#include <unordered_map>

#include "cvision/ui/view.hpp"

namespace ckv::ui {

enum class DockEdge { Top, Bottom, Left, Right, Center };

// Reserves each present edge's own strip from `available` (local,
// starting at {0, 0}) in the canonical Top/Bottom/Left/Right order,
// clamping each edge's requested extent to whatever space remains so
// far; `edges` fields left as nullopt mean "no child on that edge" —
// the reservation step for that edge is skipped entirely. Whatever
// remains after all four goes to `center`. Pure function, no View
// dependency, exhaustively testable in isolation — the Dock analogue
// of distribute_main_axis.
struct DockEdgeExtents {
    std::optional<int> top;     // preferred height
    std::optional<int> bottom;  // preferred height
    std::optional<int> left;    // preferred width
    std::optional<int> right;   // preferred width
};

struct DockRects {
    Rect top, bottom, left, right, center;
};

DockRects compute_dock_layout(Rect available, DockEdgeExtents edges) noexcept;

class Dock : public View {
public:
    using View::View;

    // `edge`'s own preferred size hint along its perpendicular axis
    // (height for Top/Bottom, width for Left/Right) is honored,
    // clamped to fit the space earlier edges left; Center fills
    // whatever space is left after all four. CKV_ASSERTs if `edge`
    // already has a child — remove_item first to replace one.
    View* add_item(std::unique_ptr<View> child, DockEdge edge);
    std::unique_ptr<View> remove_item(View* child);

    void on_resized() override { relayout(); }
    // See Row/Column/Grid: a child's own hint changing (M9/WP-16)
    // needs the same response as this Dock's own bounds changing.
    void on_child_size_hint_changed(View&) override { relayout(); }

private:
    void relayout();

    std::unordered_map<View*, DockEdge> specs_;
};

}  // namespace ckv::ui
